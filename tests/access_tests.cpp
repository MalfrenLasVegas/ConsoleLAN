#include "access_policy.hpp"
#include "room.hpp"
#include <iostream>
#include <stdexcept>
using namespace cl;
static int checks = 0;
static void check(bool b, const char *s) {
    if (!b)
        throw std::runtime_error(s);
    ++checks;
    std::cout << "PASS " << s << "\n";
}
static void p16(QByteArray &b, int p, quint16 n) {
    b[p] = char(n >> 8);
    b[p + 1] = char(n);
}
static void p32(QByteArray &b, int p, quint32 n) {
    p16(b, p, quint16(n >> 16));
    p16(b, p + 2, quint16(n));
}
static QByteArray packet(quint32 src, quint32 dst) {
    QByteArray b(40, 0);
    b[0] = 0x45;
    p16(b, 2, 40);
    b[8] = 64;
    b[9] = 6;
    p32(b, 12, src);
    p32(b, 16, dst);
    return b;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        const quint32 net = 0x0afdf800;
        QJsonObject c{{"owner", "local"},      {"key", "console"},           {"name", "PS5"},
                      {"ip", ipText(net | 2)}, {"mac", "02:00:00:00:00:02"}, {"adapter", "ethernet"}};
        auto remote = c;
        remote["owner"] = "remote";
        remote["ip"] = ipText(net | 3);
        QJsonObject v{{"mode", "client"},
                      {"owner", "local"},
                      {"subnet", ipText(net)},
                      {"consoles", QJsonArray{c, remote}}};
        auto plan = accessPlan(v);
        check(plan && plan->targets.size() == 1, "only owned local consoles are accessible");
        check(plan->pcIp() == (net | 1), "PC address uses reserved .1; console pool unchanged");
        auto frame = accessToEthernet(*plan, packet(net | 1, net | 2));
        check(frame && frame->adapter == "ethernet" && inspect(frame->bytes)->destination == c["mac"],
              "IPv4 mapped to correct physical MAC and adapter");
        check(!accessToEthernet(*plan, packet(net | 1, net | 3)),
              "remote console cannot be reached through local access");
        check(!accessToEthernet(*plan, packet(net | 1, 0x08080808)),
              "no Internet route through local access");
        check(!accessToEthernet(*plan, packet(net | 1, net | 255)), "broadcast cannot enter local access");
        check(!accessToEthernet(*plan, packet(net | 2, net | 2)), "spoofed PC source blocked");
        for (quint16 port : {1, 21, 1337, 2121, 21210, 65535}) {
            auto tcpPacket = packet(net | 1, net | 2);
            p16(tcpPacket, 20, 45678);
            p16(tcpPacket, 22, port);
            auto forwarded = accessToEthernet(*plan, tcpPacket);
            check(forwarded && forwarded->bytes.mid(14) == tcpPacket,
                  qPrintable(
                      QString("TCP port %1 and payload are preserved without a port allowlist").arg(port)));
        }
        auto bad = packet(net | 1, net | 2);
        bad[0] = 0x65;
        check(!accessToEthernet(*plan, bad), "IPv6 cannot enter IPv4 local path");
        bad = packet(net | 1, net | 2);
        p16(bad, 2, 500);
        check(!accessToEthernet(*plan, bad), "truncated IP packet blocked");
        auto ip = packet(net | 2, net | 1);
        auto reply = QByteArray::fromHex(AccessMac.toLatin1()) +
                     QByteArray::fromHex(c["mac"].toString().toLatin1()) + QByteArray::fromHex("0800") + ip;
        check(accessFromEthernet(*plan, "ethernet", reply).ip == ip,
              "console reply becomes IP packet for Windows");
        check(accessFromEthernet(*plan, "other-nic", reply).ip.isEmpty(), "reply on wrong Ethernet blocked");
        reply[6] = char(4);
        check(accessFromEthernet(*plan, "ethernet", reply).ip.isEmpty(), "spoofed console MAC blocked");
        reply = frame->bytes;
        reply.replace(6, 6, QByteArray::fromHex("020000000099"));
        check(accessFromEthernet(*plan, "ethernet", reply).conflict,
              "foreign sender on PC IP stops local access");
        QByteArray arp(60, 0);
        arp.replace(0, 6, QByteArray::fromHex("ffffffffffff"));
        arp.replace(6, 6, QByteArray::fromHex(c["mac"].toString().toLatin1()));
        p16(arp, 12, 0x0806);
        p16(arp, 14, 1);
        p16(arp, 16, 0x0800);
        arp[18] = 6;
        arp[19] = 4;
        p16(arp, 20, 1);
        arp.replace(22, 6, arp.mid(6, 6));
        p32(arp, 28, net | 2);
        p32(arp, 38, net | 1);
        auto answer = accessFromEthernet(*plan, "ethernet", arp).ethernet;
        check(!answer.isEmpty() && inspect(answer)->sourceIp == (net | 1) && be16(answer, 20) == 2,
              "ARP responder makes local PC reachable from console");
        check(isLocalAccessFrame(arp, net) && isLocalAccessFrame(frame->bytes, net),
              "ARP and IPv4 PC traffic marked outside VPN plane");
        Room host;
        host.host("isolation", net, clockMs());
        const auto filtered = host.view()["filtered"].toDouble();
        host.captured("ethernet", arp, clockMs());
        check(host.outbound.isEmpty() && host.view()["filtered"].toDouble() > filtered,
              "room capture never sends PC access ARP to VPN");
        // Regression: Windows routing can retransmit the console's IPv4 packet with the PC NIC MAC.
        // It is not a second console claiming the reservation.
        const QString pcMac = "02:00:00:00:00:10";
        const QString consoleMac = "02:00:00:00:00:20";
        Room echoes;
        echoes.host("echo-regression", net, clockMs());
        echoes.addConsole("PS5", "ethernet");
        echoes.tick(clockMs() + 2000);
        auto makeFrame = [&](const QString &source, quint32 destination) {
            return QByteArray::fromHex("020000000030") + QByteArray::fromHex(source.toLatin1()) +
                   QByteArray::fromHex("0800") + packet(net | 2, destination);
        };
        auto own = makeFrame(pcMac, 0xcb007109);
        echoes.captured("ethernet", own, clockMs(), pcMac);
        check(echoes.remembered()[0].mac.isEmpty(),
              "PC forwarded traffic must not become the console identity");
        auto real = makeFrame(consoleMac, 0xe00000fb);
        echoes.captured("ethernet", real, clockMs(), pcMac);
        check(echoes.remembered()[0].mac == consoleMac, "real console MAC is learned normally");
        echoes.captured("ethernet", own, clockMs(), pcMac);
        check(!echoes.view()["consoles"].toArray()[0].toObject()["status"].toString().contains("CONFLICTO"),
              "PC retransmission must not create an IP MAC conflict");
        auto duplicate = makeFrame("02:00:00:00:00:99", 0xe00000fb);
        echoes.captured("ethernet", duplicate, clockMs(), pcMac);
        check(echoes.view()["consoles"].toArray()[0].toObject()["status"].toString().contains("CONFLICTO"),
              "a genuinely different console MAC still creates a conflict");
        auto invalid = v;
        invalid["mode"] = "offline";
        check(!accessPlan(invalid), "feature requires active room");
        invalid = v;
        invalid["consoles"] = QJsonArray{remote};
        check(!accessPlan(invalid), "remote-only room does not enable local access");
        auto conflict = c;
        conflict["status"] = "CONFLICTO MAC";
        invalid = v;
        invalid["consoles"] = QJsonArray{conflict};
        check(!accessPlan(invalid), "conflicted console cannot enable access");
        auto unknown = c;
        unknown["mac"] = "";
        invalid["consoles"] = QJsonArray{unknown};
        check(!accessPlan(invalid), "wait for learned console MAC");
        invalid = v;
        invalid["consoles"] = QJsonArray{c, c};
        check(!accessPlan(invalid), "duplicate addresses rejected");
        invalid = v;
        invalid["subnet"] = "8.8.8.0";
        check(!accessPlan(invalid), "public subnet rejected");
        std::cout << "SUCCESS: " << checks << " local access checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
