#pragma once
#include "ethernet.hpp"
namespace cl {
// Local PC access is deliberately not a member of the room or its VPN transport.
inline const QString AccessMac = "02:43:4c:50:43:01";
struct AccessTarget {
    quint32 ip = 0;
    QString mac, adapter;
    bool operator==(const AccessTarget &) const = default;
};
struct AccessPlan {
    quint32 subnet = 0;
    QList<AccessTarget> targets;
    quint32 pcIp() const {
        return subnet | 1;
    }
    bool operator==(const AccessPlan &) const = default;
};
std::optional<AccessPlan> accessPlan(const QJsonObject &view, QString *error = nullptr);
bool isLocalAccessFrame(const QByteArray &frame, quint32 subnet);
struct AccessEthernet {
    QString adapter;
    QByteArray bytes;
};
std::optional<AccessEthernet> accessToEthernet(const AccessPlan &, const QByteArray &ip);
struct AccessReply {
    QByteArray ip, ethernet;
    bool conflict = false;
};
AccessReply accessFromEthernet(const AccessPlan &, const QString &adapter, const QByteArray &frame);
} // namespace cl
