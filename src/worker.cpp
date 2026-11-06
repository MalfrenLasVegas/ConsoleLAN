#include "worker.hpp"
#include "access_controller.hpp"
#include "access_policy.hpp"
namespace cl {
Worker::Worker(QString dataDir, QObject *parent)
    : QObject(parent), dataDir_(std::move(dataDir)), thread_([this](std::stop_token stop) { run(stop); }) {}
Worker::~Worker() {
    thread_.request_stop();
    if (thread_.joinable())
        thread_.join();
}
void Worker::post(QJsonObject c) {
    std::lock_guard lock(mutex_);
    if (commands_.size() < 64)
        commands_.push_back(std::move(c));
}
void Worker::run(std::stop_token stop) {
    // thread_ is declared last in the class so every object used by it is initialized.
    Store store(dataDir_ + "/state.json");
    Room room(&store);
    AccessController localAccess;
    room.validateSubnet = [](quint32 network) { return subnetConflict(network, adapters()); };
    UdpSocket udp, discovery;
    Adapter selected;
    std::map<QString, std::unique_ptr<Capture>> captures;
    std::map<QString, QString> captureMacs;
    std::map<QString, qint64> blocked;
    QStringList captureErrors;
    QString lastError = store.error;
    quint64 injected = 0, observed = 0, sendErrors = 0;
    qint64 lastView = 0, lastCaps = 0, lastBeacon = 0, lastAdapters = 0;
    int receiveBudget = 0;
    QFile logFile(dataDir_ + "/consolelan.log");
    QDir().mkpath(dataDir_);
    if (logFile.size() > 2 * 1024 * 1024) {
        QFile::remove(dataDir_ + "/consolelan.previous.log");
        logFile.rename(dataDir_ + "/consolelan.previous.log");
        logFile.setFileName(dataDir_ + "/consolelan.log");
    }
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        lastError = "No se pudo abrir el registro: " + logFile.errorString();
    auto log = [&](const QString &line) {
        if (logFile.size() > 2 * 1024 * 1024) {
            logFile.close();
            QFile::remove(dataDir_ + "/consolelan.previous.log");
            QFile::rename(dataDir_ + "/consolelan.log", dataDir_ + "/consolelan.previous.log");
            if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
                lastError = "No se pudo reabrir el registro: " + logFile.errorString();
        }
        QString s = QDateTime::currentDateTime().toString(Qt::ISODateWithMs) + "  " + line;
        if (logFile.isOpen()) {
            logFile.write(s.toUtf8() + "\n");
            logFile.flush();
        }
        emit logged(s);
    };
    log("ConsoleLAN 1.3.1 iniciado. " + Capture::version());
    if (!store.error.isEmpty())
        log(store.error);
    auto flush = [&]() {
        for (auto &d : room.outbound)
            if (!udp.send(d)) {
                ++sendErrors;
                if (lastError != udp.error) {
                    lastError = udp.error;
                    log(lastError);
                }
            }
        room.outbound.clear();
    };
    auto bindSelected = [&](const QJsonObject &c) -> bool {
        auto all = adapters();
        auto it = std::find_if(all.begin(), all.end(), [&](const Adapter &a) {
            return a.id == c["vpn"].toString() && a.ip == parseIp(c["vpnIp"].toString()).value_or(0);
        });
        if (it == all.end() || !it->up || !it->ip) {
            log("Seleccione una interfaz VPN activa con IPv4.");
            return false;
        }
        if (udp.isOpen() && selected.id == it->id && selected.ip == it->ip)
            return true;
        room.leave();
        flush();
        captures.clear();
        selected = *it;
        if (!udp.open(selected)) {
            log(udp.error);
            return false;
        }
        if (!discovery.open(selected, 32189, true))
            log("Descubrimiento no disponible: " + discovery.error + ". Use conexión directa.");
        log("UDP enlazado exclusivamente a " + selected.name + " / " + udp.bound.text());
        return true;
    };
    while (!stop.stop_requested()) {
        qint64 now = clockMs();
        std::deque<QJsonObject> cmds;
        {
            std::lock_guard lock(mutex_);
            cmds.swap(commands_);
        }
        for (auto &c : cmds) {
            auto op = c["op"].toString();
            if (op == "localAccess") {
                localAccess.enable(c["enabled"].toBool());
                continue;
            } else if (op == "internet") {
                localAccess.internet(c["enabled"].toBool());
                continue;
            }
            if (!store.error.isEmpty()) {
                log("Persistencia no disponible: " + store.error);
                continue;
            }
            room.setBudget(c["budget"].toInt(1100));
            if (op == "host") {
                auto subnet = parseIp(c["subnet"].toString());
                auto conflict =
                    subnet ? subnetConflict(*subnet, adapters()) : QString("Subred IPv4 inválida");
                if (!conflict.isEmpty()) {
                    log(conflict);
                    lastError = conflict;
                    continue;
                }
                if (bindSelected(c)) {
                    room.host(c["name"].toString(), *subnet, now);
                    lastError.clear();
                }
            } else if (op == "join") {
                auto ip = parseIp(c["ip"].toString());
                if (!ip || !*ip || (*ip >> 28) >= 14) {
                    log("IP de anfitrión inválida");
                    continue;
                }
                if (bindSelected(c)) {
                    room.join({*ip, quint16(c["port"].toInt(32190))}, now);
                    lastError.clear();
                }
            } else if (op == "search") {
                if (bindSelected(c)) {
                    QList<Endpoint> ds{{0xffffffff, 32189}};
                    if (selected.prefix > 0 && selected.prefix < 32) {
                        quint32 mask = 0xffffffffu << (32 - selected.prefix);
                        ds.append({selected.ip | ~mask, 32189});
                    }
                    room.discover(ds, now);
                    log("Búsqueda UDP en la VPN. Si no admite broadcast, use conexión directa.");
                }
            } else if (op == "leave") {
                localAccess.enable(false);
                room.leave();
                captures.clear();
            } else if (op == "add")
                room.addConsole(c["name"].toString(), c["adapter"].toString());
            else if (op == "remove")
                room.removeConsole(c["key"].toString());
            else if (op == "release")
                room.releaseReservation(c["owner"].toString(), c["key"].toString());
            else if (op == "adapter")
                room.setAdapter(c["key"].toString(), c["adapter"].toString());
            else if (op == "resetMac")
                room.forgetMac(c["key"].toString());
        }
        // Bounded polling keeps command processing and shutdown responsive under load.
        receiveBudget = 256;
        while (receiveBudget--) {
            auto d = udp.receive();
            if (!d)
                break;
            room.receive(d->endpoint, d->bytes, now);
        }
        for (int i = 0; i < 32; ++i) {
            auto d = discovery.receive();
            if (!d)
                break;
            auto p = decode(d->bytes);
            if (p && p->h.type == Type::Discover)
                room.receive(d->endpoint, d->bytes, now);
        }
        room.tick(now);
        if (udp.isOpen() && now - lastAdapters > 2000) {
            lastAdapters = now;
            auto all = adapters();
            bool present = std::any_of(all.begin(), all.end(), [&](const Adapter &a) {
                return a.up && a.id == selected.id && a.ip == selected.ip;
            });
            if (!present) {
                localAccess.enable(false);
                room.leave();
                captures.clear();
                flush();
                udp.close();
                discovery.close();
                lastError = "La VPN se desconectó o cambió de IP. Reenvío detenido.";
                log(lastError);
            }
        }
        if (room.connected() && now - lastCaps > 5000) {
            lastCaps = now;
            captureErrors.clear();
            auto all = adapters();
            std::set<QString> wanted;
            for (auto &d : room.remembered()) {
                if (!d.adapter.isEmpty())
                    wanted.insert(d.adapter);
                else
                    for (auto &a : all)
                        if (a.physical && a.up && a.id != selected.id)
                            wanted.insert(a.id);
            }
            for (auto it = captures.begin(); it != captures.end();)
                if (!wanted.contains(it->first))
                    it = captures.erase(it);
                else
                    ++it;
            for (auto &id : wanted) {
                if (id == selected.id) {
                    captureErrors << "VPN y Ethernet no pueden ser la misma interfaz.";
                    continue;
                }
                auto interface = std::find_if(all.begin(), all.end(),
                                              [&](const Adapter &a) { return a.id == id && a.up; });
                if (interface == all.end()) {
                    captures.erase(id);
                    captureErrors << "Ethernet ausente o desconectada: " + id;
                    continue;
                }
                if (!validMac(interface->mac)) {
                    captures.erase(id);
                    captureErrors << "No se pudo identificar la MAC de la Ethernet: " + id;
                    continue;
                }
                captureMacs[id] = interface->mac;
                if (captures.contains(id))
                    continue;
                auto cap = std::make_unique<Capture>();
                if (!cap->open(id, roomCaptureFilter())) {
                    captureErrors << id + ": " + cap->error;
                    continue;
                }
                log("Captura promiscua Ethernet abierta: " + id);
                captures.emplace(id, std::move(cap));
            }
            for (auto &e : captureErrors)
                if (lastError != e) {
                    lastError = e;
                    log(e);
                }
        }
        if (!room.connected())
            captures.clear();
        QStringList failedCaptures;
        for (auto &[id, cap] : captures) {
            for (int i = 0; i < 128; ++i) {
                auto b = cap->receive();
                if (!b)
                    break;
                ++observed;
                if (auto other = beaconNode(*b)) {
                    if (*other != room.node) {
                        if (blocked[id] < now)
                            log("ERROR: otro PC ConsoleLAN está en esta LAN física. Reenvío Ethernet "
                                "bloqueado en " +
                                id);
                        blocked[id] = now + 6500;
                    }
                    continue;
                }
                if (blocked[id] > now)
                    continue;
                room.captured(id, *b, now, captureMacs.at(id));
            }
            if (now - lastBeacon >= 2000)
                cap->inject(bridgeBeacon(room.node));
            if (!cap->error.isEmpty()) {
                failedCaptures << id;
                captureErrors << id + ": " + cap->error;
                if (lastError != cap->error) {
                    lastError = cap->error;
                    log("Se reabrirá la captura Ethernet: " + lastError);
                }
            }
        }
        for (auto &id : failedCaptures)
            captures.erase(id);
        if (now - lastBeacon >= 2000)
            lastBeacon = now;
        flush();
        for (auto &i : room.injections) {
            auto it = captures.find(i.adapter);
            if (it != captures.end() && blocked[i.adapter] <= now) {
                if (it->second->inject(i.bytes))
                    ++injected;
                else {
                    ++sendErrors;
                    if (lastError != it->second->error) {
                        lastError = it->second->error;
                        log(lastError);
                    }
                }
            }
        }
        room.injections.clear();
        for (auto &line : room.logs)
            log(line);
        room.logs.clear();
        if (now - lastView > 250) {
            lastView = now;
            auto v = room.view();
            v["bound"] = udp.isOpen() ? udp.bound.text() : QString("Sin socket VPN");
            v["npcap"] = Capture::version();
            v["observed"] = double(observed);
            v["injected"] = double(injected);
            v["sendErrors"] = double(sendErrors);
            v["error"] = lastError;
            v["captureErrors"] = QJsonArray::fromStringList(captureErrors);
            QJsonArray caps;
            for (auto &[id, _] : captures)
                caps.append(QJsonObject{{"adapter", id}, {"blocked", blocked[id] > now}});
            v["captures"] = caps;
            v["dataDir"] = dataDir_;
            localAccess.update(v);
            v["localAccess"] = localAccess.status();
            emit updated(v);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    room.leave();
    localAccess.enable(false);
    flush();
    captures.clear();
    log("Cierre limpio: sockets y capturas liberados.");
}
} // namespace cl
