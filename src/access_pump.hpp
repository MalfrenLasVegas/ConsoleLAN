#pragma once
#include "network.hpp"
#include <windows.h>
#include <span>
#include <array>
namespace cl {
// Bounded turns keep control messages and all adapters responsive. A full
// turn means work may remain queued: the caller must not sleep in that case.
inline constexpr int AccessPacketBudget = 512;
struct AccessPass {
    int packets = 0;
    bool saturated = false;
};
template <class Receive, class Consume>
AccessPass drainAccessPackets(Receive receive, Consume consume, int budget = AccessPacketBudget) {
    AccessPass pass;
    while (pass.packets < budget) {
        auto packet = receive();
        if (!packet)
            return pass;
        ++pass.packets;
        if (!consume(*packet))
            return pass;
    }
    pass.saturated = true;
    return pass;
}
// Borrowed Wintun/Npcap handles; parent comes first so shutdown wins over data.
inline DWORD waitForAccessActivity(HANDLE parent, std::span<const HANDLE> packets, DWORD timeout) {
    if (packets.size() >= MAXIMUM_WAIT_OBJECTS) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return WAIT_FAILED;
    }
    std::array<HANDLE, MAXIMUM_WAIT_OBJECTS> events{};
    events[0] = parent;
    std::copy(packets.begin(), packets.end(), events.begin() + 1);
    return WaitForMultipleObjects(DWORD(packets.size() + 1), events.data(), FALSE, timeout);
}
} // namespace cl
