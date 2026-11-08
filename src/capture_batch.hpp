#pragma once
#include "common.hpp"
#include <span>
namespace cl {
// A short burst bounds the time before ACK, control and Internet are serviced.
inline constexpr int LocalBatchPackets = 64;
enum class BatchSendResult { Sent, Unavailable, Failed };
// Only fall back before a native send has been attempted. A partial driver
// write is a failure, never permission to duplicate the complete batch.
template <class NativeSend, class SingleSend>
bool dispatchLocalBatch(std::span<const QByteArray> frames, NativeSend native, SingleSend single) {
    if (frames.empty())
        return true;
    switch (native()) {
    case BatchSendResult::Sent:
        return true;
    case BatchSendResult::Failed:
        return false;
    case BatchSendResult::Unavailable:
        for (const auto &frame : frames)
            if (!single(frame))
                return false;
        return true;
    }
    return false;
}
} // namespace cl
