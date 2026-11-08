#pragma once
#include "common.hpp"
#include "capture_batch.hpp"
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
    // Opt-in: used only by the PC-to-local-console access helper.
    bool injectLocalBatch(std::span<const QByteArray> frames);
    void *readEvent() const;
    static QString version();
};
QByteArray bridgeBeacon(Id node);
std::optional<Id> beaconNode(const QByteArray &b);
} // namespace cl
