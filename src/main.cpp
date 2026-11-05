#include "window.hpp"
#include <iostream>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("ConsoleLAN");
    app.setOrganizationName("ConsoleLAN");
    app.setApplicationVersion("1.0.0");
    QString data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(data);
    auto args = app.arguments();
    if (args.contains("--diagnose")) {
        QJsonArray a;
        for (auto &n : cl::adapters())
            a.append(QJsonObject{{"id", n.id},
                                 {"name", n.name},
                                 {"ip", cl::ipText(n.ip)},
                                 {"index", int(n.index)},
                                 {"prefix", n.prefix},
                                 {"up", n.up},
                                 {"physical", n.physical}});
        QFile out;
        if (!out.open(stdout, QIODevice::WriteOnly))
            return 1;
        out.write(QJsonDocument(QJsonObject{{"npcap", cl::Capture::version()}, {"adapters", a}}).toJson());
        return 0;
    }
    if (args.contains("--smoke-test")) {
        app.setOrganizationName("ConsoleLAN-smoke");
        data = QDir::tempPath() + "/ConsoleLAN-smoke-" + cl::hex(cl::randomId());
    }
    QDir().mkpath(data);
    QLockFile instance(data + "/instance.lock");
    if (!instance.tryLock(0)) {
        QMessageBox::information(nullptr, "ConsoleLAN", "ConsoleLAN ya está abierto para este usuario.");
        return 1;
    }
    cl::Window window(data);
    window.show();
    if (args.contains("--smoke-test"))
        QTimer::singleShot(1200, &app, [&] {
            auto i = args.indexOf("--screenshot");
            if (i >= 0 && i + 1 < args.size())
                window.grab().save(args[i + 1]);
            app.quit();
        });
    return app.exec();
}
