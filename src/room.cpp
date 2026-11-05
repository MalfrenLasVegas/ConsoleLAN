#include "room.hpp"
namespace cl {
static QJsonArray arrayOf(const QList<Console> &cs) {
    QJsonArray a;
    for (auto &c : cs)
        a.append(consoleJson(c));
    return a;
}
Room::Room(Store *store) : store_(store) {
    owner_ = store ? store->data["owner"].toString() : QString();
    if (owner_.isEmpty())
        owner_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    nodeName_ = QSysInfo::machineHostName();
    if (store) {
        for (auto v : store->data["consoles"].toArray()) {
            auto c = consoleFromJson(v.toObject());
            if (!c.key.isEmpty()) {
                c.owner = owner_;
                desired_.append(c);
            }
        }
        store->data["owner"] = owner_;
        store->save();
    }
}
void Room::send(Endpoint ep, Type t, const QJsonObject &o, Id token, Id room) {
    Header h{t, room ? room : room_, token, node, ++serial_};
    for (auto &b : fragment(h, jsonBytes(o), budget_)) {
        if (outbound.size() < 2048)
            outbound.append({ep, b});
        else
            ++queueDrops;
    }
}
void Room::sendBytes(Endpoint ep, Type t, const QByteArray &b, Id token, Id origin, Id seq) {
    if (t == Type::Frame) {
        if (linkSequences_.size() >= 1024 && !linkSequences_.contains({ep, origin}))
            return;
        seq = ++linkSequences_[{ep, origin}];
    }
    Header h{t, room_, token, origin, seq ? seq : ++serial_};
    for (auto &d : fragment(h, b, budget_)) {
        if (outbound.size() < 2048)
            outbound.append({ep, d});
        else
            ++queueDrops;
    }
}
void Room::host(QString name, quint32 subnet, qint64 now) {
    leave();
    now_ = now;
    subnet_ = subnet;
    name_ = cleanName(name);
    room_ = randomId();
    if (store_) {
        auto h = store_->data["host"].toObject();
        if (parseIp(h["subnet"].toString()).value_or(0) == subnet_) {
            room_ = id(h["room"]);
            if (!room_)
                room_ = randomId();
            for (auto v : h["ledger"].toArray())
                ledger_.append(consoleFromJson(v.toObject()));
        }
    }
    mode_ = "host";
    status_ = "Anfitrión activo · conectividad de consolas pendiente de prueba";
    masterNode_ = node;
    members_[node] = {node, 0, owner_, nodeName_, {}, now};
    owners_[owner_] = node;
    applyDesired(members_.at(node), arrayOf(desired_));
    saveLedger();
    lastTick_ = now - 1000;
    logs << "Sala creada: " + name_ + " / " + hex(room_);
}
void Room::join(Endpoint master, qint64 now) {
    leave();
    now_ = now;
    mode_ = "joining";
    master_ = master;
    nonce_ = randomId();
    joinAt_ = now;
    lastTick_ = now - 1000;
    status_ = "Negociando con " + master.text();
}
void Room::leave() {
    outbound.clear();
    if (mode_ == "client")
        send(master_, Type::Leave, {{"bye", true}}, session_);
    if (mode_ == "host")
        for (auto &[n, m] : members_)
            if (n != node)
                send(m.endpoint, Type::Leave, {{"bye", true}}, m.token);
    mode_ = "offline";
    status_ = "Sin sala";
    members_.clear();
    owners_.clear();
    consoles_.clear();
    ledger_.clear();
    pending_.clear();
    assembler_.clear();
    loss_.clear();
    linkSequences_.clear();
    flood_.clear();
    udpFragments_.clear();
    ambiguous_.clear();
    observedLocal_.clear();
    localConflicts_.clear();
    revision_ = appliedRevision_ = 0;
    room_ = session_ = challenge_ = masterNode_ = 0;
    rtt_ = -1;
    injections.clear();
}
void Room::discover(const QList<Endpoint> &ds, qint64 now) {
    now_ = now;
    for (auto ep : ds)
        send(ep, Type::Discover, {{"probe", hex(randomId())}});
}
void Room::receive(Endpoint ep, const QByteArray &b, qint64 now) {
    now_ = now;
    auto p = decode(b);
    if (!p) {
        ++invalid;
        return;
    }
    auto t = p->h.type;
    if (t == Type::Frame) {
        bool allowed = false;
        if (mode_ == "host") {
            auto it = members_.find(p->h.node);
            allowed = it != members_.end() && it->first != node && it->second.endpoint == ep &&
                      it->second.token == p->h.session && p->h.room == room_;
        }
        if (mode_ == "client")
            allowed = ep == master_ && p->h.session == session_ && p->h.room == room_ && p->h.node != node &&
                      members_.contains(p->h.node);
        if (!allowed) {
            ++invalid;
            return;
        }
    } else if (t == Type::Welcome) {
        if (mode_ != "joining" || ep != master_ || !p->h.session) {
            ++invalid;
            return;
        }
    } else if (t == Type::Error && mode_ == "joining" && ep == master_) {
        if (p->h.count != 1)
            return;
    } else if (t != Type::Discover && t != Type::Offer && t != Type::Hello && t != Type::Challenge &&
               t != Type::Confirm) {
        bool allowed = false;
        if (mode_ == "host") {
            auto it = members_.find(p->h.node);
            allowed = it != members_.end() && it->first != node && it->second.endpoint == ep &&
                      it->second.token == p->h.session && p->h.room == room_;
        }
        if (mode_ == "client")
            allowed =
                ep == master_ && p->h.session == session_ && p->h.room == room_ && p->h.node == masterNode_;
        if (!allowed) {
            ++invalid;
            return;
        }
    } else if (p->h.count != 1) {
        ++invalid;
        return;
    }
    auto complete = assembler_.accept(ep, *p, now);
    if (complete)
        handle(ep, *complete);
}
void Room::handle(Endpoint ep, const Packet &p) {
    auto t = p.h.type;
    auto o = t == Type::Frame ? QJsonObject() : jsonObject(p.payload);
    if (t != Type::Frame && o.isEmpty()) {
        ++invalid;
        return;
    }
    if (t == Type::Error && mode_ == "joining" && ep == master_ && id(o["nonce"]) == nonce_) {
        leave();
        status_ = o["error"].toString().left(256);
        logs << status_;
        return;
    }
    if (t == Type::Discover) {
        if (mode_ == "host")
            send(ep, Type::Offer,
                 {{"name", name_}, {"members", int(members_.size())}, {"subnet", ipText(subnet_)}}, 0);
        return;
    }
    if (t == Type::Offer) {
        if (offers.size() < 64 || offers.contains(ep))
            offers[ep] = {ep, p.h.room, cleanName(o["name"].toString()), o["members"].toInt(), now_};
        return;
    }
    if (t == Type::Hello && mode_ == "host") {
        if (p.h.room && p.h.room != room_)
            return;
        auto own = o["owner"].toString();
        Id nonce = id(o["nonce"]);
        if (!p.h.node || p.h.node == node || !nonce || own.size() < 8 || own.size() > 64 ||
            members_.size() >= 32)
            return;
        if (members_.contains(p.h.node)) {
            auto &known = members_.at(p.h.node);
            if (known.owner != own || known.endpoint != ep) {
                send(ep, Type::Error,
                     {{"error", "Identificador de nodo ya ocupado."}, {"nonce", hex(nonce)}});
                return;
            }
        }
        if (owners_.contains(own)) {
            auto &m = members_.at(owners_.at(own));
            if (m.node == p.h.node && m.endpoint == ep) {
                send(ep, Type::Welcome, {{"nonce", hex(nonce)}, {"token", hex(m.token)}, {"state", state()}},
                     m.token);
                return;
            }
            send(ep, Type::Error,
                 {{"error", "Identidad duplicada: otro proceso o PC usa la misma persistencia."},
                  {"nonce", hex(nonce)}});
            return;
        }
        if (pending_.size() >= 64 && !pending_.contains(ep))
            return;
        auto it = pending_.find(ep);
        if (it == pending_.end() || it->second.node != p.h.node || it->second.nonce != nonce)
            pending_[ep] = {p.h.node, nonce, randomId(), own, cleanName(o["name"].toString()), now_};
        auto &a = pending_.at(ep);
        send(ep, Type::Challenge, {{"nonce", hex(a.nonce)}, {"challenge", hex(a.challenge)}});
        return;
    }
    if (t == Type::Challenge && mode_ == "joining" && ep == master_ && id(o["nonce"]) == nonce_ && p.h.room &&
        p.h.node) {
        challenge_ = id(o["challenge"]);
        room_ = p.h.room;
        masterNode_ = p.h.node;
        send(ep, Type::Confirm, {{"nonce", hex(nonce_)}, {"challenge", hex(challenge_)}});
        return;
    }
    if (t == Type::Confirm && mode_ == "host") {
        auto it = pending_.find(ep);
        if (it == pending_.end())
            return;
        auto a = it->second;
        if (p.h.room != room_ || p.h.node != a.node || id(o["nonce"]) != a.nonce ||
            id(o["challenge"]) != a.challenge || now_ - a.at > 5000 || owners_.contains(a.owner) ||
            members_.contains(a.node))
            return;
        Member m{a.node, randomId(), a.owner, a.name, ep, now_};
        members_[m.node] = m;
        owners_[m.owner] = m.node;
        pending_.erase(it);
        send(ep, Type::Welcome, {{"nonce", hex(a.nonce)}, {"token", hex(m.token)}, {"state", state()}},
             m.token);
        logs << "Participante conectado: " + m.name;
        snapshot();
        return;
    }
    if (t == Type::Welcome && mode_ == "joining" && ep == master_ && id(o["nonce"]) == nonce_ &&
        id(o["token"]) && p.h.room) {
        session_ = id(o["token"]);
        if (p.h.session != session_)
            return;
        room_ = p.h.room;
        masterNode_ = p.h.node;
        mode_ = "client";
        lastMaster_ = now_;
        status_ = "Conectado al maestro · conectividad de consolas pendiente de prueba";
        applyState(o["state"].toObject());
        lastTick_ = now_ - 1000;
        return;
    }
    if (t == Type::Frame) {
        if (mode_ == "host")
            members_[p.h.node].seen = now_;
        else
            lastMaster_ = now_;
        loss_.see(p.h.node, p.h.frame, now_);
        relay(p.h.node, p.h.frame, p.payload);
        return;
    }
    if (mode_ == "host") {
        auto it = members_.find(p.h.node);
        if (it == members_.end())
            return;
        auto m = it->second;
        it->second.seen = now_;
        if (t == Type::Desired) {
            applyDesired(m, o["consoles"].toArray());
            snapshot();
        }
        if (t == Type::Ping)
            send(ep, Type::Pong, {{"ping", o["ping"]}}, m.token);
        if (t == Type::Leave) {
            cleanupMember(m);
            owners_.erase(m.owner);
            members_.erase(m.node);
            consoles_.erase(std::remove_if(consoles_.begin(), consoles_.end(),
                                           [&](const Console &c) { return c.owner == m.owner; }),
                            consoles_.end());
            logs << "Participante salió: " + m.name;
            snapshot();
        }
    } else if (mode_ == "client") {
        lastMaster_ = now_;
        if (t == Type::State)
            applyState(o);
        if (t == Type::Pong && id(o["ping"]) == ping_)
            rtt_ = int(now_ - pingAt_);
        if (t == Type::Error) {
            status_ = o["error"].toString().left(256);
            logs << status_;
        }
        if (t == Type::Leave) {
            leave();
            status_ = "El maestro cerró la sala. Reenvío detenido; puede reconectar.";
            logs << status_;
        }
    }
}
QJsonObject Room::state() const {
    QJsonArray a;
    for (auto &[_, m] : members_)
        a.append(QJsonObject{{"node", hex(m.node)}, {"owner", m.owner}, {"name", m.name}});
    return {{"name", name_},
            {"subnet", ipText(subnet_)},
            {"members", a},
            {"consoles", arrayOf(consoles_)},
            {"revision", hex(revision_)}};
}
void Room::snapshot() {
    if (mode_ != "host")
        return;
    ++revision_;
    auto s = state();
    for (auto &[n, m] : members_)
        if (n != node)
            send(m.endpoint, Type::State, s, m.token);
    applyState(s);
}
void Room::applyState(const QJsonObject &s) {
    if (id(s["revision"]) < appliedRevision_)
        return;
    auto subnet = parseIp(s["subnet"].toString());
    if (!subnet || (*subnet & 255) || s["members"].toArray().size() > 32 ||
        s["consoles"].toArray().size() > 64)
        return;
    if (mode_ == "client" && validateSubnet && (subnet_ != *subnet || owners_.empty())) {
        auto conflict = validateSubnet(*subnet);
        if (!conflict.isEmpty()) {
            leave();
            status_ = "Subred de sala incompatible con esta sede: " + conflict;
            logs << status_;
            return;
        }
    }
    std::map<QString, Id> os;
    std::map<Id, Member> ms;
    for (auto v : s["members"].toArray()) {
        auto o = v.toObject();
        auto n = id(o["node"]);
        auto own = o["owner"].toString();
        if (!n || os.contains(own) || ms.contains(n))
            return;
        os[own] = n;
        ms[n] = {n, 0, own, cleanName(o["name"].toString()), {}, now_};
    }
    QList<Console> cs;
    std::set<quint32> ips;
    std::set<QString> macs, keys;
    for (auto v : s["consoles"].toArray()) {
        auto c = consoleFromJson(v.toObject());
        auto k = c.owner + "/" + c.key;
        if (!os.contains(c.owner) || keys.contains(k) || ips.contains(c.ip) || c.ip <= *subnet + 1 ||
            c.ip >= *subnet + 255 || (!c.mac.isEmpty() && (!validMac(c.mac) || macs.contains(c.mac))))
            return;
        keys.insert(k);
        ips.insert(c.ip);
        if (!c.mac.isEmpty())
            macs.insert(c.mac);
        cs.append(c);
    }
    appliedRevision_ = id(s["revision"]);
    subnet_ = *subnet;
    name_ = cleanName(s["name"].toString());
    consoles_ = cs;
    owners_ = os;
    if (mode_ == "client") {
        for (auto &[n, oldMember] : members_)
            if (!ms.contains(n))
                cleanupMember(oldMember);
        members_ = ms;
    }
    bool changed = false;
    for (auto &c : consoles_)
        if (c.owner == owner_)
            for (auto &d : desired_)
                if (d.key == c.key) {
                    if (d.ip != c.ip) {
                        if (d.ip)
                            logs << QString("Reconfigurar %1: %2 → %3")
                                        .arg(d.name, ipText(d.ip), ipText(c.ip));
                        d.ip = c.ip;
                        changed = true;
                    }
                    if (!c.mac.isEmpty() && d.mac != c.mac) {
                        d.mac = c.mac;
                        changed = true;
                    }
                }
    if (changed)
        saveDesired();
}
void Room::errorTo(const Member &m, const QString &text) {
    if (m.node == node) {
        status_ = text;
        logs << text;
    } else
        send(m.endpoint, Type::Error, {{"error", text}}, m.token);
}
void Room::cleanupMember(const Member &m) {
    for (auto it = linkSequences_.begin(); it != linkSequences_.end();)
        if (it->first.first == m.endpoint || it->first.second == m.node)
            it = linkSequences_.erase(it);
        else
            ++it;
    for (auto it = udpFragments_.begin(); it != udpFragments_.end();)
        if (std::get<0>(it->first) == m.node)
            it = udpFragments_.erase(it);
        else
            ++it;
    flood_.erase(m.node);
    loss_.forget(m.node);
}
void Room::applyDesired(const Member &m, const QJsonArray &a) {
    if (a.size() > 16) {
        errorTo(m, "Máximo 16 consolas por PC.");
        return;
    }
    std::set<QString> keys;
    QList<Console> requests;
    for (auto v : a) {
        auto c = consoleFromJson(v.toObject());
        if (c.key.size() < 8 || c.key.size() > 64 || c.adapter.size() > 64 || keys.contains(c.key) ||
            c.name.isEmpty() || (!c.mac.isEmpty() && !validMac(c.mac))) {
            errorTo(m, "Solicitud de consolas inválida.");
            return;
        }
        keys.insert(c.key);
        c.owner = m.owner;
        requests.append(c);
    }
    auto before = jsonBytes({{"ledger", arrayOf(ledger_)}});
    ledger_.erase(
        std::remove_if(ledger_.begin(), ledger_.end(),
                       [&](const Console &c) { return c.owner == m.owner && !keys.contains(c.key); }),
        ledger_.end());
    consoles_.erase(std::remove_if(consoles_.begin(), consoles_.end(),
                                   [&](const Console &c) { return c.owner == m.owner; }),
                    consoles_.end());
    for (auto request : requests) {
        if (consoles_.size() >= 64) {
            errorTo(m, "Máximo 64 consolas activas por sala.");
            break;
        }
        Console *existing = nullptr;
        for (auto &c : ledger_)
            if (c.owner == m.owner && c.key == request.key)
                existing = &c;
        Console c = existing ? *existing : request;
        c.owner = m.owner;
        c.name = request.name;
        c.adapter = request.adapter;
        c.manualAdapter = request.manualAdapter;
        c.status = "Esperando tráfico con IP reservada";
        if (!existing) {
            auto used = [&](quint32 ip) {
                return std::any_of(ledger_.begin(), ledger_.end(),
                                   [&](const Console &v) { return v.ip == ip; });
            };
            if (c.ip <= subnet_ + 1 || c.ip >= subnet_ + 255 || used(c.ip)) {
                c.ip = 0;
                for (quint32 ip = subnet_ + 2; ip < subnet_ + 255; ++ip)
                    if (!used(ip)) {
                        c.ip = ip;
                        break;
                    }
            }
            c.mac.clear();
            if (!c.ip || ledger_.size() >= 128) {
                errorTo(m, "No quedan reservas libres en la sala.");
                continue;
            }
        }
        if (!request.mac.isEmpty()) {
            bool duplicate = std::any_of(ledger_.begin(), ledger_.end(), [&](const Console &other) {
                return other.mac == request.mac && (other.owner != c.owner || other.key != c.key);
            });
            if (duplicate || (!c.mac.isEmpty() && c.mac != request.mac))
                c.status = "CONFLICTO MAC: elimine o revise el registro anterior";
            else {
                c.mac = request.mac;
                c.status = "MAC registrada · conectividad sin verificar";
            }
        } else if (!c.mac.isEmpty())
            c.status = "MAC recordada · esperando observación en esta sesión";
        if (existing)
            *existing = c;
        else
            ledger_.append(c);
        consoles_.append(c);
    }
    if (before != jsonBytes({{"ledger", arrayOf(ledger_)}}))
        saveLedger();
}
void Room::saveLedger() {
    if (!store_ || mode_ != "host")
        return;
    store_->data["host"] = QJsonObject{
        {"room", hex(room_)}, {"name", name_}, {"subnet", ipText(subnet_)}, {"ledger", arrayOf(ledger_)}};
    if (!store_->save())
        logs << "Error guardando reservas: " + store_->error;
}
void Room::saveDesired() {
    if (store_) {
        store_->data["consoles"] = arrayOf(desired_);
        if (!store_->save())
            logs << "Error guardando consolas: " + store_->error;
    }
}
QString Room::addConsole(QString name, QString adapter) {
    if (desired_.size() >= 16)
        return {};
    Console c{QUuid::createUuid().toString(QUuid::WithoutBraces),
              cleanName(name),
              owner_,
              {},
              adapter,
              0,
              {},
              !adapter.isEmpty()};
    if (c.name.isEmpty())
        return {};
    desired_.append(c);
    saveDesired();
    lastTick_ = now_ - 1000;
    return c.key;
}
void Room::removeConsole(const QString &key) {
    desired_.erase(
        std::remove_if(desired_.begin(), desired_.end(), [&](const Console &c) { return c.key == key; }),
        desired_.end());
    saveDesired();
    lastTick_ = now_ - 1000;
}
void Room::releaseReservation(const QString &owner, const QString &key) {
    if (mode_ != "host" || owners_.contains(owner)) {
        logs << "Solo pueden liberarse reservas de sedes desconectadas; las activas se eliminan en su PC.";
        return;
    }
    ledger_.erase(std::remove_if(ledger_.begin(), ledger_.end(),
                                 [&](const Console &c) { return c.owner == owner && c.key == key; }),
                  ledger_.end());
    saveLedger();
    logs << "Reserva desconectada liberada por el maestro: " + key;
}
void Room::setAdapter(const QString &key, const QString &adapter) {
    for (auto &c : desired_)
        if (c.key == key) {
            c.adapter = adapter;
            c.manualAdapter = !adapter.isEmpty();
        }
    ambiguous_.erase(key);
    saveDesired();
    lastTick_ = now_ - 1000;
}
void Room::forgetMac(const QString &key) {
    // A changed MAC requires a new reservation identity; never overwrite the old association.
    for (auto &c : desired_)
        if (c.key == key) {
            c.key = QUuid::createUuid().toString(QUuid::WithoutBraces);
            c.mac.clear();
        }
    saveDesired();
    lastTick_ = now_ - 1000;
}
void Room::injectLocal(const QByteArray &b) {
    auto f = inspect(b);
    if (!f)
        return;
    std::set<QString> targets;
    for (auto &c : consoles_)
        if (c.owner == owner_) {
            auto it = std::find_if(desired_.begin(), desired_.end(),
                                   [&](const Console &d) { return d.key == c.key; });
            if (it != desired_.end() && !it->adapter.isEmpty() &&
                (f->group || c.mac.isEmpty() || c.mac == f->destination))
                targets.insert(it->adapter);
        }
    // Unknown unicast must reach all local console interfaces as well.
    bool known = std::any_of(consoles_.begin(), consoles_.end(),
                             [&](const Console &c) { return c.mac == f->destination; });
    if (!known)
        for (auto &d : desired_)
            if (!d.adapter.isEmpty())
                targets.insert(d.adapter);
    for (auto &adapter : targets)
        if (injections.size() < 256)
            injections.append({adapter, b});
        else
            ++queueDrops;
}
void Room::relay(Id origin, Id seq, const QByteArray &b) {
    auto route = routeFrame(b, origin, node, consoles_, owners_);
    if (!route.accepted) {
        ++filtered;
        return;
    }
    if (!allowFlood(origin, b) || !allowUdpFragment(origin, b)) {
        ++filtered;
        return;
    }
    ++rxFrames;
    if (route.inject)
        injectLocal(b);
    if (mode_ == "host")
        for (Id n : route.nodes)
            if (members_.contains(n))
                sendBytes(members_.at(n).endpoint, Type::Frame, b, members_.at(n).token, origin, seq);
}
void Room::captured(const QString &adapter, const QByteArray &b, qint64 now) {
    now_ = now;
    if (!connected())
        return;
    auto f = inspect(b);
    if (!f) {
        ++filtered;
        return;
    }
    // Learning uses IPv4 source / ARP sender IP only. Target IP never proves identity.
    for (auto &d : desired_) {
        auto it = std::find_if(consoles_.begin(), consoles_.end(),
                               [&](const Console &c) { return c.owner == owner_ && c.key == d.key; });
        if (it == consoles_.end() || it->ip != f->sourceIp)
            continue;
        bool remote = std::any_of(consoles_.begin(), consoles_.end(),
                                  [&](const Console &c) { return c.owner != owner_ && c.mac == f->source; });
        if (remote) {
            ++filtered;
            return;
        }
        if (!d.mac.isEmpty() && d.mac != f->source) {
            if (!localConflicts_.contains(d.key)) {
                localConflicts_[d.key] = "CONFLICTO IP/MAC: revise la consola o use Reaprender MAC";
                logs << "CONFLICTO: otra MAC emite con la IP " + ipText(it->ip);
            }
            ++filtered;
            return;
        }
        if (!d.adapter.isEmpty() && d.adapter != adapter) {
            if (!d.manualAdapter && ambiguous_.insert(d.key).second)
                logs << "CONFLICTO: la consola " + d.name +
                            " aparece en otra Ethernet. Seleccione la interfaz manualmente.";
            ++filtered;
            return;
        }
        if (ambiguous_.contains(d.key)) {
            ++filtered;
            return;
        }
        if (localConflicts_.contains(d.key)) {
            ++filtered;
            return;
        }
        observedLocal_.insert(d.key);
        if (d.mac.isEmpty() || d.adapter.isEmpty()) {
            d.mac = f->source;
            d.adapter = adapter;
            saveDesired();
            lastTick_ = now_ - 1000;
            logs << "MAC aprendida por IP de origen: " + d.name + " = " + d.mac;
            if (mode_ == "host") {
                applyDesired(members_.at(node), arrayOf(desired_));
                snapshot();
            }
        }
        break;
    }
    auto r = routeFrame(b, node, node, consoles_, owners_);
    if (!r.accepted) {
        ++filtered;
        return;
    }
    if (!allowFlood(node, b) || !allowUdpFragment(node, b)) {
        ++filtered;
        return;
    }
    // No VPN copy for unicast addressed to another known console of this same PC.
    if (r.nodes.empty())
        return;
    Id seq = ++dataSerial_;
    ++txFrames;
    if (mode_ == "host") {
        for (auto n : r.nodes)
            if (members_.contains(n))
                sendBytes(members_.at(n).endpoint, Type::Frame, b, members_.at(n).token, node, seq);
    } else if (mode_ == "client")
        sendBytes(master_, Type::Frame, b, session_, node, seq);
}
bool Room::allowUdpFragment(Id origin, const QByteArray &b) {
    auto f = inspect(b);
    if (!f || !f->udp || (!f->moreFragments && !f->fragmentOffset))
        return true;
    auto key = std::tuple{origin, f->sourceIp, f->destIp, f->ipId};
    if (!f->fragmentOffset) {
        if (udpFragments_.size() >= 512 && !udpFragments_.contains(key))
            return false;
        udpFragments_[key] = now_;
        return true;
    }
    auto it = udpFragments_.find(key);
    return it != udpFragments_.end() && now_ - it->second <= 5000;
}
bool Room::allowFlood(Id origin, const QByteArray &b) {
    auto f = inspect(b);
    if (!f || f->group)
        return true;
    bool known = std::any_of(consoles_.begin(), consoles_.end(),
                             [&](const Console &c) { return !c.mac.isEmpty() && c.mac == f->destination; });
    if (known)
        return true;
    auto &bucket = flood_[origin];
    if (now_ - bucket.first >= 1000)
        bucket = {now_, 0};
    return ++bucket.second <= 200;
}
void Room::tick(qint64 now) {
    now_ = now;
    assembler_.expire(now);
    for (auto it = udpFragments_.begin(); it != udpFragments_.end();)
        if (now - it->second > 5000)
            it = udpFragments_.erase(it);
        else
            ++it;
    loss_.tick(now);
    for (auto it = offers.begin(); it != offers.end();)
        if (now - it->second.at > 5000)
            it = offers.erase(it);
        else
            ++it;
    for (auto it = pending_.begin(); it != pending_.end();)
        if (now - it->second.at > 5000)
            it = pending_.erase(it);
        else
            ++it;
    if (mode_ == "client" && now - lastMaster_ > 6500) {
        leave();
        status_ = "Maestro perdido: reenvío detenido. Use Reconectar.";
        logs << status_;
    }
    if (mode_ == "joining" && now - joinAt_ > 9000) {
        leave();
        status_ = "Sin respuesta al handshake. Compruebe VPN, IP y firewall.";
        logs << status_;
    }
    if (mode_ == "host") {
        for (auto it = members_.begin(); it != members_.end();)
            if (it->first != node && now - it->second.seen > 6500) {
                auto m = it->second;
                cleanupMember(m);
                logs << "Participante expirado: " + m.name;
                owners_.erase(m.owner);
                consoles_.erase(std::remove_if(consoles_.begin(), consoles_.end(),
                                               [&](const Console &c) { return c.owner == m.owner; }),
                                consoles_.end());
                it = members_.erase(it);
            } else
                ++it;
    }
    if (now - lastTick_ < 1000)
        return;
    lastTick_ = now;
    if (mode_ == "joining")
        send(master_, Type::Hello, {{"owner", owner_}, {"name", nodeName_}, {"nonce", hex(nonce_)}});
    if (mode_ == "client") {
        send(master_, Type::Desired, {{"consoles", arrayOf(desired_)}}, session_);
        ping_ = randomId();
        pingAt_ = now;
        send(master_, Type::Ping, {{"ping", hex(ping_)}}, session_);
    }
    if (mode_ == "host") {
        applyDesired(members_.at(node), arrayOf(desired_));
        snapshot();
    }
}
QJsonObject Room::view() const {
    auto v = state();
    v["mode"] = mode_;
    v["status"] = status_;
    v["owner"] = owner_;
    v["room"] = hex(room_);
    v["master"] = mode_ == "host" ? QString("Este PC") : master_.text();
    v["remembered"] = arrayOf(desired_);
    if (mode_ == "host") {
        QJsonArray reservations;
        for (auto &c : ledger_) {
            auto entry = consoleJson(c);
            entry["active"] = owners_.contains(c.owner);
            reservations.append(entry);
        }
        v["reservations"] = reservations;
    }
    auto cs = v["consoles"].toArray();
    for (int i = 0; i < cs.size(); ++i) {
        auto c = cs[i].toObject();
        auto key = c["key"].toString();
        if (c["owner"].toString() == owner_) {
            if (ambiguous_.contains(key))
                c["status"] = "CONFLICTO Ethernet: seleccione una interfaz manualmente";
            else if (localConflicts_.contains(key))
                c["status"] = localConflicts_.at(key);
            else if (!c["status"].toString().contains("CONFLICTO") && !c["mac"].toString().isEmpty())
                c["status"] = observedLocal_.contains(key) ? "Tráfico observado · conectividad sin verificar"
                                                           : "MAC recordada · esperando tráfico";
            cs[i] = c;
        }
    }
    v["consoles"] = cs;
    QJsonArray offersJson;
    int n = 0;
    for (auto &[_, o] : offers) {
        if (n++ >= 5)
            break;
        offersJson.append(QJsonObject{{"name", o.name},
                                      {"endpoint", o.endpoint.text()},
                                      {"ip", ipText(o.endpoint.ip)},
                                      {"port", o.endpoint.port},
                                      {"members", o.members},
                                      {"room", hex(o.room)}});
    }
    v["offers"] = offersJson;
    v["tx"] = double(txFrames);
    v["rx"] = double(rxFrames);
    v["filtered"] = double(filtered);
    v["invalid"] = double(invalid);
    v["incomplete"] = double(assembler_.expired);
    v["gaps"] = double(loss_.lost);
    v["queueDrops"] = double(queueDrops);
    v["rtt"] = rtt_;
    return v;
}
} // namespace cl
