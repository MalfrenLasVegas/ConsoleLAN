#include "protocol.hpp"
namespace cl {
static void put(QByteArray &b, quint64 v, int n) {
    for (int i = n - 1; i >= 0; --i)
        b.append(char(v >> (i * 8)));
}
static quint64 get(const QByteArray &b, int p, int n) {
    quint64 v = 0;
    for (int i = 0; i < n; ++i)
        v = (v << 8) | quint8(b[p + i]);
    return v;
}
QByteArray encode(const Packet &p) {
    QByteArray b("CLAN", 4);
    put(b, 1, 1);
    put(b, quint8(p.h.type), 1);
    put(b, HeaderSize, 2);
    for (auto v : {p.h.room, p.h.session, p.h.node, p.h.frame})
        put(b, v, 8);
    put(b, p.h.total, 2);
    put(b, p.h.index, 1);
    put(b, p.h.count, 1);
    put(b, p.payload.size(), 2);
    put(b, 0, 2);
    b += p.payload;
    return b;
}
std::optional<Packet> decode(const QByteArray &b) {
    if (b.size() < HeaderSize || b.size() > MaxDatagram || b.left(4) != "CLAN" || quint8(b[4]) != 1 ||
        be16(b, 6) != HeaderSize || be16(b, 46))
        return {};
    auto t = quint8(b[5]);
    if (t < 1 || t > quint8(Type::Frame))
        return {};
    Header h{Type(t),       get(b, 8, 8),           get(b, 16, 8), get(b, 24, 8),
             get(b, 32, 8), quint16(get(b, 40, 2)), quint8(b[42]), quint8(b[43])};
    if (!h.frame || !h.count || h.count > 192 || h.index >= h.count || !h.total ||
        be16(b, 44) != b.size() - HeaderSize || b.size() == HeaderSize || b.size() - HeaderSize > h.total)
        return {};
    if (h.count == 1 && (h.index || h.total != b.size() - HeaderSize))
        return {};
    return Packet{h, b.mid(HeaderSize)};
}
QList<QByteArray> fragment(Header h, const QByteArray &payload, int budget) {
    QList<QByteArray> out;
    budget = std::clamp(budget, 512, MaxDatagram);
    int cap = budget - HeaderSize;
    if (payload.isEmpty() || payload.size() > MaxMessage)
        return out;
    int n = (int(payload.size()) + cap - 1) / cap;
    if (n > 192)
        return out;
    h.count = quint8(n);
    h.total = quint16(payload.size());
    for (int i = 0; i < n; ++i) {
        h.index = quint8(i);
        out.push_back(encode({h, payload.mid(i * cap, cap)}));
    }
    return out;
}
void Reassembler::expire(qint64 now) {
    for (auto it = pending.begin(); it != pending.end();)
        if (now - it->second.at > 1200) {
            ++expired;
            it = pending.erase(it);
        } else
            ++it;
    for (auto it = completed.begin(); it != completed.end();)
        if (now - it->second > 10000)
            it = completed.erase(it);
        else
            ++it;
}
std::optional<Packet> Reassembler::accept(Endpoint ep, const Packet &p, qint64 now) {
    expire(now);
    Key k{ep, p.h.room, p.h.session, p.h.node, p.h.frame, p.h.type};
    if (completed.contains(k)) {
        ++duplicates;
        return {};
    }
    auto finish = [&](Packet v) -> std::optional<Packet> {
        if (completed.size() >= 4096)
            completed.erase(completed.begin());
        completed[k] = now;
        return v;
    };
    if (p.h.count == 1)
        return finish(p);
    if (!pending.contains(k)) {
        if (pending.size() >= 128) {
            ++rejected;
            return {};
        }
        pending.emplace(k, Partial{p.h, now, QList<QByteArray>(p.h.count), 0});
    }
    auto &a = pending.at(k);
    if (a.h.count != p.h.count || a.h.total != p.h.total) {
        pending.erase(k);
        ++rejected;
        return {};
    }
    auto &part = a.parts[p.h.index];
    if (!part.isEmpty()) {
        if (part != p.payload) {
            pending.erase(k);
            ++rejected;
        } else
            ++duplicates;
        return {};
    }
    part = p.payload;
    a.bytes += int(part.size());
    if (a.bytes > p.h.total) {
        pending.erase(k);
        ++rejected;
        return {};
    }
    for (auto &x : a.parts)
        if (x.isEmpty())
            return {};
    QByteArray b;
    for (auto &x : a.parts)
        b += x;
    pending.erase(k);
    if (b.size() != p.h.total) {
        ++rejected;
        return {};
    }
    return finish(Packet{p.h, b});
}
void SequenceLoss::see(Id node, Id seq, qint64 now) {
    auto &t = tracks[node];
    t.missing.erase(seq);
    if (seq > t.high) {
        if (t.high) {
            Id begin = std::max(t.high + 1, seq > 4096 ? seq - 4096 : Id(1));
            for (Id i = begin; i < seq; ++i)
                t.missing.emplace(i, now);
        }
        t.high = seq;
    }
    while (t.missing.size() > 4096)
        t.missing.erase(t.missing.begin());
}
void SequenceLoss::tick(qint64 now) {
    for (auto &[_, t] : tracks)
        for (auto it = t.missing.begin(); it != t.missing.end();)
            if (now - it->second > 2000) {
                ++lost;
                it = t.missing.erase(it);
            } else
                ++it;
}
} // namespace cl
