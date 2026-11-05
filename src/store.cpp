#include "store.hpp"
namespace cl {
Store::Store(QString path) : path_(std::move(path)) {
    QFile f(path_);
    if (!f.exists())
        return;
    if (!f.open(QIODevice::ReadOnly)) {
        error = "No se pudo leer la persistencia: " + f.errorString();
        return;
    }
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(f.readAll(), &e);
    if (e.error != QJsonParseError::NoError || !doc.isObject()) {
        error = "Persistencia dañada; se conserva el archivo. Restaure una copia antes de continuar.";
        return;
    }
    data = doc.object();
}
bool Store::save() {
    if (!error.isEmpty())
        return false;
    QDir().mkpath(QFileInfo(path_).absolutePath());
    QSaveFile f(path_);
    if (!f.open(QIODevice::WriteOnly)) {
        error = f.errorString();
        return false;
    }
    auto b = QJsonDocument(data).toJson();
    if (f.write(b) != b.size() || !f.commit()) {
        error = f.errorString();
        return false;
    }
    return true;
}
} // namespace cl
