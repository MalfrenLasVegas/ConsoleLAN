#include "network.hpp"
#include "capture.hpp"
#include <windows.h>
#include <pcap.h>
namespace cl {
struct Api {
    HMODULE dll = nullptr;
    QString error;
    decltype(&pcap_create) create = nullptr;
    decltype(&pcap_activate) activate = nullptr;
    decltype(&pcap_set_snaplen) snaplen = nullptr;
    decltype(&pcap_set_promisc) promisc = nullptr;
    decltype(&pcap_set_timeout) timeout = nullptr;
    decltype(&pcap_set_immediate_mode) immediate = nullptr;
    decltype(&pcap_setnonblock) nonblock = nullptr;
    decltype(&pcap_next_ex) next = nullptr;
    decltype(&pcap_sendpacket) send = nullptr;
    decltype(&pcap_close) close = nullptr;
    decltype(&pcap_datalink) datalink = nullptr;
    decltype(&pcap_compile) compile = nullptr;
    decltype(&pcap_setfilter) filter = nullptr;
    decltype(&pcap_freecode) freecode = nullptr;
    decltype(&pcap_geterr) geterr = nullptr;
    decltype(&pcap_lib_version) version = nullptr;
    decltype(&pcap_getevent) event = nullptr;
    Api() {
        wchar_t dir[MAX_PATH];
        GetSystemDirectoryW(dir, MAX_PATH);
        auto path = QString::fromWCharArray(dir) + "\\Npcap\\wpcap.dll";
        dll = LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr,
                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!dll) {
            error = "Npcap no disponible. Instálelo desde npcap.com; si está restringido a administradores, "
                    "ejecute ConsoleLAN con los permisos adecuados.";
            return;
        }
#define LOAD(field, fn)                                                                                      \
    field = reinterpret_cast<decltype(field)>(GetProcAddress(dll, #fn));                                     \
    if (!field) {                                                                                            \
        error = "Npcap incompatible: falta " #fn;                                                            \
        return;                                                                                              \
    }
        LOAD(create, pcap_create)
        LOAD(activate, pcap_activate)
        LOAD(snaplen, pcap_set_snaplen)
        LOAD(promisc, pcap_set_promisc)
        LOAD(timeout, pcap_set_timeout)
        LOAD(immediate, pcap_set_immediate_mode)
        LOAD(nonblock, pcap_setnonblock) LOAD(next, pcap_next_ex) LOAD(send, pcap_sendpacket)
            LOAD(close, pcap_close) LOAD(datalink, pcap_datalink) LOAD(compile, pcap_compile)
                LOAD(filter, pcap_setfilter) LOAD(freecode, pcap_freecode) LOAD(geterr, pcap_geterr)
                    LOAD(version, pcap_lib_version) LOAD(event, pcap_getevent)
#undef LOAD
    }
    ~Api() {
        if (dll)
            FreeLibrary(dll);
    }
};
static Api &api() {
    static Api a;
    return a;
}
struct Capture::Impl {
    pcap_t *p = nullptr;
    ~Impl() {
        if (p)
            api().close(p);
    }
};
Capture::Capture() : impl_(std::make_unique<Impl>()) {}
Capture::~Capture() = default;
QString Capture::version() {
    auto &a = api();
    return a.error.isEmpty() ? QString::fromLatin1(a.version()) : a.error;
}
bool Capture::open(QString id, const QString &expression) {
    auto &a = api();
    if (!a.error.isEmpty()) {
        error = a.error;
        return false;
    }
    adapter = id;
    char err[PCAP_ERRBUF_SIZE]{};
    auto dev = ("\\Device\\NPF_" + id).toLatin1();
    impl_->p = a.create(dev.constData(), err);
    if (!impl_->p) {
        error = QString::fromLocal8Bit(err);
        return false;
    }
    auto p = impl_->p;
    if (a.snaplen(p, 1600) || a.promisc(p, 1) || a.timeout(p, 1) || a.immediate(p, 1)) {
        error = "No se pudo configurar Npcap";
        return false;
    }
    if (a.activate(p) < 0) {
        error = QString::fromLocal8Bit(a.geterr(p));
        return false;
    }
    if (a.datalink(p) != DLT_EN10MB) {
        error = "La interfaz no ofrece tramas Ethernet";
        return false;
    }
    if (a.nonblock(p, 1, err) < 0) {
        error = QString::fromLocal8Bit(err);
        return false;
    }
    bpf_program filter{};
    const auto filterText =
        expression.isEmpty() ? QByteArray("arp or ip or ether proto 0x88b5") : expression.toLatin1();
    if (a.compile(p, &filter, filterText.constData(), 1, PCAP_NETMASK_UNKNOWN) < 0) {
        error = QString::fromLocal8Bit(a.geterr(p));
        return false;
    }
    int result = a.filter(p, &filter);
    a.freecode(&filter);
    if (result < 0) {
        error = QString::fromLocal8Bit(a.geterr(p));
        return false;
    }
    return true;
}
std::optional<QByteArray> Capture::receive() {
    pcap_pkthdr *h = nullptr;
    const u_char *bytes = nullptr;
    int n = api().next(impl_->p, &h, &bytes);
    if (n == 1) {
        if (h->caplen != h->len || h->len > 1600)
            return {};
        return QByteArray(reinterpret_cast<const char *>(bytes), int(h->caplen));
    }
    if (n < 0)
        error = QString::fromLocal8Bit(api().geterr(impl_->p));
    return {};
}
void *Capture::readEvent() const {
    return impl_->p ? api().event(impl_->p) : nullptr;
}
bool Capture::inject(const QByteArray &b) {
    if (api().send(impl_->p, reinterpret_cast<const u_char *>(b.constData()), int(b.size()))) {
        error = QString::fromLocal8Bit(api().geterr(impl_->p));
        return false;
    }
    return true;
}
QByteArray bridgeBeacon(Id n) {
    QByteArray b = QByteArray::fromHex("ffffffffffff0200434c414e88b5");
    b += "ConsoleLAN1";
    for (int i = 7; i >= 0; --i)
        b.append(char(n >> (i * 8)));
    b.resize(60, 0);
    return b;
}
std::optional<Id> beaconNode(const QByteArray &b) {
    if (b.size() < 33 || be16(b, 12) != 0x88b5 || b.mid(14, 11) != "ConsoleLAN1")
        return {};
    Id n = 0;
    for (int i = 25; i < 33; ++i)
        n = (n << 8) | quint8(b[i]);
    return n;
}
} // namespace cl
