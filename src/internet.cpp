#include "internet.hpp"
#define LIBSLIRP_STATIC
#include <slirp/libslirp.h>
#include <chrono>
namespace cl {
struct InternetGateway::Impl {
    HMODULE dll = nullptr;
    bool winsock = false;
    Slirp *stack = nullptr;
    SlirpCb callbacks{};
    decltype(&slirp_new) create = nullptr;
    decltype(&slirp_cleanup) cleanup = nullptr;
    decltype(&slirp_input) input = nullptr;
    decltype(&slirp_pollfds_fill_socket) fill = nullptr;
    decltype(&slirp_pollfds_poll) poll = nullptr;
    AccessPlan plan;
    Output output;
    QByteArray internalMac;
    std::vector<WSAPOLLFD> sockets;
    struct Timer {
        SlirpTimerCb callback;
        void *opaque;
        qint64 expiry = -1;
    };
    std::set<Timer *> timers;
    quint64 sent = 0, received = 0;
#ifdef CONSOLELAN_NATIVE_TESTS
    bool loopback = false;
#endif
    static int64_t now(void *) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
    static slirp_ssize_t send(const void *data, size_t len, void *opaque) {
        auto &a = *static_cast<Impl *>(opaque);
        if (len < 14 || len > 1514)
            return slirp_ssize_t(len);
        QByteArray b(static_cast<const char *>(data), qsizetype(len));
        b.replace(6, 6, QByteArray::fromHex(AccessMac.toLatin1()));
        if (be16(b, 12) == 0x0806 && b.size() >= 42)
            b.replace(22, 6, b.mid(6, 6));
        if (auto frame = internetOutput(a.plan, b))
            if (a.output(*frame))
                ++a.received;
        return slirp_ssize_t(len);
    }
};
InternetGateway::InternetGateway() : impl_(std::make_unique<Impl>()) {}
InternetGateway::~InternetGateway() {
    close();
}
bool InternetGateway::active() const {
    return impl_->stack != nullptr;
}
quint64 InternetGateway::sent() const {
    return impl_->sent;
}
quint64 InternetGateway::received() const {
    return impl_->received;
}
void InternetGateway::close() {
    auto &a = *impl_;
    if (a.stack) {
        a.cleanup(a.stack);
        a.stack = nullptr;
    }
    for (auto timer : a.timers)
        delete timer;
    a.timers.clear();
    a.sockets.clear();
    a.output = {};
    if (a.dll) {
        FreeLibrary(a.dll);
        a.dll = nullptr;
    }
    if (a.winsock) {
        WSACleanup();
        a.winsock = false;
    }
}
bool InternetGateway::open(const AccessPlan &plan, Output output) {
    close();
    error.clear();
    auto &a = *impl_;
    a.plan = plan;
    a.output = std::move(output);
    a.sent = a.received = 0;
    auto fail = [&](const QString &why) {
        error = why;
        close();
        return false;
    };
    WSADATA winsock{};
    const auto wsaResult = WSAStartup(MAKEWORD(2, 2), &winsock);
    if (wsaResult != 0)
        return fail(QString("No se pudo iniciar la red de Internet (Windows %1).").arg(wsaResult));
    a.winsock = true;
    const auto path =
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/nat/libslirp-0.dll");
    a.dll = LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr,
                           LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!a.dll)
        return fail(QString("No se pudo cargar el módulo de Internet (Windows %1). Reinstala ConsoleLAN.")
                        .arg(GetLastError()));
#define NLOAD(field, name)                                                                                   \
    a.field = reinterpret_cast<decltype(a.field)>(GetProcAddress(a.dll, #name));                             \
    if (!a.field)                                                                                            \
        return fail("El módulo de Internet es incompatible. Reinstala ConsoleLAN.");
    NLOAD(create, slirp_new)
    NLOAD(cleanup, slirp_cleanup)
    NLOAD(input, slirp_input)
    NLOAD(fill, slirp_pollfds_fill_socket)
    NLOAD(poll, slirp_pollfds_poll)
#undef NLOAD
    SlirpConfig cfg{};
    cfg.version = 6;
    cfg.in_enabled = true;
    cfg.vnetwork.s_addr = htonl(plan.subnet);
    cfg.vnetmask.s_addr = htonl(0xffffff00);
    cfg.vhost.s_addr = htonl(plan.pcIp());
    cfg.vnameserver = cfg.vhost;
    cfg.vdhcp_start.s_addr = htonl(plan.subnet | 2);
    cfg.disable_dhcp = true;
    cfg.disable_host_loopback = true;
#ifdef CONSOLELAN_NATIVE_TESTS
    if (a.loopback) {
        cfg.disable_host_loopback = false;
        cfg.vnameserver.s_addr = htonl(plan.subnet | 254);
    }
#endif
    cfg.if_mtu = cfg.if_mru = 1500;
    a.internalMac = QByteArray::fromHex("5255");
    for (int i = 3; i >= 0; --i)
        a.internalMac.append(char(plan.pcIp() >> (8 * i)));
    auto &cb = a.callbacks;
    cb = {};
    cb.send_packet = Impl::send;
    cb.guest_error = [](const char *, void *) {};
    cb.clock_get_ns = Impl::now;
    cb.timer_new = [](SlirpTimerCb fn, void *data, void *opaque) -> void * {
        auto &a = *static_cast<Impl *>(opaque);
        auto t = new Impl::Timer{fn, data};
        a.timers.insert(t);
        return t;
    };
    cb.timer_free = [](void *timer, void *opaque) {
        auto &a = *static_cast<Impl *>(opaque);
        auto t = static_cast<Impl::Timer *>(timer);
        a.timers.erase(t);
        delete t;
    };
    cb.timer_mod = [](void *timer, int64_t expiry, void *) {
        static_cast<Impl::Timer *>(timer)->expiry = expiry;
    };
    cb.register_poll_socket = [](slirp_os_socket, void *) {};
    cb.unregister_poll_socket = [](slirp_os_socket, void *) {};
    cb.notify = [](void *) {};
    a.stack = a.create(&cfg, &cb, &a);
    if (!a.stack)
        return fail("No se pudo iniciar la puerta de enlace de Internet.");
    // Seed libslirp's clock before the very first UDP packet. Otherwise its
    // socket expiry is based on zero and is already overdue at the first poll.
    poll();
    if (!error.isEmpty()) {
        close();
        return false;
    }
    return true;
}
bool InternetGateway::input(const QString &adapter, const QByteArray &frame) {
    auto &a = *impl_;
    if (!a.stack)
        return false;
    bool accepted = internetInput(a.plan, adapter, frame);
#ifdef CONSOLELAN_NATIVE_TESTS
    if (!accepted && a.loopback) {
        auto f = inspect(frame);
        accepted = f && !f->arp && f->destIp == a.plan.pcIp() && f->destination == AccessMac &&
                   std::any_of(a.plan.targets.cbegin(), a.plan.targets.cend(), [&](const auto &t) {
                       return t.ip == f->sourceIp && t.mac == f->source && t.adapter == adapter;
                   });
    }
#endif
    if (!accepted)
        return false;
    auto b = frame;
    if (macText(b, 0) == AccessMac)
        b.replace(0, 6, a.internalMac);
    if (be16(b, 12) == 0x0806 && macText(b, 32) == AccessMac)
        b.replace(32, 6, a.internalMac);
    a.input(a.stack, reinterpret_cast<const uint8_t *>(b.constData()), int(b.size()));
    ++a.sent;
    return true;
}
void InternetGateway::poll() {
    auto &a = *impl_;
    if (!a.stack)
        return;
    a.sockets.clear();
    uint32_t timeout = 0;
    a.fill(
        a.stack, &timeout,
        [](slirp_os_socket fd, int events, void *opaque) {
            auto &fds = static_cast<Impl *>(opaque)->sockets;
            short flags = 0;
            if (events & SLIRP_POLL_IN)
                flags |= POLLRDNORM;
            if (events & SLIRP_POLL_OUT)
                flags |= POLLWRNORM;
            if (events & SLIRP_POLL_PRI)
                // POLLPRI makes WSAPoll fail with WSAEINVAL on Microsoft's provider.
                flags |= POLLRDBAND;
            fds.push_back({fd, flags, 0});
            return int(fds.size() - 1);
        },
        &a);
    auto rc = a.sockets.empty() ? 0 : WSAPoll(a.sockets.data(), ULONG(a.sockets.size()), 0);
    if (rc == SOCKET_ERROR)
        error = QString("No se pudo consultar la red de Internet (Windows %1).").arg(WSAGetLastError());
    a.poll(
        a.stack, rc == SOCKET_ERROR,
        [](int index, void *opaque) {
            const auto &fds = static_cast<Impl *>(opaque)->sockets;
            if (index < 0 || size_t(index) >= fds.size())
                return 0;
            auto flags = fds[index].revents;
            int events = 0;
            if (flags & POLLRDNORM)
                events |= SLIRP_POLL_IN;
            if (flags & POLLWRNORM)
                events |= SLIRP_POLL_OUT;
            if (flags & POLLRDBAND)
                events |= SLIRP_POLL_PRI;
            if (flags & (POLLERR | POLLNVAL))
                events |= SLIRP_POLL_ERR;
            if (flags & POLLHUP)
                events |= SLIRP_POLL_HUP;
            return events;
        },
        &a);
    auto timers = a.timers;
    for (auto t : timers)
        if (a.timers.contains(t) && t->expiry >= 0 && t->expiry <= Impl::now(nullptr) / 1000000) {
            t->expiry = -1;
            t->callback(t->opaque);
        }
}
#ifdef CONSOLELAN_NATIVE_TESTS
void InternetGateway::allowLoopbackForTest() {
    impl_->loopback = true;
}
#endif
} // namespace cl
