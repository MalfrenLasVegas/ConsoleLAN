#include "access_native.hpp"
#include "access_pump.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
using namespace cl;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
static void put16(QByteArray &b, int p, quint16 n) {
    b[p] = char(n >> 8);
    b[p + 1] = char(n);
}
static void put32(QByteArray &b, int p, quint32 n) {
    put16(b, p, quint16(n >> 16));
    put16(b, p + 2, quint16(n));
}
static quint16 checksum(QByteArray b) {
    if (b.size() % 2)
        b.append('\0');
    quint32 sum = 0;
    for (int i = 0; i < b.size(); i += 2)
        sum += be16(b, i);
    while (sum >> 16)
        sum = (sum & 65535) + (sum >> 16);
    return quint16(~sum);
}
static QByteArray tcp(quint32 src, quint32 dst, quint16 sport, quint16 dport, quint32 seq, quint32 ack,
                      quint8 flags, const QByteArray &data = {}) {
    const auto options = (flags & 2) ? QByteArray::fromHex("020405b401030307") : QByteArray{};
    QByteArray b(40, 0);
    b[0] = 0x45;
    b[8] = 64;
    b[9] = 6;
    put16(b, 2, quint16(40 + options.size() + data.size()));
    put32(b, 12, src);
    put32(b, 16, dst);
    put16(b, 20, sport);
    put16(b, 22, dport);
    put32(b, 24, seq);
    put32(b, 28, ack);
    b[32] = char((20 + options.size()) / 4 << 4);
    b[33] = char(flags);
    put16(b, 34, 65535);
    b += options;
    b += data;
    auto pseudo = b.mid(12, 8) + QByteArray::fromHex("0006") + QByteArray(2, 0);
    put16(pseudo, 10, quint16(b.size() - 20));
    put16(b, 36, checksum(pseudo + b.mid(20)));
    put16(b, 10, checksum(b.left(20)));
    return b;
}
static bool elevated() {
    HANDLE token = nullptr;
    TOKEN_ELEVATION e{};
    DWORD size = sizeof(e);
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    auto ok = GetTokenInformation(token, TokenElevation, &e, size, &size);
    CloseHandle(token);
    return ok && e.TokenIsElevated;
}
static QStringList addresses() {
    QStringList result;
    for (const auto &a : adapters())
        if (a.name != "ConsoleLAN Test")
            result << a.id + "|" + ipText(a.ip) + "/" + QString::number(a.prefix);
    result.sort();
    return result;
}
struct Socket {
    SOCKET value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Socket() {
        require(value != INVALID_SOCKET, "create client TCP socket");
        DWORD timeout = 15000;
        setsockopt(value, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
        setsockopt(value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
    }
    ~Socket() {
        closesocket(value);
    }
};
// Synthetic Ethernet console, real Windows TCP on the PC side. Negotiates
// window scaling and acknowledges actual bytes; no host file or console writes.
struct Peer {
    const AccessPlan &plan;
    const QByteArray &payload;
    std::deque<QByteArray> queue;
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    quint16 clientPort = 0;
    quint32 clientNext = 0, serverNext = 10000, serverAcked = 10000, window = 65535;
    int windowShift = 0, uploaded = 0, downloaded = 0;
    bool connected = false, finished = false;
    qint64 uploadStart = 0, uploadEnd = 0, downloadStart = 0;
    Peer(const AccessPlan &p, const QByteArray &data) : plan(p), payload(data) {
        require(ready, "create packet event");
    }
    ~Peer() {
        CloseHandle(ready);
    }
    void output(quint8 flags, const QByteArray &data = {}) {
        auto packet =
            tcp(plan.targets[0].ip, plan.pcIp(), 45678, clientPort, serverNext, clientNext, flags, data);
        auto frame = QByteArray::fromHex(AccessMac.toLatin1()) +
                     QByteArray::fromHex(plan.targets[0].mac.toLatin1()) + QByteArray::fromHex("0800") +
                     packet;
        queue.push_back(frame);
        SetEvent(ready);
        serverNext += quint32(data.size()) + ((flags & 3) ? 1 : 0);
    }
    std::optional<QByteArray> receive() {
        if (queue.empty()) {
            ResetEvent(ready);
            return {};
        }
        auto packet = std::move(queue.front());
        queue.pop_front();
        if (queue.empty())
            ResetEvent(ready);
        return packet;
    }
    void input(const QByteArray &ethernet) {
        const auto b = ethernet.mid(14, be16(ethernet, 16));
        require(b.size() >= 40 && quint8(b[9]) == 6, "TCP through production Ethernet policy");
        const int ihl = (quint8(b[0]) & 15) * 4;
        if (be16(b, ihl + 2) != 45678)
            return;
        clientPort = be16(b, ihl);
        const auto flags = quint8(b[ihl + 13]);
        const auto seq = be32(b, ihl + 4);
        const int th = (quint8(b[ihl + 12]) >> 4) * 4;
        require(th >= 20 && b.size() >= ihl + th, "valid TCP header");
        if (flags & 4)
            throw std::runtime_error("Windows reset bulk TCP connection");
        if ((flags & 2) && !(flags & 16)) {
            clientNext = seq + 1;
            for (int i = ihl + 20; i < ihl + th;) {
                auto kind = quint8(b[i]);
                if (!kind)
                    break;
                if (kind == 1) {
                    ++i;
                    continue;
                }
                require(i + 1 < ihl + th, "TCP options");
                int size = quint8(b[i + 1]);
                require(size >= 2 && i + size <= ihl + th, "TCP option size");
                if (kind == 3 && size == 3)
                    windowShift = std::min(14, int(quint8(b[i + 2])));
                i += size;
            }
            serverNext = 10000;
            serverAcked = 10000;
            output(0x12);
            return;
        }
        window = quint32(be16(b, ihl + 14)) << windowShift;
        if (flags & 16) {
            serverAcked = std::max(serverAcked, be32(b, ihl + 8));
            connected = true;
        }
        auto data = b.mid(ihl + th);
        if (!data.isEmpty()) {
            if (!uploadStart)
                uploadStart = clockMs();
            if (seq == clientNext) {
                require(uploaded + data.size() <= payload.size() &&
                            data == payload.mid(uploaded, data.size()),
                        "uploaded binary bytes are exact");
                uploaded += int(data.size());
                clientNext += quint32(data.size());
                if (uploaded == payload.size() && !uploadEnd)
                    uploadEnd = clockMs();
            }
            output(0x10);
        }
        if ((flags & 1) && seq + data.size() == clientNext) {
            ++clientNext;
            output(0x10);
        }
    }
    void produce() {
        if (!connected || uploaded != payload.size() || finished)
            return;
        int count = 0;
        while (downloaded < payload.size() && count++ < AccessPacketBudget) {
            auto outstanding = serverNext - serverAcked;
            if (outstanding >= window)
                break;
            int n = std::min({1460, int(payload.size() - downloaded), int(window - outstanding)});
            if (!downloadStart)
                downloadStart = clockMs();
            output(0x18, payload.mid(downloaded, n));
            downloaded += n;
        }
        if (downloaded == payload.size() && serverNext - serverAcked < window) {
            output(0x11);
            finished = true;
        }
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    auto report = [&](const QJsonObject &o) {
        auto bytes = QJsonDocument(o).toJson();
        std::cout << bytes.constData() << std::flush;
        const auto i = args.indexOf("--report");
        if (i >= 0 && i + 1 < args.size()) {
            QFile f(args[i + 1]);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(bytes);
        }
    };
    if (!elevated()) {
        report({{"state", "skip"}, {"reason", "administrator required for disposable Wintun test"}});
        return 77;
    }
    try {
        UdpSocket winsock;
        const auto before = addresses();
        AccessPlan plan{0x0afdf800, {{0x0afdf802, "02:00:00:00:00:02", "synthetic"}}};
        require(subnetConflict(plan.subnet, adapters()).isEmpty(), "isolated subnet is free");
        AccessAdapter adapter;
        require(adapter.open(plan, "ConsoleLAN Test"), qPrintable(adapter.error));
        require(adapter.readEvent(), "real Wintun readiness event");
        QByteArray payload(32 * 1024 * 1024, 0);
        for (int i = 0; i < payload.size(); ++i)
            payload[i] = char(i * 19);
        Peer peer(plan, payload);
        HANDLE parent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        auto closeParent = qScopeGuard([&] { CloseHandle(parent); });
        auto client = std::async(std::launch::async, [&] {
            Socket s;
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(plan.targets[0].ip);
            a.sin_port = htons(45678);
            require(connect(s.value, reinterpret_cast<sockaddr *>(&a), sizeof(a)) == 0,
                    "Windows bulk TCP connects");
            int sent = 0;
            while (sent < payload.size()) {
                auto n = send(s.value, payload.constData() + sent, int(payload.size() - sent), 0);
                require(n > 0, "bulk upload sends");
                sent += n;
            }
            shutdown(s.value, SD_SEND);
            QByteArray buffer(65536, 0);
            int received = 0;
            while (received < payload.size()) {
                auto n = recv(s.value, buffer.data(), int(buffer.size()), 0);
                require(n > 0, "bulk download receives");
                require(buffer.left(n) == payload.mid(received, n), "downloaded binary bytes are exact");
                received += n;
            }
            return clockMs();
        });
        const bool legacy = args.contains("--legacy");
        quint64 turns = 0, packets = 0, saturatedTurns = 0, waits = 0;
        const std::array<HANDLE, 2> events{adapter.readEvent(), peer.ready};
        const auto deadline = clockMs() + 60000;
        while (client.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
               clockMs() < deadline) {
            auto tx = drainAccessPackets([&] { return adapter.receive(); },
                                         [&](const QByteArray &ip) {
                                             if (auto frame = accessToEthernet(plan, ip))
                                                 peer.input(frame->bytes);
                                             return true;
                                         },
                                         legacy ? 128 : AccessPacketBudget);
            peer.produce();
            auto rx = drainAccessPackets([&] { return peer.receive(); },
                                         [&](const QByteArray &b) {
                                             auto reply = accessFromEthernet(plan, "synthetic", b);
                                             require(!reply.ip.isEmpty() && adapter.send(reply.ip),
                                                     "console reply enters real Windows TCP stack");
                                             return true;
                                         },
                                         legacy ? 128 : AccessPacketBudget);
            require(adapter.error.isEmpty(), qPrintable(adapter.error));
            ++turns;
            packets += tx.packets + rx.packets;
            if (tx.saturated || rx.saturated)
                ++saturatedTurns;
            if (legacy)
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            else if (!tx.saturated && !rx.saturated) {
                ++waits;
                require(waitForAccessActivity(parent, events, 20) != WAIT_FAILED, "packet wait succeeds");
            }
        }
        require(client.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready,
                "bulk transfer finishes within bound");
        const auto end = client.get();
        require(peer.uploaded == payload.size() && peer.downloaded == payload.size(),
                "both bulk directions complete");
        adapter.close();
        require(addresses() == before, "existing Ethernet and VPN addresses preserved");
        report({{"state", "pass"},
                {"mode", legacy ? "legacy-128-and-sleep" : "event-driven"},
                {"bytesEachDirection", payload.size()},
                {"uploadMBps",
                 payload.size() / double(std::max<qint64>(1, peer.uploadEnd - peer.uploadStart)) / 1000.0},
                {"downloadMBps",
                 payload.size() / double(std::max<qint64>(1, end - peer.downloadStart)) / 1000.0},
                {"turns", double(turns)},
                {"packets", double(packets)},
                {"saturatedTurns", double(saturatedTurns)},
                {"idleWaits", double(waits)},
                {"binaryExact", true},
                {"physicalConsole", false}});
    } catch (const std::exception &e) {
        report({{"state", "fail"}, {"reason", e.what()}});
        return 1;
    }
    return 0;
}
