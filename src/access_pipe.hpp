#pragma once
#include "network.hpp"
#include <windows.h>
namespace cl {
// Message reads/writes are bounded even if the other process stops responding.
inline bool pipeIo(HANDLE pipe, QByteArray &bytes, bool write) {
    if (bytes.isEmpty() || bytes.size() > 32768)
        return false;
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent)
        return false;
    DWORD n = 0;
    BOOL ok = write ? WriteFile(pipe, bytes.data(), DWORD(bytes.size()), &n, &ov)
                    : ReadFile(pipe, bytes.data(), DWORD(bytes.size()), &n, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        if (WaitForSingleObject(ov.hEvent, 500) == WAIT_OBJECT_0)
            ok = GetOverlappedResult(pipe, &ov, &n, FALSE);
        else {
            CancelIoEx(pipe, &ov);
            GetOverlappedResult(pipe, &ov, &n, TRUE);
        }
    }
    CloseHandle(ov.hEvent);
    if (!ok || (write && n != bytes.size()))
        return false;
    if (!write)
        bytes.resize(n);
    return true;
}
inline bool pipeSend(HANDLE pipe, const QJsonObject &o) {
    auto b = jsonBytes(o);
    return pipeIo(pipe, b, true);
}
inline std::optional<QJsonObject> pipeRead(HANDLE pipe, bool &broken) {
    DWORD available = 0;
    if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
        broken = true;
        return {};
    }
    if (!available)
        return {};
    auto b = QByteArray(32768, 0);
    if (!pipeIo(pipe, b, false)) {
        broken = true;
        return {};
    }
    QJsonParseError e;
    auto d = QJsonDocument::fromJson(b, &e);
    if (e.error != QJsonParseError::NoError || !d.isObject()) {
        broken = true;
        return {};
    }
    return d.object();
}
} // namespace cl
