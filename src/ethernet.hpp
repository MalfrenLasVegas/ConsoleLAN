#pragma once
#include "common.hpp"
namespace cl {
struct EthernetFrame {
    QString source, destination;
    quint16 etherType = 0;
    quint32 sourceIp = 0, destIp = 0;
    bool group = false, arp = false;
    bool udp = false, moreFragments = false;
    quint16 fragmentOffset = 0, ipId = 0;
};
QString macText(const QByteArray &b, int p);
bool validMac(const QString &s);
std::optional<EthernetFrame> inspect(const QByteArray &bytes);
struct Console {
    QString key, name, owner, mac, adapter;
    quint32 ip = 0;
    QString status;
    bool manualAdapter = false;
};
QJsonObject consoleJson(const Console &c);
Console consoleFromJson(const QJsonObject &o);
struct Route {
    bool accepted = false, inject = false;
    std::set<Id> nodes;
};
// Authority validates the reservation and MAC association on both ends.
Route routeFrame(const QByteArray &b, Id origin, Id local, const QList<Console> &consoles,
                 const std::map<QString, Id> &owners);
} // namespace cl
