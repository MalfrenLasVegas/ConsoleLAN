#pragma once
#include <QtCore>
#include <cstdint>
#include <optional>
#include <functional>
#include <map>
#include <set>
#include <deque>
namespace cl {
using Id = quint64;
inline Id randomId() {
    Id n;
    do {
        n = QRandomGenerator::system()->generate64();
    } while (!n);
    return n;
}
inline QString hex(Id n) {
    return QString::number(n, 16);
}
inline Id id(const QJsonValue &v) {
    bool ok = false;
    auto n = v.toString().toULongLong(&ok, 16);
    return ok ? n : 0;
}
inline qint64 clockMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
struct Endpoint {
    quint32 ip = 0;
    quint16 port = 0;
    auto operator<=>(const Endpoint &) const = default;
    QString text() const;
};
inline QString ipText(quint32 n) {
    return QString("%1.%2.%3.%4").arg(n >> 24).arg((n >> 16) & 255).arg((n >> 8) & 255).arg(n & 255);
}
inline std::optional<quint32> parseIp(const QString &s) {
    auto parts = s.split('.');
    if (parts.size() != 4)
        return {};
    quint32 n = 0;
    for (auto &p : parts) {
        bool ok;
        int v = p.toInt(&ok);
        if (!ok || v < 0 || v > 255 || p.isEmpty())
            return {};
        n = (n << 8) | quint32(v);
    }
    return n;
}
inline QString Endpoint::text() const {
    return ipText(ip) + ":" + QString::number(port);
}
inline QByteArray jsonBytes(const QJsonObject &o) {
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}
inline QJsonObject jsonObject(const QByteArray &b) {
    return QJsonDocument::fromJson(b).object();
}
inline QString cleanName(QString n) {
    return n.simplified().left(64);
}
inline quint16 be16(const QByteArray &b, int p) {
    return (quint16(quint8(b[p])) << 8) | quint8(b[p + 1]);
}
inline quint32 be32(const QByteArray &b, int p) {
    return (quint32(be16(b, p)) << 16) | be16(b, p + 2);
}
} // namespace cl
