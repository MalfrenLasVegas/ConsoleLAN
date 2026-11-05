#include "network.hpp"
#include "capture.hpp"
#include <iphlpapi.h>
#include <iostream>
using namespace cl;
static Adapter loopback() {
    Adapter a;
    a.id = "loopback";
    a.ip = 0x7f000001;
    a.up = true;
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(a.ip);
    DWORD index = 0;
    GetBestInterfaceEx(reinterpret_cast<sockaddr *>(&dst), &index);
    a.index = index;
    return a;
}
static QByteArray frame(const Console &src, const Console &dst, bool broadcast) {
    QByteArray bytes(broadcast ? 60 : 1442, 0);
    bytes.replace(0, 6, QByteArray::fromHex((broadcast ? QString("ff:ff:ff:ff:ff:ff") : dst.mac).toLatin1()));
    bytes.replace(6, 6, QByteArray::fromHex(src.mac.toLatin1()));
    auto u16 = [&](int p, quint16 n) {
        bytes[p] = char(n >> 8);
        bytes[p + 1] = char(n);
    };
    auto u32 = [&](int p, quint32 n) {
        u16(p, quint16(n >> 16));
        u16(p + 2, quint16(n));
    };
    if (broadcast) {
        u16(12, 0x0806);
        u16(14, 1);
        u16(16, 0x0800);
        bytes[18] = 6;
        bytes[19] = 4;
        u16(20, 1);
        bytes.replace(22, 6, bytes.mid(6, 6));
        u32(28, src.ip);
        u32(38, dst.ip);
    } else {
        u16(12, 0x0800);
        bytes[14] = 0x45;
        u16(16, 1428);
        bytes[22] = 64;
        bytes[23] = 17;
        u32(26, src.ip);
        u32(30, dst.ip);
        u16(34, 1000);
        u16(36, 1001);
        u16(38, 1408);
    }
    return bytes;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    UdpSocket startup;
    auto args = app.arguments();
    if (args.contains("--probe-npcap")) {
        std::cout << Capture::version().toStdString() << "\n";
        int opened = 0;
        for (auto &a : adapters())
            if (a.physical && a.up) {
                Capture c;
                if (c.open(a.id)) {
                    ++opened;
                    std::cout << "OPEN PROMISCUOUS OK: " << a.name.toStdString() << "\n";
                } else {
                    std::cerr << c.error.toStdString() << "\n";
                    return 1;
                }
            }
        return opened ? 0 : 2;
    }
    UdpSocket one, two, three;
    auto adapter = loopback();
    if (!one.open(adapter, 0) || !two.open(adapter, 0) || !three.open(adapter, 0)) {
        std::cerr << one.error.toStdString() << two.error.toStdString() << three.error.toStdString();
        return 1;
    }
    Room host, a, b;
    host.addConsole("H", "eth-h");
    a.addConsole("A", "eth-a");
    b.addConsole("B", "eth-b");
    qint64 begin = clockMs();
    host.host("Winsock", 0x0afd2a00, begin);
    a.join(one.bound, begin);
    b.join(one.bound, begin);
    host.setBudget(512);
    a.setBudget(512);
    b.setBudget(512);
    qint64 lastSend = 0;
    int deliveredA = 0, deliveredB = 0, unexpectedHost = 0, attempted = 0;
    QByteArray replay;
    while (clockMs() - begin < 6000) {
        auto now = clockMs();
        if (now - lastSend >= 20) {
            lastSend = now;
            auto cs = host.consoles();
            if (cs.size() == 3) {
                Console ca, cb, ch;
                for (auto c : cs) {
                    if (c.name == "A")
                        ca = c;
                    else if (c.name == "B")
                        cb = c;
                    else
                        ch = c;
                }
                ca.mac = "02:00:00:00:00:02";
                cb.mac = "02:00:00:00:00:03";
                ch.mac = "02:00:00:00:00:01";
                if (now - begin < 3000) {
                    a.captured("eth-a", frame(ca, cb, true), now);
                    b.captured("eth-b", frame(cb, ca, true), now);
                    host.captured("eth-h", frame(ch, ca, true), now);
                } else if (now - begin < 5000) {
                    ++attempted;
                    a.captured("eth-a", frame(ca, cb, false), now);
                    b.captured("eth-b", frame(cb, ca, false), now);
                }
            }
        }
        for (auto p : {std::pair{&host, &one}, std::pair{&a, &two}, std::pair{&b, &three}}) {
            p.first->tick(now);
            for (auto &d : p.first->outbound) {
                if (p.first == &a) {
                    auto decoded = decode(d.bytes);
                    if (decoded && decoded->h.type == Type::Frame)
                        replay = d.bytes;
                }
                if (!p.second->send(d)) {
                    std::cerr << p.second->error.toStdString();
                    return 1;
                }
            }
            p.first->outbound.clear();
            for (int i = 0; i < 128; ++i) {
                auto d = p.second->receive();
                if (!d)
                    break;
                p.first->receive(d->endpoint, d->bytes, now);
            }
            for (auto &injection : p.first->injections)
                if (be16(injection.bytes, 12) == 0x0800) {
                    if (p.first == &a)
                        ++deliveredA;
                    else if (p.first == &b)
                        ++deliveredB;
                    else
                        ++unexpectedHost;
                }
            p.first->injections.clear();
        }
        QThread::msleep(2);
    }
    if (!a.connected() || !b.connected() || host.view()["members"].toArray().size() != 3 ||
        a.consoles().size() != 3) {
        std::cerr << "Winsock 3-node handshake failed";
        return 1;
    }
    // A competing process/socket cannot silently share this endpoint.
    UdpSocket duplicate;
    if (duplicate.open(adapter, one.bound.port)) {
        std::cerr << "Exclusive binding failed";
        return 1;
    }
    if (attempted < 30 || deliveredA != attempted || deliveredB != attempted || unexpectedHost) {
        std::cerr << "Unicast failed A=" << deliveredA << " B=" << deliveredB << " expected=" << attempted
                  << " unexpected=" << unexpectedHost;
        return 1;
    }
    UdpSocket rogue;
    if (!rogue.open(adapter, 0))
        return 1;
    rogue.send({one.bound, replay});
    auto previous = host.invalid;
    auto wait = clockMs();
    while (clockMs() - wait < 100) {
        auto d = one.receive();
        if (d)
            host.receive(d->endpoint, d->bytes, clockMs());
        QThread::msleep(1);
    }
    if (host.invalid <= previous) {
        std::cerr << "endpoint spoof accepted";
        return 1;
    }
    std::cout << "PASS real Winsock: three sockets, challenge, authoritative reservations, heartbeat, "
                 "exclusive bind, endpoint replay rejection\nPASS fragmented unicast A->master->B: "
              << deliveredB << " frames; B->master->A: " << deliveredA
              << " frames; host injections: " << unexpectedHost << "\n";
    return 0;
}
