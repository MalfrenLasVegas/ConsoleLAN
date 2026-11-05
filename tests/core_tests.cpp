#include "room.hpp"
#include <iostream>
#include <stdexcept>
using namespace cl;
static int checks = 0;
static void check(bool ok, const char *text) {
    if (!ok)
        throw std::runtime_error(text);
    ++checks;
    std::cout << "PASS " << text << "\n";
}
static void write16(QByteArray &b, int p, quint16 n) {
    b[p] = char(n >> 8);
    b[p + 1] = char(n);
}
static void write32(QByteArray &b, int p, quint32 n) {
    write16(b, p, quint16(n >> 16));
    write16(b, p + 2, quint16(n));
}
static QByteArray arp(QString src, quint32 sender, quint32 target, QString dst = "ff:ff:ff:ff:ff:ff") {
    QByteArray b(60, 0);
    b.replace(0, 6, QByteArray::fromHex(dst.toLatin1()));
    b.replace(6, 6, QByteArray::fromHex(src.toLatin1()));
    write16(b, 12, 0x0806);
    write16(b, 14, 1);
    write16(b, 16, 0x0800);
    b[18] = 6;
    b[19] = 4;
    write16(b, 20, 1);
    b.replace(22, 6, b.mid(6, 6));
    write32(b, 28, sender);
    write32(b, 38, target);
    return b;
}
static QByteArray ipv4(QString src, quint32 sender, QString dst, quint32 target, int payload = 100) {
    QByteArray b(14 + 20 + 8 + payload, 0);
    b.replace(0, 6, QByteArray::fromHex(dst.toLatin1()));
    b.replace(6, 6, QByteArray::fromHex(src.toLatin1()));
    write16(b, 12, 0x0800);
    b[14] = 0x45;
    write16(b, 16, quint16(b.size() - 14));
    b[22] = 64;
    b[23] = 17;
    write32(b, 26, sender);
    write32(b, 30, target);
    write16(b, 34, 1000);
    write16(b, 36, 1001);
    write16(b, 38, quint16(8 + payload));
    return b;
}
struct Lab {
    Room h, a, b;
    Endpoint eh{0x7f000001, 32190}, ea{0x7f000001, 32191}, eb{0x7f000001, 32192};
    qint64 now = 10000;
    void pump() {
        for (int turn = 0; turn < 40; ++turn) {
            bool sent = false;
            for (auto pair : {std::pair{&h, eh}, std::pair{&a, ea}, std::pair{&b, eb}}) {
                auto ds = std::exchange(pair.first->outbound, {});
                for (auto &d : ds) {
                    sent = true;
                    if (d.endpoint == eh)
                        h.receive(pair.second, d.bytes, now);
                    else if (d.endpoint == ea)
                        a.receive(pair.second, d.bytes, now);
                    else if (d.endpoint == eb)
                        b.receive(pair.second, d.bytes, now);
                }
            }
            if (!sent)
                return;
        }
        throw std::runtime_error("network pump did not settle");
    }
    void step(int ms = 1000) {
        now += ms;
        h.tick(now);
        a.tick(now);
        b.tick(now);
        pump();
    }
    Console console(Room &r) {
        for (auto &c : r.consoles())
            if (c.owner == r.owner())
                return c;
        throw std::runtime_error("missing console");
    }
    void clear() {
        h.injections.clear();
        a.injections.clear();
        b.injections.clear();
        h.outbound.clear();
        a.outbound.clear();
        b.outbound.clear();
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        Header header{Type::Frame, 1, 2, 3, 4};
        QByteArray data(1514, 'x');
        auto ds = fragment(header, data, 512);
        check(ds.size() == 4, "conservative MTU fragments Ethernet frame");
        Reassembler r;
        std::optional<Packet> done;
        for (int i = int(ds.size()) - 1; i >= 0; --i) {
            auto d = decode(ds[i]);
            check(bool(d), "fragment binary decode");
            auto v = r.accept({1, 2}, *d, 100);
            if (v)
                done = v;
            if (i > 0)
                r.accept({1, 2}, *d, 100);
        }
        check(done && done->payload == data, "out-of-order fragments and duplicates reassemble");
        check(!r.accept({1, 2}, *decode(ds[0]), 200), "completed frame replay rejected");
        auto bad = ds[0];
        bad[4] = 9;
        check(!decode(bad), "unsupported version rejected");
        bad = ds[0];
        bad[42] = 100;
        check(!decode(bad), "bad fragment index rejected");
        bad = ds[0];
        bad[46] = 1;
        check(!decode(bad), "reserved header flags rejected");
        Reassembler expiry;
        expiry.accept({1, 2}, *decode(ds[0]), 1);
        expiry.expire(1300);
        check(expiry.size() == 0 && expiry.expired == 1, "lost fragment expires and releases memory");
        Reassembler bounded;
        for (Id i = 1; i < 200; ++i) {
            auto p = *decode(ds[0]);
            p.h.frame = i;
            bounded.accept({1, 2}, p, 1);
        }
        check(bounded.size() == 128 && bounded.rejected > 0, "reassembly has strict allocation bound");
        Reassembler clash;
        auto p = *decode(ds[0]);
        clash.accept({1, 2}, p, 1);
        p.payload[0] = 'z';
        clash.accept({1, 2}, p, 2);
        check(clash.rejected == 1 && clash.size() == 0, "conflicting duplicate fragment rejected");
        for (int i = 0; i < 10000; ++i) {
            QByteArray fuzz(i % 1800, 0);
            for (auto &c : fuzz)
                c = char(QRandomGenerator::global()->generate());
            decode(fuzz);
        }
        check(true, "10000 malformed random datagrams handled");
        SequenceLoss loss;
        loss.see(1, 1, 0);
        loss.see(1, 4, 100);
        loss.see(1, 2, 150);
        loss.tick(2200);
        check(loss.lost == 1, "sequence loss tolerates reordering before counting gap");
        auto ar = arp("02:00:00:00:00:01", 0x0afd2a02, 0x0afd2a03);
        check(inspect(ar) && inspect(ar)->sourceIp == 0x0afd2a02, "ARP sender IP parsed");
        auto invalid = ar;
        invalid[22] = 3;
        check(!inspect(invalid), "ARP sender MAC must match Ethernet source");
        auto dhcp = ipv4("02:00:00:00:00:01", 0x0afd2a02, "ff:ff:ff:ff:ff:ff", 0xffffffff);
        write16(dhcp, 34, 68);
        write16(dhcp, 36, 67);
        check(!inspect(dhcp), "DHCP is blocked");
        write16(dhcp, 20, 0x2000);
        check(!inspect(dhcp), "fragmented UDP cannot bypass DHCP block");
        auto other = ar;
        write16(other, 12, 0x86dd);
        check(!inspect(other), "IPv6 stays outside tunnel");
        write16(other, 12, 0x8100);
        check(!inspect(other), "VLAN EtherType rejected explicitly");
        Lab lab;
        lab.h.addConsole("Host", "eth-h");
        lab.a.addConsole("A", "eth-a");
        lab.b.addConsole("B", "eth-b");
        lab.h.host("Test", 0x0afd2a00, lab.now);
        lab.a.join(lab.eh, lab.now);
        lab.b.join(lab.eh, lab.now);
        lab.step();
        lab.step();
        check(lab.a.connected() && lab.b.connected(), "two clients complete endpoint challenge handshake");
        check(lab.h.view()["members"].toArray().size() == 3, "master tracks three nodes");
        auto h = lab.console(lab.h), a = lab.console(lab.a), b = lab.console(lab.b);
        check(h.ip != a.ip && a.ip != b.ip && h.ip != b.ip, "master assigns unique console IPs");
        lab.clear();
        lab.a.captured("eth-a", arp("02:00:00:00:00:aa", 0x01020304, a.ip), lab.now);
        lab.step();
        check(lab.console(lab.a).mac.isEmpty(), "ARP question about reserved IP never learns source MAC");
        auto learn = [&](Room &room, const char *eth, const char *mac, quint32 ip) {
            room.captured(eth, arp(mac, ip, 0x0afd2afe), lab.now);
            lab.step();
        };
        learn(lab.h, "eth-h", "02:00:00:00:00:01", h.ip);
        learn(lab.a, "eth-a", "02:00:00:00:00:02", a.ip);
        learn(lab.b, "eth-b", "02:00:00:00:00:03", b.ip);
        check(lab.console(lab.a).mac == "02:00:00:00:00:02", "MAC learned from ARP sender with reserved IP");
        check(lab.console(lab.b).mac == "02:00:00:00:00:03", "MAC binding synchronized through master");
        lab.clear();
        lab.a.captured("eth-a", ipv4("02:00:00:00:00:02", a.ip, "02:00:00:00:00:03", b.ip), lab.now);
        lab.pump();
        check(lab.b.injections.size() == 1, "client A unicast relayed through master to client B");
        check(lab.h.injections.isEmpty() && lab.a.injections.isEmpty(),
              "known unicast only reaches destination site");
        lab.clear();
        lab.b.captured("eth-b", arp("02:00:00:00:00:03", b.ip, a.ip), lab.now);
        lab.pump();
        check(lab.a.injections.size() == 1 && lab.h.injections.size() == 1,
              "ARP broadcast reaches both other sites");
        lab.clear();
        lab.a.captured("eth-a", ipv4("02:00:00:00:00:02", a.ip, "01:00:5e:01:02:03", 0xef010203), lab.now);
        lab.pump();
        check(lab.b.injections.size() == 1 && lab.h.injections.size() == 1,
              "IPv4 multicast relayed across three sites");
        lab.clear();
        lab.a.captured("eth-a", ipv4("02:00:00:00:00:02", a.ip, "02:ff:00:00:00:09", b.ip), lab.now);
        lab.pump();
        check(lab.b.injections.size() == 1 && lab.h.injections.size() == 1,
              "unknown unicast flooded to console sites");
        lab.clear();
        for (int i = 0; i < 300; ++i)
            lab.a.captured("eth-a", ipv4("02:00:00:00:00:02", a.ip, "02:ff:00:00:00:09", b.ip), lab.now);
        lab.pump();
        check(lab.b.injections.size() <= 200, "unknown unicast flooding rate is bounded");
        lab.step();
        lab.clear();
        auto remote = ipv4("02:00:00:00:00:03", b.ip, "ff:ff:ff:ff:ff:ff", 0xffffffff);
        lab.a.captured("eth-a", remote, lab.now);
        check(lab.a.outbound.isEmpty(), "recaptured remote source excluded from local tunnel");
        lab.clear();
        lab.a.captured("eth-a", ipv4("02:00:00:00:00:77", 0xc0a80103, "ff:ff:ff:ff:ff:ff", 0xffffffff),
                       lab.now);
        check(lab.a.outbound.isEmpty(), "unregistered domestic traffic filtered");
        lab.clear();
        lab.a.captured("eth-a", ipv4("02:00:00:00:00:02", b.ip, "02:00:00:00:00:03", b.ip), lab.now);
        check(lab.a.outbound.isEmpty(), "registered MAC cannot spoof another reserved IP");
        lab.a.addConsole("A2", "eth-a");
        lab.step();
        Console a2;
        for (auto &c : lab.a.consoles())
            if (c.owner == lab.a.owner() && c.key != a.key)
                a2 = c;
        learn(lab.a, "eth-a", "02:00:00:00:00:04", a2.ip);
        lab.clear();
        lab.a.captured("eth-a", ipv4("02:00:00:00:00:02", a.ip, "02:00:00:00:00:04", a2.ip), lab.now);
        check(lab.a.outbound.isEmpty(), "unicast between two local consoles stays on physical switch");
        lab.clear();
        lab.a.setBudget(512);
        lab.b.setBudget(512);
        lab.h.setBudget(512);
        lab.a.captured("eth-a", ipv4("02:00:00:00:00:02", a.ip, "02:00:00:00:00:03", b.ip, 1400), lab.now);
        lab.pump();
        check(lab.b.injections.size() == 1 && lab.b.injections[0].bytes.size() == 1442,
              "large Ethernet frame survives client-master-client fragmentation");
        lab.clear();
        auto firstIpFragment = ipv4("02:00:00:00:00:02", a.ip, "02:00:00:00:00:03", b.ip, 1400);
        auto laterIpFragment = ipv4("02:00:00:00:00:02", a.ip, "02:00:00:00:00:03", b.ip, 100);
        write16(firstIpFragment, 18, 1234);
        write16(laterIpFragment, 18, 1234);
        write16(firstIpFragment, 20, 0x2000);
        write16(laterIpFragment, 20, 176);
        lab.a.captured("eth-a", laterIpFragment, lab.now);
        lab.pump();
        check(lab.b.injections.isEmpty(), "orphan IPv4 UDP fragment cannot bypass DHCP policy");
        lab.a.captured("eth-a", firstIpFragment, lab.now);
        lab.pump();
        lab.a.captured("eth-a", laterIpFragment, lab.now);
        lab.pump();
        check(lab.b.injections.size() == 2,
              "non-DHCP IPv4 UDP fragments forwarded after validated first fragment");
        lab.clear();
        lab.a.captured("eth-a", remote, lab.now);
        Header fake{Type::Frame, id(lab.h.view()["room"]), 1, lab.a.node, 999};
        auto evil = fragment(fake, remote, 1100);
        auto prev = lab.h.invalid;
        lab.h.receive(lab.ea, evil[0], lab.now);
        check(lab.h.invalid > prev, "declared node and room do not bypass session validation");
        auto wrong = fragment(Header{Type::Frame, randomId(), randomId(), lab.a.node, 999}, remote, 1100);
        prev = lab.h.invalid;
        lab.h.receive({lab.ea.ip, 22222}, wrong[0], lab.now);
        check(lab.h.invalid > prev, "unknown endpoint and wrong room rejected");
        lab.clear();
        auto duplicateHello = fragment(Header{Type::Hello, 0, 0, lab.a.node, randomId()},
                                       jsonBytes({{"owner", QUuid::createUuid().toString()},
                                                  {"nonce", hex(randomId())},
                                                  {"name", "Duplicado"}}),
                                       1100);
        lab.h.receive({0x7f000001, 44444}, duplicateHello.first(), lab.now);
        bool denied = false;
        for (auto &d : lab.h.outbound) {
            auto decodedError = decode(d.bytes);
            if (decodedError && decodedError->h.type == Type::Error)
                denied = true;
        }
        check(denied && lab.h.view()["members"].toArray().size() == 3,
              "duplicate node ID cannot overwrite an established member");
        lab.h.outbound.clear();
        lab.now += 7000;
        lab.a.tick(lab.now);
        check(lab.a.mode() == "offline", "master timeout stops client forwarding");
        lab.h.tick(lab.now);
        check(lab.h.view()["members"].toArray().size() == 1, "master expires silent members");
        auto oldReservations = lab.h.view()["reservations"].toArray();
        Console disconnected;
        for (auto v : oldReservations) {
            auto c = consoleFromJson(v.toObject());
            if (c.owner == lab.b.owner())
                disconnected = c;
        }
        lab.h.releaseReservation(disconnected.owner, disconnected.key);
        check(lab.h.view()["reservations"].toArray().size() == oldReservations.size() - 1,
              "master can explicitly release a disconnected reservation");
        auto activeCount = lab.h.view()["reservations"].toArray().size();
        lab.h.releaseReservation(h.owner, h.key);
        check(lab.h.view()["reservations"].toArray().size() == activeCount,
              "active reservations cannot be silently released by master");
        Lab overlap;
        overlap.b.validateSubnet = [](quint32) { return QString("red local coincidente"); };
        overlap.h.host("Overlap", 0x0afd2a00, overlap.now);
        overlap.b.join(overlap.eh, overlap.now);
        overlap.step();
        check(overlap.b.mode() == "offline" && overlap.b.view()["status"].toString().contains("incompatible"),
              "client refuses a room subnet overlapping its local network");
        QTemporaryDir dir;
        QString file = dir.path() + "/state.json";
        quint32 reserved = 0;
        QString key;
        Id roomId = 0;
        {
            Store store(file);
            Room rr(&store);
            key = rr.addConsole("Persistida", "eth");
            rr.host("Persistencia", 0x0afb2a00, 1);
            reserved = rr.consoles().first().ip;
            roomId = id(rr.view()["room"]);
        }
        {
            Store store(file);
            Room rr(&store);
            rr.host("Persistencia", 0x0afb2a00, 1);
            check(rr.consoles().first().key == key && rr.consoles().first().ip == reserved &&
                      id(rr.view()["room"]) == roomId,
                  "reservation identity IP and room survive restart");
        }
        {
            QFile f(file);
            if (!f.open(QIODevice::WriteOnly))
                throw std::runtime_error("test file open");
            f.write("broken");
            f.close();
            Store s(file);
            check(!s.error.isEmpty() && !s.save(),
                  "corrupt persistence is preserved without silent overwrite");
        }
        Room autoRoom;
        auto autoKey = autoRoom.addConsole("Auto");
        autoRoom.host("Auto", 0x0afa2a00, 1);
        auto ip = autoRoom.consoles().first().ip;
        auto source = ipv4("02:00:00:00:aa:01", ip, "ff:ff:ff:ff:ff:ff", 0xffffffff);
        autoRoom.captured("one", source, 10);
        check(autoRoom.remembered().first().mac == "02:00:00:00:aa:01" &&
                  autoRoom.remembered().first().adapter == "one",
              "IPv4 source learns both MAC and Ethernet interface");
        autoRoom.captured("two", source, 11);
        check(autoRoom.view()["consoles"].toArray().first().toObject()["status"].toString().contains(
                  "CONFLICTO"),
              "ambiguous Ethernet detection produces visible conflict");
        autoRoom.setAdapter(autoKey, "one");
        autoRoom.captured("one", source, 12);
        check(!autoRoom.view()["consoles"].toArray().first().toObject()["status"].toString().contains(
                  "CONFLICTO"),
              "manual Ethernet selection resolves ambiguity");
        auto large = fragment(Header{Type::State, 1, 2, 3, 999}, QByteArray(50000, 'a'), 512);
        Reassembler stateParts;
        std::optional<Packet> stateDone;
        for (auto &d : large) {
            auto v = stateParts.accept({1, 2}, *decode(d), 1);
            if (v)
                stateDone = v;
        }
        check(stateDone && stateDone->payload.size() == 50000,
              "large room snapshot uses bounded control fragmentation at 512-byte budget");
        std::cout << "SUCCESS: " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << " (after " << checks << " checks)\n";
        return 1;
    }
}
