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
};
int accessHelperMain(const QString &pipe, quint32 parentPid);
} // namespace cl
