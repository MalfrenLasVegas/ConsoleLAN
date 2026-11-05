#include "access_controller.hpp"
#include "access_policy.hpp"
#include "access_pipe.hpp"
#include <shellapi.h>
#include <sddl.h>
namespace cl {
AccessController::AccessController() : thread_([this](std::stop_token s) { run(s); }) {}
AccessController::~AccessController() {
    thread_.request_stop();
    if (thread_.joinable())
        thread_.join();
}
void AccessController::enable(bool b) {
    std::lock_guard l(mutex_);
    requested_ = b;
    if (b)
        status_ = {{"state", "starting"}};
}
void AccessController::update(QJsonObject v) {
    std::lock_guard l(mutex_);
    view_ = std::move(v);
}
QJsonObject AccessController::status() {
    std::lock_guard l(mutex_);
    auto s = status_;
    s["requested"] = requested_;
    return s;
}
void AccessController::run(std::stop_token stop) {
    while (!stop.stop_requested()) {
        QJsonObject v;
        bool wanted;
        {
            std::lock_guard l(mutex_);
            v = view_;
            wanted = requested_;
        }
        if (!wanted) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        auto finish = [&](QString error) {
            std::lock_guard l(mutex_);
            requested_ = false;
            status_ = {{"state", error.isEmpty() ? "off" : "error"}, {"error", error}};
        };
        QString error;
        if (!accessPlan(v, &error)) {
            finish(error);
            continue;
        }
        QString pipeName =
            "\\\\.\\pipe\\ConsoleLAN-Access-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        // Only the current Windows user, Administrators and SYSTEM can connect.
        HANDLE token = nullptr;
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        DWORD size = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        QByteArray info(size, 0);
        bool got = GetTokenInformation(token, TokenUser, info.data(), size, &size);
        if (token)
            CloseHandle(token);
        LPWSTR sid = nullptr;
        if (!got || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(info.data())->User.Sid, &sid)) {
            finish("No se pudo proteger el canal de acceso local.");
            continue;
        }
        QString acl = "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;" + QString::fromWCharArray(sid) + ")";
        LocalFree(sid);
        PSECURITY_DESCRIPTOR sd = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(reinterpret_cast<LPCWSTR>(acl.utf16()),
                                                                  SDDL_REVISION_1, &sd, nullptr)) {
            finish("No se pudo proteger el canal de acceso local.");
            continue;
        }
        SECURITY_ATTRIBUTES sa{sizeof(sa), sd, FALSE};
        HANDLE pipe = CreateNamedPipeW(
            reinterpret_cast<LPCWSTR>(pipeName.utf16()),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 32768,
            32768, 0, &sa);
        LocalFree(sd);
        if (pipe == INVALID_HANDLE_VALUE) {
            finish("No se pudo crear el canal de acceso local.");
            continue;
        }
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        BOOL connected = ConnectNamedPipe(pipe, &ov);
        auto connectError = GetLastError();
        const auto exe =
            QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/ConsoleLAN.Access.exe");
        const auto args = "--pipe \"" + pipeName + "\" --parent " + QString::number(GetCurrentProcessId());
        SHELLEXECUTEINFOW launch{};
        launch.cbSize = sizeof(launch);
        launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        launch.lpVerb = L"runas";
        launch.lpFile = reinterpret_cast<LPCWSTR>(exe.utf16());
        launch.lpParameters = reinterpret_cast<LPCWSTR>(args.utf16());
        launch.nShow = SW_HIDE;
        bool launched = ShellExecuteExW(&launch);
        if (!launched)
            error = GetLastError() == ERROR_CANCELLED
                        ? "Se canceló el permiso de administrador. No se ha activado el acceso local."
                        : "No se pudo iniciar ConsoleLAN.Access.exe.";
        if (launched) {
            auto deadline = clockMs() + 15000;
            while (!connected && connectError == ERROR_IO_PENDING && !stop.stop_requested() &&
                   clockMs() < deadline) {
                if (WaitForSingleObject(ov.hEvent, 50) == WAIT_OBJECT_0) {
                    DWORD n;
                    connected = GetOverlappedResult(pipe, &ov, &n, FALSE);
                    break;
                }
                if (WaitForSingleObject(launch.hProcess, 0) == WAIT_OBJECT_0)
                    break;
            }
            if (connectError == ERROR_PIPE_CONNECTED)
                connected = TRUE;
            ULONG child = 0;
            if (!connected || !GetNamedPipeClientProcessId(pipe, &child) ||
                child != GetProcessId(launch.hProcess))
                error = "No se pudo verificar el proceso de acceso local.";
        }
        if (!connected) {
            CancelIoEx(pipe, &ov);
            DWORD n;
            GetOverlappedResult(pipe, &ov, &n, TRUE);
        }
        CloseHandle(ov.hEvent);
        qint64 sent = 0, lastStatus = clockMs();
        bool broken = false;
        while (launched && error.isEmpty() && !stop.stop_requested()) {
            {
                std::lock_guard l(mutex_);
                wanted = requested_;
                v = view_;
            }
            if (!wanted)
                break;
            if (!accessPlan(v, &error))
                break;
            if (clockMs() - sent > 400) {
                if (!pipeSend(pipe, v)) {
                    error = "Se perdió el canal de acceso local.";
                    break;
                }
                sent = clockMs();
            }
            if (auto reply = pipeRead(pipe, broken)) {
                lastStatus = clockMs();
                {
                    std::lock_guard l(mutex_);
                    status_ = *reply;
                }
                if ((*reply)["state"] == "error") {
                    error = (*reply)["error"].toString();
                    break;
                }
            }
            if (broken || WaitForSingleObject(launch.hProcess, 0) == WAIT_OBJECT_0 ||
                clockMs() - lastStatus > 8000) {
                error = "El proceso de acceso local se detuvo.";
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (connected)
            pipeSend(pipe, {{"stop", true}});
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        if (launched) {
            WaitForSingleObject(launch.hProcess, 4000);
            CloseHandle(launch.hProcess);
        }
        finish(error);
    }
}
} // namespace cl
