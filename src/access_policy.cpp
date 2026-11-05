#include "access_policy.hpp"
namespace cl {
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
} // namespace cl
