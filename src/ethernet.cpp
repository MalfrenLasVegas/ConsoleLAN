#include "ethernet.hpp"
namespace cl {
QString macText(const QByteArray &b, int p) {
    return b.mid(p, 6).toHex(':');
}
bool validMac(const QString &s) {
    auto b = QByteArray::fromHex(s.toLatin1());
    return s.size() == 17 && b.size() == 6 && !(quint8(b[0]) & 1) && b != QByteArray(6, 0) &&
           b.toHex(':') == s.toLatin1();
}
std::optional<EthernetFrame> inspect(const QByteArray &b) {
    if (b.size() < 14 || b.size() > 1514)
        return {};
    EthernetFrame f{macText(b, 6), macText(b, 0), be16(b, 12), 0, 0, bool(quint8(b[0]) & 1), false};
    if (!validMac(f.source))
        return {};
    if (f.etherType == 0x0806) {
        if (b.size() < 42 || be16(b, 14) != 1 || be16(b, 16) != 0x0800 || quint8(b[18]) != 6 ||
            quint8(b[19]) != 4 || (be16(b, 20) != 1 && be16(b, 20) != 2) || macText(b, 22) != f.source)
            return {};
        f.sourceIp = be32(b, 28);
        f.destIp = be32(b, 38);
        f.arp = true;
    } else if (f.etherType == 0x0800) {
        if (b.size() < 34 || (quint8(b[14]) >> 4) != 4)
            return {};
        int ihl = (quint8(b[14]) & 15) * 4;
        int len = be16(b, 16);
        if (ihl < 20 || len < ihl || b.size() < 14 + len)
            return {};
        f.sourceIp = be32(b, 26);
        f.destIp = be32(b, 30);
        f.fragmentOffset = be16(b, 20) & 0x1fff;
        f.moreFragments = bool(be16(b, 20) & 0x2000);
        f.ipId = be16(b, 18);
        if ((be16(b, 20) & 0x8000) || (f.moreFragments && ((len - ihl) % 8)))
            return {};
        if (quint8(b[23]) == 17) {
            f.udp = true;
            if (!f.fragmentOffset) {
                if (len < ihl + 8)
                    return {};
                auto a = be16(b, 14 + ihl), d = be16(b, 16 + ihl);
                if (a == 67 || a == 68 || d == 67 || d == 68)
                    return {};
            }
        }
    } else
        return {}; // IPv6, VLAN, LLDP and other EtherTypes stay on the physical LAN.
    return f;
}
QJsonObject consoleJson(const Console &c) {
    return {{"key", c.key},         {"name", c.name},
            {"owner", c.owner},     {"mac", c.mac},
            {"adapter", c.adapter}, {"ip", ipText(c.ip)},
            {"status", c.status},   {"manualAdapter", c.manualAdapter}};
}
Console consoleFromJson(const QJsonObject &o) {
    return {o["key"].toString(),     cleanName(o["name"].toString()),
            o["owner"].toString(),   o["mac"].toString().toLower(),
            o["adapter"].toString(), parseIp(o["ip"].toString()).value_or(0),
            o["status"].toString(),  o["manualAdapter"].toBool()};
}
Route routeFrame(const QByteArray &b, Id origin, Id local, const QList<Console> &consoles,
                 const std::map<QString, Id> &owners) {
    Route out;
    auto f = inspect(b);
    if (!f)
        return out;
    const Console *src = nullptr;
    const Console *dst = nullptr;
    for (auto &c : consoles) {
        if (!c.mac.isEmpty() && c.mac == f->source)
            src = &c;
        if (!c.mac.isEmpty() && c.mac == f->destination)
            dst = &c;
    }
    if (!src || src->status.contains("CONFLICTO") || !owners.contains(src->owner) ||
        owners.at(src->owner) != origin || src->ip != f->sourceIp)
        return out;
    out.accepted = true;
    if (!f->group && dst && owners.contains(dst->owner)) {
        Id target = owners.at(dst->owner);
        if (target != origin) {
            if (target == local)
                out.inject = true;
            else
                out.nodes.insert(target);
        }
        return out;
    }
    // ARP/broadcast/multicast and unknown unicast are flooded only to sites with consoles.
    for (auto &c : consoles)
        if (owners.contains(c.owner)) {
            Id n = owners.at(c.owner);
            if (n != origin) {
                if (n == local)
                    out.inject = true;
                else
                    out.nodes.insert(n);
            }
        }
    return out;
}
} // namespace cl
