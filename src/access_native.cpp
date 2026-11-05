#include "access_native.hpp"
#include "access_pipe.hpp"
#include "capture.hpp"
#include <iphlpapi.h>
#include <netioapi.h>
#include <wintun.h>
#include <thread>
namespace cl {
struct AccessAdapter::Impl {
    HMODULE dll = nullptr;
    WINTUN_CREATE_ADAPTER_FUNC *create = nullptr;
    WINTUN_CLOSE_ADAPTER_FUNC *closeAdapter = nullptr;
    WINTUN_GET_ADAPTER_LUID_FUNC *luid = nullptr;
    WINTUN_START_SESSION_FUNC *start = nullptr;
    WINTUN_END_SESSION_FUNC *end = nullptr;
    WINTUN_RECEIVE_PACKET_FUNC *receive = nullptr;
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC *release = nullptr;
    WINTUN_ALLOCATE_SEND_PACKET_FUNC *allocate = nullptr;
    WINTUN_SEND_PACKET_FUNC *send = nullptr;
    WINTUN_ADAPTER_HANDLE adapter = nullptr;
    WINTUN_SESSION_HANDLE session = nullptr;
    NET_LUID net{};
    MIB_UNICASTIPADDRESS_ROW address{};
    bool addressAdded = false;
    QList<MIB_IPFORWARD_ROW2> routes;
};
AccessAdapter::AccessAdapter() : impl_(std::make_unique<Impl>()) {}
AccessAdapter::~AccessAdapter() {
    close();
}
void AccessAdapter::close() {
    auto &a = *impl_;
    if (a.session) {
        a.end(a.session);
        a.session = nullptr;
    }
    for (auto &r : a.routes)
        DeleteIpForwardEntry2(&r);
    a.routes.clear();
    if (a.addressAdded) {
        DeleteUnicastIpAddressEntry(&a.address);
        a.addressAdded = false;
    }
    if (a.adapter) {
        a.closeAdapter(a.adapter);
        a.adapter = nullptr;
    }
    if (a.dll) {
        FreeLibrary(a.dll);
        a.dll = nullptr;
    }
    a.net = {};
}
quint32 AccessAdapter::index() const {
    NET_IFINDEX i = 0;
    ConvertInterfaceLuidToIndex(&impl_->net, &i);
    return i;
}
bool AccessAdapter::open(const AccessPlan &p, QString name) {
    close();
    error.clear();
    auto &a = *impl_;
    auto fail = [&](QString what, DWORD rc = GetLastError()) {
        error = what + " (Windows " + QString::number(rc) + ")";
        close();
        return false;
    };
    const auto path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/wintun.dll");
    a.dll = LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr,
                           LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!a.dll)
        return fail("No se pudo cargar wintun.dll");
#define WLOAD(field, fn)                                                                                     \
    a.field = reinterpret_cast<decltype(a.field)>(GetProcAddress(a.dll, #fn));                               \
    if (!a.field)                                                                                            \
        return fail("Wintun incompatible: " #fn);
    WLOAD(create, WintunCreateAdapter)
    WLOAD(closeAdapter, WintunCloseAdapter) WLOAD(luid, WintunGetAdapterLUID) WLOAD(start, WintunStartSession)
        WLOAD(end, WintunEndSession) WLOAD(receive, WintunReceivePacket)
            WLOAD(release, WintunReleaseReceivePacket) WLOAD(allocate, WintunAllocateSendPacket)
                WLOAD(send, WintunSendPacket)
#undef WLOAD
        // Stable identity; never open, reconfigure or remove somebody else's adapter.
        GUID guid{0x3c25397a, 0x0c31, 0x4b99, {0xbf, 0x36, 0x43, 0xf5, 0x37, 0xc5, 0xb0, 0x55}};
    if (name == "ConsoleLAN Test")
        guid.Data4[7] = 0x56;
    a.adapter = a.create(reinterpret_cast<LPCWSTR>(name.utf16()), L"ConsoleLAN", &guid);
    if (!a.adapter)
        return fail("No se pudo crear el adaptador ConsoleLAN Local");
    a.luid(a.adapter, &a.net);
    MIB_IPINTERFACE_ROW iface{};
    InitializeIpInterfaceEntry(&iface);
    iface.Family = AF_INET;
    iface.InterfaceLuid = a.net;
    DWORD rc = GetIpInterfaceEntry(&iface);
    if (rc != NO_ERROR)
        return fail("No se pudo consultar el adaptador", rc);
    iface.DadTransmits = 0;
    iface.NlMtu = 1500;
    iface.ForwardingEnabled = FALSE;
    iface.WeakHostSend = FALSE;
    iface.WeakHostReceive = FALSE;
    iface.UseAutomaticMetric = FALSE;
    iface.Metric = 5;
    iface.SitePrefixLength = 0;
    rc = SetIpInterfaceEntry(&iface);
    if (rc != NO_ERROR)
        return fail("No se pudo configurar el adaptador propio", rc);
    InitializeUnicastIpAddressEntry(&a.address);
    a.address.InterfaceLuid = a.net;
    a.address.Address.Ipv4.sin_family = AF_INET;
    a.address.Address.Ipv4.sin_addr.s_addr = htonl(p.pcIp());
    a.address.OnLinkPrefixLength = 32;
    a.address.DadState = IpDadStatePreferred;
    a.address.PrefixOrigin = IpPrefixOriginManual;
    a.address.SuffixOrigin = IpSuffixOriginManual;
    rc = CreateUnicastIpAddressEntry(&a.address);
    if (rc != NO_ERROR)
        return fail("No se pudo asignar la IP del adaptador propio", rc);
    a.addressAdded = true;
    for (const auto &t : p.targets) {
        MIB_IPFORWARD_ROW2 r{};
        InitializeIpForwardEntry(&r);
        r.InterfaceLuid = a.net;
        r.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
        r.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr = htonl(t.ip);
        r.DestinationPrefix.PrefixLength = 32;
        r.NextHop.Ipv4.sin_family = AF_INET;
        r.Metric = 5;
        r.Protocol = MIB_IPPROTO_NETMGMT;
        rc = CreateIpForwardEntry2(&r);
        if (rc != NO_ERROR)
            return fail("No se pudo crear la ruta local de consola", rc);
        a.routes.append(r);
    }
    a.session = a.start(a.adapter, 0x400000);
    if (!a.session)
        return fail("No se pudo iniciar el adaptador propio");
    auto deadline = clockMs() + 3000;
    while (clockMs() < deadline) {
        auto row = a.address;
        if (GetUnicastIpAddressEntry(&row) == NO_ERROR && row.DadState == IpDadStatePreferred)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return fail("No se pudo activar la IP del adaptador propio", ERROR_TIMEOUT);
    return true;
}
std::optional<QByteArray> AccessAdapter::receive() {
    auto &a = *impl_;
    DWORD length = 0;
    auto packet = a.receive(a.session, &length);
    if (!packet) {
        if (GetLastError() != ERROR_NO_MORE_ITEMS)
            error = "No se pudo leer el adaptador propio.";
        return {};
    }
    auto b = QByteArray(reinterpret_cast<const char *>(packet), int(length));
    a.release(a.session, packet);
    return b;
}
bool AccessAdapter::send(const QByteArray &b) {
    auto &a = *impl_;
    auto packet = a.allocate(a.session, DWORD(b.size()));
    if (!packet) {
        if (GetLastError() != ERROR_BUFFER_OVERFLOW)
            error = "No se pudo escribir en el adaptador propio.";
        return false;
    }
    memcpy(packet, b.constData(), b.size());
    a.send(a.session, packet);
    return true;
}
int accessHelperMain(const QString &pipeName, quint32 parentPid) {
    HANDLE parent = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentPid);
    if (!parent)
        return 2;
    HANDLE pipe = CreateFileW(reinterpret_cast<LPCWSTR>(pipeName.utf16()), GENERIC_READ | GENERIC_WRITE, 0,
                              nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        CloseHandle(parent);
        return 3;
    }
    ULONG server = 0;
    if (!GetNamedPipeServerProcessId(pipe, &server) || server != parentPid) {
        CloseHandle(pipe);
        CloseHandle(parent);
        return 4;
    }
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);
    AccessAdapter tun;
    std::optional<AccessPlan> current;
    std::map<QString, std::unique_ptr<Capture>> captures;
    bool broken = false;
    QString error;
    auto heard = clockMs();
    qint64 reported = 0;
    quint64 sent = 0, received = 0, filtered = 0;
    while (WaitForSingleObject(parent, 0) == WAIT_TIMEOUT && clockMs() - heard < 4000) {
        if (auto config = pipeRead(pipe, broken)) {
            heard = clockMs();
            if ((*config)["stop"].toBool())
                break;
            auto plan = accessPlan(*config, &error);
            if (!plan)
                break;
            if (!current || *current != *plan) {
                tun.close();
                captures.clear();
                current.reset();
                auto all = adapters();
                error = subnetConflict(plan->subnet, all);
                if (!error.isEmpty())
                    break;
                for (const auto &t : plan->targets) {
#ifdef CONSOLELAN_NATIVE_TESTS
                    if (t.adapter == "synthetic-ethernet")
                        continue;
#endif
                    const bool physical = std::any_of(all.cbegin(), all.cend(), [&](const Adapter &a) {
                        return a.id == t.adapter && a.physical && a.up;
                    });
                    if (!physical) {
                        error = "La Ethernet local no está disponible o no es física.";
                        break;
                    }
                    if (captures.contains(t.adapter))
                        continue;
                    auto cap = std::make_unique<Capture>();
                    if (!cap->open(t.adapter)) {
                        error = cap->error;
                        break;
                    }
                    captures.emplace(t.adapter, std::move(cap));
                }
                if (!error.isEmpty())
                    break;
                // Probe the reserved .1 address before using it, without configuring the physical NIC.
                auto probe = QByteArray::fromHex(
                    "ffffffffffff02434c5043010806000108000604000102434c50430100000000000000000000");
                const auto pc = plan->pcIp();
                for (int i = 3; i >= 0; --i)
                    probe.append(char(pc >> (i * 8)));
                probe.resize(60, 0);
                for (int attempt = 0; attempt < 3 && error.isEmpty(); ++attempt) {
                    for (auto &[id, cap] : captures)
                        if (!cap->inject(probe))
                            error = cap->error;
                    auto until = clockMs() + 200;
                    while (clockMs() < until && error.isEmpty()) {
                        for (auto &[id, cap] : captures)
                            for (int i = 0; i < 128; ++i) {
                                auto b = cap->receive();
                                if (!b)
                                    break;
                                if (accessFromEthernet(*plan, id, *b).conflict) {
                                    error =
                                        "La dirección reservada para el PC ya está en uso en la LAN física.";
                                    break;
                                }
                            }
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    }
                }
                if (!error.isEmpty())
                    break;
#ifdef CONSOLELAN_NATIVE_TESTS
                if (!tun.open(*plan, "ConsoleLAN Test")) {
                    error = tun.error;
                    break;
                }
#else
                if (!tun.open(*plan)) {
                    error = tun.error;
                    break;
                }
#endif
                current = *plan;
            }
        }
        if (broken)
            break;
        if (current) {
            for (int i = 0; i < 128; ++i) {
                auto ip = tun.receive();
                if (!ip)
                    break;
                auto frame = accessToEthernet(*current, *ip);
                if (frame && captures.contains(frame->adapter)) {
                    if (captures.at(frame->adapter)->inject(frame->bytes))
                        ++sent;
                    else
                        error = captures.at(frame->adapter)->error;
                } else
                    ++filtered;
            }
            for (auto &[id, cap] : captures) {
                for (int i = 0; i < 128; ++i) {
                    auto b = cap->receive();
                    if (!b)
                        break;
                    auto reply = accessFromEthernet(*current, id, *b);
                    if (reply.conflict) {
                        error = "Conflicto de IP en el acceso local. Se ha detenido el adaptador propio.";
                        break;
                    }
                    if (!reply.ethernet.isEmpty() && !cap->inject(reply.ethernet))
                        error = cap->error;
                    if (!reply.ip.isEmpty() && tun.send(reply.ip))
                        ++received;
                }
                if (!cap->error.isEmpty())
                    error = cap->error;
            }
            if (!tun.error.isEmpty())
                error = tun.error;
            if (!error.isEmpty())
                break;
        }
        if (clockMs() - reported > 400) {
            if (!pipeSend(pipe, {{"state", current ? "active" : "starting"},
                                 {"pcIp", current ? ipText(current->pcIp()) : QString()},
                                 {"sent", double(sent)},
                                 {"received", double(received)},
                                 {"filtered", double(filtered)}}))
                break;
            reported = clockMs();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    tun.close();
    captures.clear();
    if (!error.isEmpty())
        pipeSend(pipe, {{"state", "error"}, {"error", error}});
    CloseHandle(pipe);
    CloseHandle(parent);
    return error.isEmpty() ? 0 : 5;
}
} // namespace cl
