#include "internet.hpp"
#include "network.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
using namespace cl;
static void check(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(name);
    std::cout << "PASS " << name << std::endl;
}
static void p16(QByteArray &b, int p, quint16 x) {
    b[p] = char(x >> 8);
    b[p + 1] = char(x);
}
static void p32(QByteArray &b, int p, quint32 x) {
    p16(b, p, quint16(x >> 16));
    p16(b, p + 2, quint16(x));
}
static quint16 checksum(QByteArray b) {
    if (b.size() % 2)
        b.append('\0');
    quint32 n = 0;
    for (int i = 0; i < b.size(); i += 2)
        n += be16(b, i);
    while (n >> 16)
        n = (n & 65535) + (n >> 16);
    return quint16(~n);
}
static QByteArray ipPacket(quint32 source, quint32 dest, int proto, QByteArray payload) {
    QByteArray b(20, 0);
    b[0] = 0x45;
    b[8] = 64;
    b[9] = char(proto);
    p16(b, 2, quint16(20 + payload.size()));
    p32(b, 12, source);
    p32(b, 16, dest);
    if (proto == 6 || proto == 17) {
        auto pseudo = b.mid(12, 8) + QByteArray(1, 0) + QByteArray(1, char(proto)) + QByteArray(2, 0);
        p16(pseudo, 10, quint16(payload.size()));
        p16(payload, proto == 6 ? 16 : 6, checksum(pseudo + payload));
    }
    p16(b, 10, checksum(b));
    return b + payload;
}
struct TestSocket {
    SOCKET s;
    explicit TestSocket(int type)
        : s(socket(AF_INET, type, type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP)) {
        if (s == INVALID_SOCKET)
            throw std::runtime_error("socket");
        DWORD timeout = 4000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
    }
    ~TestSocket() {
        closesocket(s);
    }
    quint16 bindLoopback() {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)))
            throw std::runtime_error("bind");
        int n = sizeof(a);
        getsockname(s, reinterpret_cast<sockaddr *>(&a), &n);
        return ntohs(a.sin_port);
    }
};
struct Guest {
    AccessPlan plan{0x0afdf600, {{0x0afdf602, "02:00:00:00:00:02", "synthetic"}}};
    InternetGateway nat;
    std::deque<QByteArray> replies;
    quint32 seq = 10000, ack = 0;
    quint16 port = 0, local = 40000;
    quint32 remote = 0x7f000001;
    QByteArray bytes;
    void open(bool loopback = true) {
        if (loopback)
            nat.allowLoopbackForTest();
        check(nat.open(plan,
                       [&](const AccessEthernet &f) {
                           if (f.adapter != "synthetic")
                               return false;
                           replies.push_back(f.bytes);
                           return true;
                       }),
              qPrintable(nat.error.isEmpty() ? QString("userspace NAT loads with isolated DLL dependencies")
                                             : nat.error));
        QByteArray arp(60, 0);
        arp.replace(0, 6, QByteArray::fromHex("ffffffffffff"));
        arp.replace(6, 6, QByteArray::fromHex(plan.targets[0].mac.toLatin1()));
        p16(arp, 12, 0x0806);
        p16(arp, 14, 1);
        p16(arp, 16, 0x0800);
        arp[18] = 6;
        arp[19] = 4;
        p16(arp, 20, 1);
        arp.replace(22, 6, arp.mid(6, 6));
        p32(arp, 28, plan.targets[0].ip);
        p32(arp, 38, plan.pcIp());
        check(nat.input("synthetic", arp), "authorized console ARP enters NAT");
        check(!replies.empty() && inspect(replies.front())->source == AccessMac &&
                  inspect(replies.front())->sourceIp == plan.pcIp(),
              "gateway ARP shares the local-access MAC");
        replies.clear();
    }
    void sendIp(const QByteArray &ip) {
        auto frame = QByteArray::fromHex(AccessMac.toLatin1()) +
                     QByteArray::fromHex(plan.targets[0].mac.toLatin1()) + QByteArray::fromHex("0800") + ip;
        if (!nat.input("synthetic", frame))
            throw std::runtime_error("input policy");
    }
    void tcp(quint8 flags, QByteArray data = {}) {
        QByteArray t(20, 0);
        p16(t, 0, local);
        p16(t, 2, port);
        p32(t, 4, seq);
        p32(t, 8, ack);
        t[12] = 0x50;
        t[13] = char(flags);
        p16(t, 14, 65535);
        t += data;
        sendIp(ipPacket(plan.targets[0].ip, remote, 6, t));
        seq += quint32(data.size()) + ((flags & 3) ? 1 : 0);
    }
    template <class F> bool until(F predicate, int ms = 5000) {
        auto end = clockMs() + ms;
        while (clockMs() < end) {
            nat.poll();
            if (predicate())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }
    void receiveTcp() {
        while (!replies.empty()) {
            auto frame = replies.front();
            replies.pop_front();
            auto f = inspect(frame);
            if (!f || f->arp || quint8(frame[23]) != 6)
                continue;
            int h = (quint8(frame[14]) & 15) * 4;
            auto ip = frame.mid(14, be16(frame, 16));
            if (be16(ip, h) != port || be16(ip, h + 2) != local)
                continue;
            if (checksum(ip.left(h)) != 0)
                throw std::runtime_error("IP checksum");
            auto pseudo = ip.mid(12, 8) + QByteArray::fromHex("0006") + QByteArray(2, 0);
            p16(pseudo, 10, quint16(ip.size() - h));
            if (checksum(pseudo + ip.mid(h)) != 0)
                throw std::runtime_error("TCP checksum");
            auto flags = quint8(ip[h + 13]);
            auto s = be32(ip, h + 4);
            int th = (quint8(ip[h + 12]) >> 4) * 4;
            auto data = ip.mid(h + th);
            if (flags & 4)
                throw std::runtime_error("unexpected TCP reset");
            if (flags & 2) {
                ack = s + 1;
                tcp(16);
            } else if (s == ack && (!data.isEmpty() || (flags & 1))) {
                bytes += data;
                ack += quint32(data.size()) + ((flags & 1) ? 1 : 0);
                tcp(16);
            }
        }
    }
    void connectTcp(quint32 ip, quint16 p) {
        remote = ip;
        port = p;
        ack = 0;
        bytes.clear();
        tcp(2);
        check(until([&] {
                  receiveTcp();
                  return ack != 0;
              }),
              "console TCP handshake translated to a real host socket");
    }
    void udp(quint32 dest, quint16 p, QByteArray data) {
        QByteArray t(8, 0);
        p16(t, 0, local);
        p16(t, 2, p);
        p16(t, 4, quint16(data.size() + 8));
        sendIp(ipPacket(plan.targets[0].ip, dest, 17, t + data));
    }
    std::optional<QByteArray> receiveUdp(quint16 p) {
        while (!replies.empty()) {
            auto frame = replies.front();
            replies.pop_front();
            auto f = inspect(frame);
            if (f && !f->arp && quint8(frame[23]) == 17) {
                int h = (quint8(frame[14]) & 15) * 4;
                if (be16(frame, 14 + h) == p && be16(frame, 16 + h) == local)
                    return frame.mid(22 + h, be16(frame, 18 + h) - 8);
            }
        }
        return {};
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa))
        return 1;
    try {
        if (!app.arguments().contains("--external-only")) {
            Guest guest;
            guest.open();
            QByteArray payload;
            for (int i = 0; i < 16384; ++i)
                payload.append(char((i * 19) % 256));
            TestSocket udp(SOCK_DGRAM);
            auto udpPort = udp.bindLoopback();
            auto echo = std::async(std::launch::async, [&] {
                char b[1024];
                sockaddr_in peer{};
                int len = sizeof(peer);
                int n = recvfrom(udp.s, b, sizeof(b), 0, reinterpret_cast<sockaddr *>(&peer), &len);
                return n > 0 && sendto(udp.s, b, n, 0, reinterpret_cast<sockaddr *>(&peer), len) == n;
            });
            guest.udp(guest.plan.pcIp(), udpPort, payload.left(512));
            std::optional<QByteArray> response;
            check(guest.until([&] {
                response = guest.receiveUdp(udpPort);
                return response.has_value();
            }),
                  "first UDP packet after open gets a real response at the correct console");
            check(echo.get() && *response == payload.left(512), "UDP payload is byte-exact");
            TestSocket listener(SOCK_STREAM);
            auto port = listener.bindLoopback();
            check(listen(listener.s, 1) == 0, "controlled TCP echo service");
            auto server = std::async(std::launch::async, [&] {
                WSAPOLLFD pending{listener.s, POLLRDNORM, 0};
                if (WSAPoll(&pending, 1, 5000) <= 0)
                    return false;
                auto peer = accept(listener.s, nullptr, nullptr);
                if (peer == INVALID_SOCKET)
                    return false;
                DWORD ms = 5000;
                setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&ms), sizeof(ms));
                QByteArray got;
                char b[2048];
                while (got.size() < payload.size()) {
                    int n = recv(peer, b, sizeof(b), 0);
                    if (n <= 0)
                        break;
                    got.append(b, n);
                    int sent = 0;
                    while (sent < n) {
                        int c = send(peer, b + sent, n - sent, 0);
                        if (c <= 0)
                            break;
                        sent += c;
                    }
                }
                shutdown(peer, SD_SEND);
                closesocket(peer);
                return got == payload;
            });
            guest.connectTcp(guest.plan.pcIp(), port);
            for (int offset = 0; offset < payload.size(); offset += 700) {
                auto chunk = payload.mid(offset, 700);
                guest.tcp(24, chunk);
                check(guest.until([&] {
                    guest.receiveTcp();
                    return guest.bytes.size() >= offset + chunk.size();
                }),
                      "binary TCP data crosses NAT in both directions");
            }
            check(server.get() && guest.bytes == payload, "16 KiB TCP payload is byte-exact");
            check(guest.nat.sent() > 20 && guest.nat.received() > 20,
                  "Internet counters reflect actual packets");
            guest.nat.close();
            check(!guest.nat.active() && !guest.nat.input("synthetic", QByteArray(60, 0)),
                  "disable stops the stack and rejects further packets");
        }
        Guest external;
        external.open(false);
        std::optional<QByteArray> response;
        if (app.arguments().contains("--external") || app.arguments().contains("--external-only")) {
            // Read-only public endpoint; optional because the regular suite is offline-capable.
            auto query = QByteArray::fromHex("123401000001000000000000076578616d706c6503636f6d0000010001");
            external.udp(0x08080808, 53, query);
            check(external.until(
                      [&] {
                          response = external.receiveUdp(53);
                          return response.has_value();
                      },
                      10000),
                  "public DNS through UDP NAT");
            external.udp(external.plan.pcIp(), 53, query);
            check(external.until(
                      [&] {
                          response = external.receiveUdp(53);
                          return response.has_value();
                      },
                      10000),
                  "gateway DNS proxy reaches the PC resolver");
            check(response->size() >= 12 && be16(*response, 0) == 0x1234 && (quint8((*response)[2]) & 0x80) &&
                      (quint8((*response)[3]) & 15) == 0 && be16(*response, 6) > 0,
                  "real DNS query has a successful answer");
            external.connectTcp(0x01010101, 80);
            external.tcp(24, "GET /cdn-cgi/trace HTTP/1.0\r\nHost: one.one.one.one\r\n\r\n");
            check(external.until(
                      [&] {
                          external.receiveTcp();
                          return external.bytes.contains("\r\n\r\n");
                      },
                      10000) &&
                      external.bytes.startsWith("HTTP/1."),
                  "console HTTP request reaches public Internet through NAT");
            external.connectTcp(external.plan.pcIp(), 53);
            QByteArray dnsLength(2, 0);
            p16(dnsLength, 0, quint16(query.size()));
            external.tcp(24, dnsLength + query);
            check(external.until(
                      [&] {
                          external.receiveTcp();
                          return external.bytes.size() >= 2 &&
                                 external.bytes.size() >= be16(external.bytes, 0) + 2;
                      },
                      10000) &&
                      be16(external.bytes, 2) == 0x1234 && (quint8(external.bytes[5]) & 15) == 0,
                  "gateway DNS also resolves over a real TCP connection");
        }
        std::cout << "SUCCESS: Internet gateway tests\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << std::endl;
        WSACleanup();
        return 1;
    }
    WSACleanup();
    return 0;
}
