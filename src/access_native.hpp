#pragma once
#include "access_policy.hpp"
namespace cl {
class AccessAdapter {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    QString error;
    AccessAdapter();
    ~AccessAdapter();
    bool open(const AccessPlan &plan, QString name = "ConsoleLAN Local");
    void close();
    std::optional<QByteArray> receive();
    bool send(const QByteArray &ip);
    quint32 index() const;
    quint64 loopGuardId() const;
#ifdef CONSOLELAN_NATIVE_TESTS
    bool removeLoopGuardForTest();
#endif
};
#ifdef CONSOLELAN_NATIVE_TESTS
int liveAccessGuardTest(const QString &reportPath);
#endif
int accessHelperMain(const QString &pipe, quint32 parentPid);
} // namespace cl
