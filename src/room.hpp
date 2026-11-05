#pragma once
#include "protocol.hpp"
#include "ethernet.hpp"
#include "store.hpp"
namespace cl {
struct Datagram {
    Endpoint endpoint;
    QByteArray bytes;
};
struct Injection {
    QString adapter;
    QByteArray bytes;
};
struct Offer {
    Endpoint endpoint;
    Id room = 0;
    QString name;
    int members = 0;
    qint64 at = 0;
};
struct Member {
    Id node = 0, token = 0;
    QString owner, name;
    Endpoint endpoint;
    qint64 seen = 0;
};
class Room {
    struct Pending {
        Id node = 0, nonce = 0, challenge = 0;
        QString owner, name;
        qint64 at = 0;
    };
    Store *store_;
    std::map<Endpoint, Pending> pending_;
    std::map<Id, Member> members_;
    std::map<QString, Id> owners_;
    QList<Console> consoles_, desired_;
    QList<Console> ledger_;
    std::set<QString> ambiguous_;
    std::set<QString> observedLocal_;
    std::map<QString, QString> localConflicts_;
    std::map<std::pair<Endpoint, Id>, Id> linkSequences_;
    std::map<Id, std::pair<qint64, int>> flood_;
    std::map<std::tuple<Id, quint32, quint32, quint16>, qint64> udpFragments_;
    Id revision_ = 0, appliedRevision_ = 0;
    Reassembler assembler_;
    SequenceLoss loss_;
    Id room_ = 0, session_ = 0, nonce_ = 0, challenge_ = 0, serial_ = 0, dataSerial_ = 0;
    Id masterNode_ = 0;
    Endpoint master_;
    quint32 subnet_ = 0;
    QString name_, mode_ = "offline", status_ = "Sin sala", owner_, nodeName_;
    qint64 lastMaster_ = 0, lastTick_ = 0, joinAt_ = 0, now_ = 0, lastUnknown_ = 0;
    int budget_ = 1100, rtt_ = -1;
    Id ping_ = 0;
    qint64 pingAt_ = 0;
    bool lastConnected_ = false;
    void send(Endpoint ep, Type type, const QJsonObject &object, Id token = 0, Id room = 0);
    void sendBytes(Endpoint ep, Type type, const QByteArray &b, Id token, Id origin, Id seq = 0);
    void handle(Endpoint ep, const Packet &packet);
    void snapshot();
    QJsonObject state() const;
    void applyState(const QJsonObject &o);
    void applyDesired(const Member &m, const QJsonArray &a);
    void saveDesired();
    void saveLedger();
    void relay(Id origin, Id seq, const QByteArray &b);
    void injectLocal(const QByteArray &b);
    void errorTo(const Member &m, const QString &text);
    void cleanupMember(const Member &member);
    bool allowFlood(Id origin, const QByteArray &bytes);
    bool allowUdpFragment(Id origin, const QByteArray &bytes);

  public:
    Id node = randomId();
    std::function<QString(quint32)> validateSubnet;
    QList<Datagram> outbound;
    QList<Injection> injections;
    QStringList logs;
    std::map<Endpoint, Offer> offers;
    quint64 txFrames = 0, rxFrames = 0, filtered = 0, invalid = 0, queueDrops = 0;
    explicit Room(Store *store = nullptr);
    QString owner() const {
        return owner_;
    }
    QString mode() const {
        return mode_;
    }
    bool connected() const {
        return mode_ == "host" || mode_ == "client";
    }
    const QList<Console> &consoles() const {
        return consoles_;
    }
    const QList<Console> &remembered() const {
        return desired_;
    }
    void setBudget(int n) {
        budget_ = std::clamp(n, 512, 1400);
    }
    void setNodeName(QString n) {
        nodeName_ = cleanName(n);
    }
    void host(QString name, quint32 subnet, qint64 now);
    void join(Endpoint master, qint64 now);
    void leave();
    void discover(const QList<Endpoint> &destinations, qint64 now);
    void receive(Endpoint ep, const QByteArray &b, qint64 now);
    void tick(qint64 now);
    QString addConsole(QString name, QString adapter = {});
    void removeConsole(const QString &key);
    void releaseReservation(const QString &owner, const QString &key);
    void setAdapter(const QString &key, const QString &adapter);
    void forgetMac(const QString &key);
    void captured(const QString &adapter, const QByteArray &b, qint64 now);
    QJsonObject view() const;
};
} // namespace cl
