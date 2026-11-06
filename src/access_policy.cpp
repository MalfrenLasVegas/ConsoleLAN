#include "access_policy.hpp"
namespace cl {
QString localAccessCaptureFilter(quint32 pcIp) {
    return QString("arp or (ip and (ether dst %1 or (src host %2 and not ether src %1)))")
        .arg(AccessMac, ipText(pcIp));
}
QString roomCaptureFilter() {
    // Preserve all duplicate-bridge beacons. ARP/IPv4 with the reserved MAC
    // already fail Room::isLocalAccessFrame; reject them before copying them
    // into the room capture queue so a fast local transfer cannot fill it.
    return QString("ether proto 0x88b5 or ((arp or ip) and not (ether src %1 or ether dst %1))")
        .arg(AccessMac);
}
static void put16(QByteArray &b, int p, quint16 n) {
    b[p] = char(n >> 8);
    b[p + 1] = char(n);
}
static void put32(QByteArray &b, int p, quint32 n) {
    put16(b, p, quint16(n >> 16));
    put16(b, p + 2, quint16(n));
}
std::optional<AccessPlan> accessPlan(const QJsonObject &v, QString *error) {
    auto fail = [&](const char *s) -> std::optional<AccessPlan> {
        if (error)
            *error = QString::fromUtf8(s);
        return {};
    };
    if (v["mode"] != "host" && v["mode"] != "client")
        return fail("Entra en una sala antes de activar el acceso local.");
    AccessPlan p;
    p.subnet = parseIp(v["subnet"].toString()).value_or(0);
    const auto net = p.subnet;
    if ((net & 255) || !((net >> 24) == 10 || (net >> 20) == 0xac1 || (net >> 16) == 0xc0a8))
        return fail("La sala no tiene una subred privada /24 válida.");
    const auto owner = v["owner"].toString();
    if (owner.isEmpty())
        return fail("Falta la identidad del PC.");
    std::set<quint32> ips;
    for (const auto &value : v["consoles"].toArray()) {
        const auto c = consoleFromJson(value.toObject());
        if (c.owner != owner || c.status.contains("CONFLICTO"))
            continue;
        if ((c.ip & 0xffffff00) != net || (c.ip & 255) < 2 || (c.ip & 255) > 254 || !validMac(c.mac) ||
            c.mac == AccessMac || c.adapter.isEmpty())
            continue;
        if (!ips.insert(c.ip).second)
            return fail("Hay direcciones de consola duplicadas.");
        p.targets.append({c.ip, c.mac, c.adapter});
    }
    if (p.targets.isEmpty() || p.targets.size() > 16)
        return fail("Espera a que una consola local tenga IP, MAC y Ethernet detectadas.");
    std::sort(p.targets.begin(), p.targets.end(), [](const auto &a, const auto &b) { return a.ip < b.ip; });
    return p;
}
bool isLocalAccessFrame(const QByteArray &b, quint32 subnet) {
    auto f = inspect(b);
    return f && (f->source == AccessMac || f->destination == AccessMac || f->sourceIp == (subnet | 1) ||
                 f->destIp == (subnet | 1));
}
std::optional<AccessEthernet> accessToEthernet(const AccessPlan &p, const QByteArray &ip) {
    if (ip.size() < 20 || ip.size() > 1500 || (quint8(ip[0]) >> 4) != 4 || be16(ip, 2) != ip.size())
        return {};
    const auto dst = be32(ip, 16);
    for (const auto &t : p.targets)
        if (t.ip == dst) {
            auto b = QByteArray::fromHex(t.mac.toLatin1()) + QByteArray::fromHex(AccessMac.toLatin1()) +
                     QByteArray::fromHex("0800") + ip;
            auto f = inspect(b);
            if (!f || f->sourceIp != p.pcIp() || f->group)
                return {};
            return AccessEthernet{t.adapter, b};
        }
    return {};
}
AccessReply accessFromEthernet(const AccessPlan &p, const QString &adapter, const QByteArray &b) {
    auto f = inspect(b);
    if (!f)
        return {};
    if (f->sourceIp == p.pcIp() && f->source != AccessMac)
        return {{}, {}, true};
    for (const auto &t : p.targets) {
        if (t.adapter != adapter || f->sourceIp != t.ip || f->source != t.mac || f->destIp != p.pcIp())
            continue;
        if (f->arp && be16(b, 20) == 1 && (f->group || f->destination == AccessMac)) {
            auto reply = QByteArray(60, 0);
            reply.replace(0, 6, b.mid(6, 6));
            reply.replace(6, 6, QByteArray::fromHex(AccessMac.toLatin1()));
            put16(reply, 12, 0x0806);
            put16(reply, 14, 1);
            put16(reply, 16, 0x0800);
            reply[18] = 6;
            reply[19] = 4;
            put16(reply, 20, 2);
            reply.replace(22, 6, reply.mid(6, 6));
            put32(reply, 28, p.pcIp());
            reply.replace(32, 6, b.mid(6, 6));
            put32(reply, 38, t.ip);
            return {{}, reply, false};
        }
        if (!f->arp && f->destination == AccessMac)
            return {b.mid(14, be16(b, 16)), {}, false};
    }
    return {};
}
// Internet is a separate, local-only Ethernet path. Never accept a remote room
// peer, an unlearned MAC, DHCP, or a packet addressed to the host's private LAN.
bool internetInput(const AccessPlan &p, const QString &adapter, const QByteArray &b) {
    const auto f = inspect(b);
    if (!f || !std::any_of(p.targets.cbegin(), p.targets.cend(), [&](const auto &t) {
            return t.adapter == adapter && t.ip == f->sourceIp && t.mac == f->source;
        }))
        return false;
    if (f->arp)
        return f->destIp == p.pcIp() && (f->group || f->destination == AccessMac);
    if (f->destination != AccessMac || quint8(b[14]) != 0x45)
        return false;
    const auto proto = quint8(b[23]);
    const auto dst = f->destIp;
    if (dst == p.pcIp()) {
        // Fragments to the host are excluded, so a non-DNS service cannot be
        // smuggled into the DNS proxy via reassembly.
        int h = (quint8(b[14]) & 15) * 4;
        return !f->fragmentOffset && !f->moreFragments && (proto == 6 || proto == 17) &&
               be16(b, 16) >= h + (proto == 6 ? 20 : 8) && be16(b, 16 + h) == 53;
    }
    if (proto != 1 && proto != 6 && proto != 17)
        return false;
    if ((dst >> 24) == 0 || (dst >> 24) == 10 || (dst >> 24) == 127 || (dst >> 22) == 0x191 ||
        (dst >> 16) == 0xa9fe || (dst >> 20) == 0xac1 || (dst >> 16) == 0xc0a8 || (dst >> 24) >= 224 ||
        ((dst >> 24) == 192 && ((dst >> 8) == 0xc00000 || (dst >> 8) == 0xc00002)) || (dst >> 17) == 0x6309 ||
        (dst >> 8) == 0xc63364 || (dst >> 8) == 0xcb0071)
        return false;
    return true;
}
std::optional<AccessEthernet> internetOutput(const AccessPlan &p, const QByteArray &b) {
    const auto f = inspect(b);
    if (!f || f->source != AccessMac)
        return {};
    for (const auto &t : p.targets)
        if (f->destIp == t.ip &&
            (f->destination == t.mac || (f->arp && f->group && f->sourceIp == p.pcIp() && be16(b, 20) == 1)))
            return AccessEthernet{t.adapter, b};
    return {};
}
} // namespace cl
