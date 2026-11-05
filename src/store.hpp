#pragma once
#include "common.hpp"
namespace cl {
class Store {
    QString path_;

  public:
    QJsonObject data;
    QString error;
    explicit Store(QString path);
    bool save();
};
} // namespace cl
