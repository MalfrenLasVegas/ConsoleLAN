#include "access_policy.hpp"
#include "capture.hpp"
#include "network.hpp"
#include <winsock2.h>
#include <windows.h>
#include <pcap.h>
#include <iostream>
#include <stdexcept>
using namespace cl;
static void check(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(name);
    std::cout << "PASS " << name << '\n';
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    wchar_t system[MAX_PATH];
    GetSystemDirectoryW(system, MAX_PATH);
    const auto path = QString::fromWCharArray(system) + "\\Npcap\\wpcap.dll";
    auto dll = LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr,
                              LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dll) {
        std::cerr << "SKIP Npcap unavailable\n";
        return 77;
    }
    auto unload = qScopeGuard([&] { FreeLibrary(dll); });
    auto open = reinterpret_cast<decltype(&pcap_open_dead)>(GetProcAddress(dll, "pcap_open_dead"));
    auto compile = reinterpret_cast<decltype(&pcap_compile)>(GetProcAddress(dll, "pcap_compile"));
    auto filter =
        reinterpret_cast<decltype(&pcap_offline_filter)>(GetProcAddress(dll, "pcap_offline_filter"));
    auto freecode = reinterpret_cast<decltype(&pcap_freecode)>(GetProcAddress(dll, "pcap_freecode"));
    auto close = reinterpret_cast<decltype(&pcap_close)>(GetProcAddress(dll, "pcap_close"));
    try {
        check(open && compile && filter && freecode && close, "real Npcap BPF API available");
        auto p = open(DLT_EN10MB, 1600);
        check(p, "offline Ethernet capture");
        auto cleanup = qScopeGuard([&] { close(p); });
        bpf_program code{};
        auto expression = localAccessCaptureFilter(0xc0a83201).toLatin1();
        check(compile(p, &code, expression.constData(), 1, PCAP_NETMASK_UNKNOWN) == 0,
              "production local-access BPF compiles");
        auto free = qScopeGuard([&] { freecode(&code); });
        auto accepts = [&](const QByteArray &b) {
            pcap_pkthdr h{};
            h.caplen = h.len = b.size();
            return filter(&code, &h, reinterpret_cast<const u_char *>(b.constData())) != 0;
        };
        AccessPlan plan{0xc0a83200, {{0xc0a83202, "02:00:00:00:00:02", "test"}}};
        QByteArray ip(40, 0);
        ip[0] = 0x45;
        ip[3] = 40;
        ip[8] = 64;
        ip[9] = 6;
        ip.replace(12, 4, QByteArray::fromHex("c0a83201"));
        ip.replace(16, 4, QByteArray::fromHex("c0a83202"));
        auto outbound = accessToEthernet(plan, ip);
        check(outbound && !accepts(outbound->bytes), "own bulk injections are not captured again by helper");
        auto reply = QByteArray::fromHex(AccessMac.toLatin1()) + QByteArray::fromHex("0200000000020800") + ip;
        reply.replace(26, 4, QByteArray::fromHex("c0a83202"));
        reply.replace(30, 4, QByteArray::fromHex("c0a83201"));
        check(accepts(reply), "console replies and new inbound TCP remain captured");
        reply.replace(30, 4, QByteArray::fromHex("08080808"));
        check(accepts(reply), "console Internet packets addressed to gateway MAC remain captured");
        auto conflict = outbound->bytes;
        conflict.replace(6, 6, QByteArray::fromHex("020000000099"));
        check(accepts(conflict), "foreign IPv4 sender on reserved PC address remains visible");
        auto other = conflict;
        other.replace(26, 4, QByteArray::fromHex("c0a80186"));
        check(!accepts(other), "unrelated host IPv4 traffic is excluded from local helper");
        auto arp = QByteArray::fromHex(
            "ffffffffffff02000000000208060001080006040001020000000002c0a83202000000000000c0a83201");
        check(accepts(arp), "ARP and reserved-address conflict probes remain visible");
        bpf_program roomCode{};
        auto roomExpression = roomCaptureFilter().toLatin1();
        check(compile(p, &roomCode, roomExpression.constData(), 1, PCAP_NETMASK_UNKNOWN) == 0,
              "room isolation BPF compiles");
        auto freeRoom = qScopeGuard([&] { freecode(&roomCode); });
        auto roomAccepts = [&](const QByteArray &b) {
            pcap_pkthdr h{};
            h.caplen = h.len = b.size();
            return filter(&roomCode, &h, reinterpret_cast<const u_char *>(b.constData())) != 0;
        };
        check(!roomAccepts(outbound->bytes) && !roomAccepts(reply),
              "local bulk data cannot fill the room capture queue");
        auto lan = outbound->bytes;
        lan.replace(0, 6, QByteArray::fromHex("020000000003"));
        lan.replace(6, 6, QByteArray::fromHex("020000000002"));
        lan.replace(26, 4, QByteArray::fromHex("c0a83202"));
        lan.replace(30, 4, QByteArray::fromHex("c0a83203"));
        check(roomAccepts(lan), "ordinary console IPv4 game traffic remains captured");
        check(roomAccepts(arp), "console ARP remains captured by the room");
        auto beacon = bridgeBeacon(1234);
        check(roomAccepts(beacon), "duplicate bridge detection beacons remain captured");
        beacon.replace(6, 6, QByteArray::fromHex(AccessMac.toLatin1()));
        check(roomAccepts(beacon), "beacon visibility does not depend on source MAC");
        // Observe one real capture handle, without injecting packets or changing
        // addresses/routes. The worker's default filter remains unchanged.
        const auto all = adapters();
        const auto physical =
            std::find_if(all.cbegin(), all.cend(), [](const auto &a) { return a.physical && a.up; });
        if (physical != all.cend()) {
            Capture live;
            check(live.open(physical->id, localAccessCaptureFilter(plan.pcIp())),
                  "local capture opens with production filter");
            check(live.readEvent() && WaitForSingleObject(live.readEvent(), 0) != WAIT_FAILED,
                  "real Npcap readiness handle is waitable");
        }
        std::cout << "SUCCESS: local capture filter regression checks\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
    return 0;
}
