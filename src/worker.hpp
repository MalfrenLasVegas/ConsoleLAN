#pragma once
#include "network.hpp"
#include "capture.hpp"
#include <thread>
#include <mutex>
namespace cl {
class Worker : public QObject {
    Q_OBJECT
    std::mutex mutex_;
    std::deque<QJsonObject> commands_;
    QString dataDir_;
    std::jthread thread_;
    void run(std::stop_token stop);

  public:
    explicit Worker(QString dataDir, QObject *parent = nullptr);
    ~Worker();
    void post(QJsonObject command);
  signals:
    void updated(QJsonObject view);
    void logged(QString line);
};
} // namespace cl
