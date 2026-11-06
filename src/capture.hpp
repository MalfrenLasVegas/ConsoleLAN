#pragma once
#include "common.hpp"
namespace cl {
class Capture {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    QString adapter, error;
    Capture();
    ~Capture();
    bool open(QString adapter, const QString &filter = {});
    std::optional<QByteArray> receive();
    bool inject(const QByteArray &b);
    void *readEvent() const;
    static QString version();
};
QByteArray bridgeBeacon(Id node);
std::optional<Id> beaconNode(const QByteArray &b);
} // namespace cl
