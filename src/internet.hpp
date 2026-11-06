#pragma once
#include "access_policy.hpp"
#include <functional>
namespace cl {
// One stack per helper. All calls, callbacks and socket polling run on its thread.
class InternetGateway {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    using Output = std::function<bool(const AccessEthernet &)>;
    InternetGateway();
    ~InternetGateway();
    QString error;
    bool open(const AccessPlan &, Output);
    void close();
    bool active() const;
    bool input(const QString &adapter, const QByteArray &frame);
    void poll();
    quint64 sent() const;
    quint64 received() const;
#ifdef CONSOLELAN_NATIVE_TESTS
    void allowLoopbackForTest();
#endif
};
} // namespace cl
