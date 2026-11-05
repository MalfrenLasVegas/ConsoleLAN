#pragma once
#include "common.hpp"
namespace cl {
enum class Type : quint8 {
    Discover = 1,
    Offer,
    Hello,
    Challenge,
    Confirm,
    Welcome,
    State,
    Desired,
    Ping,
    Pong,
    Leave,
    Error,
    Frame
};
constexpr int HeaderSize = 48, MaxMessage = 65535, MaxDatagram = 1400;
struct Header {
    Type type = Type::Frame;
    Id room = 0, session = 0, node = 0, frame = 0;
    quint16 total = 0;
    quint8 index = 0, count = 1;
};
struct Packet {
    Header h;
    QByteArray payload;
};
QByteArray encode(const Packet &p);
std::optional<Packet> decode(const QByteArray &b);
QList<QByteArray> fragment(Header h, const QByteArray &payload, int budget);
class Reassembler {
    struct Key {
        Endpoint ep;
        Id room, session, node, frame;
        Type type;
        auto operator<=>(const Key &) const = default;
    };
    struct Partial {
        Header h;
        qint64 at;
        QList<QByteArray> parts;
        int bytes = 0;
    };
    std::map<Key, Partial> pending;
    std::map<Key, qint64> completed;

  public:
    quint64 expired = 0, rejected = 0, duplicates = 0;
    std::optional<Packet> accept(Endpoint ep, const Packet &p, qint64 now);
    void expire(qint64 now);
    void clear() {
        pending.clear();
        completed.clear();
    }
    size_t size() const {
        return pending.size();
    }
};
// Counts gaps only after a two-second reorder window; it is a protocol estimate.
class SequenceLoss {
    struct Track {
        Id high = 0;
        std::map<Id, qint64> missing;
    };
    std::map<Id, Track> tracks;

  public:
    quint64 lost = 0;
    void see(Id node, Id seq, qint64 now);
    void tick(qint64 now);
    void clear() {
        tracks.clear();
        lost = 0;
    }
    void forget(Id node) {
        tracks.erase(node);
    }
};
} // namespace cl
