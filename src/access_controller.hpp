#pragma once
#include "common.hpp"
#include <thread>
#include <mutex>
namespace cl {
class AccessController {
    std::mutex mutex_;
    bool requested_ = false;
    QJsonObject view_, status_{{"state", "off"}};
    void run(std::stop_token stop);
    std::jthread thread_;

  public:
    AccessController();
    ~AccessController();
    void enable(bool enabled);
    void update(QJsonObject view);
    QJsonObject status();
};
} // namespace cl
