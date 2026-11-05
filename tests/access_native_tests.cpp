#include "access_native.hpp"
#include "network.hpp"
#include "access_pipe.hpp"
#include <iphlpapi.h>
#include <netioapi.h>
#include <future>
#include <iostream>
#include <stdexcept>
using namespace cl;
static void check(bool b, const char *s) {
    if (!b)
        throw std::runtime_error(s);
    std::cout << "PASS " << s << std::endl;
}
static void p16(QByteArray &b, int p, quint16 n) {
    b[p] = char(n >> 8);
    b[p + 1] = char(n);
}
static void p32(QByteArray &b, int p, quint32 n) {
    p16(b, p, quint16(n >> 16));
    p16(b, p + 2, quint16(n));
}
static quint16 checksum(QByteArray b) {
    if (b.size() % 2)
        b.append('\0');
    quint32 sum = 0;
    for (int i = 0; i < b.size(); i += 2)
        sum += be16(b, i);
    while (sum >> 16)
        sum = (sum & 65535) + (sum >> 16);
    return quint16(~sum);
}
static QByteArray tcp(quint32 src, quint32 dst, quint16 port, quint16 client, quint32 seq, quint32 ack,
                      quint8 flags, QByteArray data = {}) {
    QByteArray b(40, 0);
    b[0] = 0x45;
    b[8] = 64;
    b[9] = 6;
    p16(b, 2, quint16(40 + data.size()));
    p32(b, 12, src);
    p32(b, 16, dst);
    p16(b, 20, port);
    p16(b, 22, client);
    p32(b, 24, seq);
    p32(b, 28, ack);
    b[32] = 0x50;
    b[33] = char(flags);
    p16(b, 34, 65535);
    b += data;
    auto pseudo = b.mid(12, 8) + QByteArray::fromHex("0006");
    pseudo.append(char((b.size() - 20) >> 8));
    pseudo.append(char(b.size() - 20));
    p16(b, 36, checksum(pseudo + b.mid(20)));
    p16(b, 10, checksum(b.left(20)));
    return b;
}
struct Socket {
    SOCKET s = INVALID_SOCKET;
    Socket() {
        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        DWORD ms = 5000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&ms), sizeof(ms));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&ms), sizeof(ms));
    }
    ~Socket() {
        if (s != INVALID_SOCKET)
            closesocket(s);
    }
    void connectTo(quint32 ip, quint16 port) {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(ip);
        a.sin_port = htons(port);
        check(connect(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)) == 0,
              "Windows TCP connection through Wintun");
    }
    void write(QByteArray b) {
        int p = 0;
        while (p < b.size()) {
            auto n = send(s, b.constData() + p, int(b.size() - p), 0);
            if (n <= 0)
                throw std::runtime_error("TCP write");
            p += n;
        }
    }
    QByteArray line() {
        QByteArray b;
        while (b.size() < 1024) {
            char c;
            auto n = recv(s, &c, 1, 0);
            if (n <= 0)
                throw std::runtime_error("FTP response timeout");
            b += c;
            if (c == '\n')
                return b;
        }
        throw std::runtime_error("FTP line bound");
    }
};
static QStringList existingAddresses() {
    QStringList out;
    for (const auto &a : adapters())
        if (!a.name.startsWith("ConsoleLAN"))
            out << a.id + "|" + ipText(a.ip) + "/" + QString::number(a.prefix);
    out.sort();
    return out;
}
static bool hasRoute(quint32 index) {
    PMIB_IPFORWARD_TABLE2 table = nullptr;
    if (GetIpForwardTable2(AF_INET, &table) != NO_ERROR)
        return true;
    bool found = false;
    for (ULONG i = 0; i < table->NumEntries; ++i)
        if (table->Table[i].InterfaceIndex == index)
            found = true;
    FreeMibTable(table);
    return found;
}

static QJsonObject helperConfig() {
    QJsonObject console{{"owner", "test-owner"},       {"key", "test-console"},
                        {"name", "Synthetic console"}, {"ip", "10.253.248.2"},
                        {"mac", "02:00:00:00:00:02"},  {"adapter", "synthetic-ethernet"}};
    return {{"mode", "client"},
            {"owner", "test-owner"},
            {"subnet", "10.253.248.0"},
            {"consoles", QJsonArray{console}}};
}
static HANDLE launchSelf(const QString &args) {
    auto command = "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\" " + args;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    check(CreateProcessW(nullptr, reinterpret_cast<LPWSTR>(command.data()), nullptr, nullptr, FALSE,
                         CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process),
          "launch isolated helper test process");
    CloseHandle(process.hThread);
    return process.hProcess;
}
struct HelperPeer {
    HANDLE pipe = INVALID_HANDLE_VALUE, process = nullptr;
    HelperPeer() {
        const auto name =
            "\\\\.\\pipe\\ConsoleLAN-Access-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        pipe = CreateNamedPipeW(reinterpret_cast<LPCWSTR>(name.utf16()),
                                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
                                    PIPE_REJECT_REMOTE_CLIENTS,
                                1, 32768, 32768, 0, nullptr);
        check(pipe != INVALID_HANDLE_VALUE, "create helper test channel");
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ConnectNamedPipe(pipe, &ov);
        process = launchSelf("--helper \"" + name + "\" " + QString::number(GetCurrentProcessId()));
        DWORD n = 0;
        check(WaitForSingleObject(ov.hEvent, 5000) == WAIT_OBJECT_0 &&
                  GetOverlappedResult(pipe, &ov, &n, FALSE),
              "helper connects to actual parent pipe");
        CloseHandle(ov.hEvent);
        auto deadline = clockMs() + 10000;
        bool active = false, broken = false;
        auto sent = qint64(0);
        while (clockMs() < deadline && !active) {
            if (clockMs() - sent > 400) {
                check(pipeSend(pipe, helperConfig()), "helper configuration heartbeat");
                sent = clockMs();
            }
            if (auto reply = pipeRead(pipe, broken)) {
                if ((*reply)["state"] == "error")
                    throw std::runtime_error(qPrintable((*reply)["error"].toString()));
                active = (*reply)["state"] == "active";
            }
            if (broken)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        check(active, "real helper owns active Wintun adapter");
    }
    ~HelperPeer() {
        if (pipe != INVALID_HANDLE_VALUE) {
            pipeSend(pipe, {{"stop", true}});
            CloseHandle(pipe);
        }
        if (process) {
            WaitForSingleObject(process, 6000);
            CloseHandle(process);
        }
    }
    void disconnect() {
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }
    void wait() {
        check(WaitForSingleObject(process, 6500) == WAIT_OBJECT_0, "helper terminates within cleanup bound");
        DWORD code = 999;
        GetExitCodeProcess(process, &code);
        check(code == 0, "helper exits cleanly");
    }
};
static void assertHelperClean() {
    auto deadline = clockMs() + 3000;
    bool present = true;
    while (clockMs() < deadline) {
        const auto list = adapters();
        present = std::any_of(list.cbegin(), list.cend(),
                              [](const auto &a) { return a.name == "ConsoleLAN Test"; });
        if (!present)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    check(!present, "helper adapter removed after disable disconnect timeout or parent death");
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() == 4 && app.arguments()[1] == "--helper")
        return accessHelperMain(app.arguments()[2], app.arguments()[3].toUInt());
    if (app.arguments().size() == 3 && app.arguments()[1] == "--owner") {
        HelperPeer peer;
        QFile handoff(app.arguments()[2]);
        if (!handoff.open(QIODevice::WriteOnly))
            return 2;
        handoff.write(QByteArray::number(GetProcessId(peer.process)));
        handoff.close();
        ExitProcess(0); // Deliberately bypass destructors to exercise parent-crash cleanup.
    }
    QFile report;
    if (app.arguments().size() == 3 && app.arguments()[1] == "--report") {
        report.setFileName(app.arguments()[2]);
        if (!report.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return 2;
    }
    auto result = [&](QString text, int code) {
        if (report.isOpen()) {
            report.write(text.toUtf8());
            report.flush();
        }
        return code;
    };
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID admins = nullptr;
    BOOL elevated = FALSE;
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                             &admins);
    CheckTokenMembership(nullptr, admins, &elevated);
    FreeSid(admins);
    if (!elevated)
        return result("SKIP: administrator permission required for real Wintun integration\n", 77);
    try {
        UdpSocket winsock;
        const quint32 net = 0x0afdf800;
        const auto before = existingAddresses();
        check(subnetConflict(net, adapters()).isEmpty(),
              "isolated test subnet does not overlap existing networks");
        AccessPlan plan{net, {{net | 2, "02:00:00:00:00:02", "synthetic-ethernet"}}};
        AccessAdapter tun;
        check(tun.open(plan, "ConsoleLAN Test"), qPrintable(tun.error));
        const auto adapterIndex = tun.index();
        check(adapterIndex != 0, "native adapter has interface index");
        SOCKADDR_INET destination{};
        destination.Ipv4.sin_family = AF_INET;
        destination.Ipv4.sin_addr.s_addr = htonl(net | 2);
        MIB_IPFORWARD_ROW2 best{};
        SOCKADDR_INET source{};
        check(GetBestRoute2(nullptr, 0, nullptr, &destination, 0, &best, &source) == NO_ERROR &&
                  best.InterfaceIndex == adapterIndex && ntohl(source.Ipv4.sin_addr.s_addr) == (net | 1),
              "only console host route selects virtual PC address");
        const QByteArray payload = QByteArray("ConsoleLAN FTP integration\n").repeated(200);
        auto client = std::async(std::launch::async, [&] {
            Socket control;
            control.connectTo(net | 2, 21);
            check(control.line().startsWith("220"), "FTP greeting over real Windows TCP");
            control.write("USER anonymous\r\n");
            check(control.line().startsWith("331"), "FTP USER response");
            control.write("PASS test\r\n");
            check(control.line().startsWith("230"), "FTP login response");
            control.write("PASV\r\n");
            check(control.line().startsWith("227"), "FTP passive endpoint response");
            Socket data;
            data.connectTo(net | 2, 21210);
            control.write("RETR test.bin\r\n");
            check(control.line().startsWith("150"), "FTP RETR response");
            QByteArray downloaded;
            char buffer[4096];
            for (;;) {
                auto n = recv(data.s, buffer, sizeof(buffer), 0);
                if (n == 0)
                    break;
                if (n < 0)
                    throw std::runtime_error("FTP data transfer timeout");
                downloaded.append(buffer, n);
            }
            check(downloaded == payload, "passive FTP binary data matches across multiple IP packets");
            check(control.line().startsWith("226"), "FTP transfer complete");
            control.write("PASV\r\n");
            check(control.line().startsWith("227"), "FTP second passive endpoint response");
            Socket upload;
            upload.connectTo(net | 2, 21211);
            control.write("STOR upload.bin\r\n");
            check(control.line().startsWith("150"), "FTP STOR response");
            upload.write(payload);
            shutdown(upload.s, SD_SEND);
            check(control.line().startsWith("226"), "FTP upload complete");
            control.write("QUIT\r\n");
            check(control.line().startsWith("221"), "FTP QUIT response");
        });
        struct Flow {
            quint16 server = 0, client = 0;
            quint32 seq = 10000, ack = 0;
            bool ready = false, greeting = false;
        };
        std::map<quint16, Flow> flows;
        bool retrieve = false, transferred = false, storing = false, uploaded = false;
        int passive = 0;
        QByteArray uploadBytes;
        auto deadline = clockMs() + 15000;
        auto sendResponse = [&](Flow &f, quint8 flags, QByteArray data = {}) {
            const auto ip = tcp(net | 2, net | 1, f.server, f.client, f.seq, f.ack, flags, data);
            const auto frame = QByteArray::fromHex(AccessMac.toLatin1()) +
                               QByteArray::fromHex(plan.targets[0].mac.toLatin1()) +
                               QByteArray::fromHex("0800") + ip;
            auto response = accessFromEthernet(plan, "synthetic-ethernet", frame);
            check(!response.ip.isEmpty() && tun.send(response.ip),
                  "synthetic Ethernet response enters real Windows stack");
            f.seq += quint32(data.size()) + ((flags & 3) ? 1 : 0);
        };
        while (clockMs() < deadline &&
               client.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            for (int count = 0; count < 128; ++count) {
                auto b = tun.receive();
                if (!b)
                    break;
                auto frame = accessToEthernet(plan, *b);
                if (!frame)
                    continue;
                const int ihl = (quint8((*b)[0]) & 15) * 4;
                if (b->size() < ihl + 20 || quint8((*b)[9]) != 6)
                    continue;
                const auto port = be16(*b, ihl + 2), local = be16(*b, ihl);
                if (port != 21 && port != 21210 && port != 21211)
                    continue;
                auto &f = flows[port];
                f.server = port;
                f.client = local;
                const auto flags = quint8((*b)[ihl + 13]);
                const auto seq = be32(*b, ihl + 4);
                const int header = (quint8((*b)[ihl + 12]) >> 4) * 4;
                auto data = b->mid(ihl + header);
                if (flags & 4)
                    continue;
                if ((flags & 2) && !(flags & 16)) {
                    f.ack = seq + 1;
                    sendResponse(f, 0x12);
                    continue;
                }
                f.ready = true;
                if (port == 21 && !f.greeting) {
                    sendResponse(f, 0x18, "220 ConsoleLAN synthetic FTP peer\r\n");
                    f.greeting = true;
                }
                if (!data.isEmpty()) {
                    f.ack = seq + quint32(data.size());
                    sendResponse(f, 0x10);
                    if (port == 21211 && storing)
                        uploadBytes += data;
                    else if (data.startsWith("USER"))
                        sendResponse(f, 0x18, "331 Password\r\n");
                    else if (data.startsWith("PASS"))
                        sendResponse(f, 0x18, "230 Logged in\r\n");
                    else if (data.startsWith("PASV")) {
                        ++passive;
                        sendResponse(f, 0x18,
                                     passive == 1 ? "227 Entering Passive Mode (10,253,248,2,82,218)\r\n"
                                                  : "227 Entering Passive Mode (10,253,248,2,82,219)\r\n");
                    } else if (data.startsWith("RETR")) {
                        sendResponse(f, 0x18, "150 Opening data\r\n");
                        retrieve = true;
                    } else if (data.startsWith("STOR")) {
                        sendResponse(f, 0x18, "150 Ready for upload\r\n");
                        storing = true;
                    } else if (data.startsWith("QUIT"))
                        sendResponse(f, 0x18, "221 Bye\r\n");
                }
                if (flags & 1) {
                    f.ack = seq + quint32(data.size()) + 1;
                    sendResponse(f, 0x10);
                    if (port == 21211 && storing) {
                        check(uploadBytes == payload, "uploaded binary matches across Windows TCP packets");
                        sendResponse(flows[21], 0x18, "226 Upload complete\r\n");
                        uploaded = true;
                    }
                }
            }
            if (retrieve && !transferred && flows[21210].ready) {
                auto &data = flows[21210];
                for (int i = 0; i < payload.size(); i += 1200)
                    sendResponse(data, 0x18, payload.mid(i, 1200));
                sendResponse(data, 0x11);
                sendResponse(flows[21], 0x18, "226 Transfer complete\r\n");
                transferred = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        client.get();
        check(transferred && uploaded, "FTP download and upload use independent passive TCP connections");
        tun.close();
        auto until = clockMs() + 3000;
        while (clockMs() < until && hasRoute(adapterIndex))
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        check(!hasRoute(adapterIndex), "all test interface routes removed on close");
        const auto remaining = adapters();
        check(std::none_of(remaining.cbegin(), remaining.cend(),
                           [](const Adapter &a) { return a.name == "ConsoleLAN Test"; }),
              "test adapter removed on close");
        {
            HelperPeer helper;
            check(pipeSend(helper.pipe, {{"stop", true}}), "explicit disable reaches helper");
            helper.wait();
        }
        assertHelperClean();
        {
            HelperPeer helper;
            helper.disconnect();
            helper.wait();
        }
        assertHelperClean();
        {
            HelperPeer helper;
            helper.wait();
        } // No heartbeat: the helper must clean up within four seconds.
        assertHelperClean();
        const auto handoff = QDir::tempPath() + "/ConsoleLAN-test-parent-" +
                             QUuid::createUuid().toString(QUuid::WithoutBraces) + ".txt";
        HANDLE owner = launchSelf("--owner \"" + handoff + "\"");
        check(WaitForSingleObject(owner, 12000) == WAIT_OBJECT_0,
              "test parent terminates without normal cleanup");
        CloseHandle(owner);
        QFile marker(handoff);
        check(marker.open(QIODevice::ReadOnly), "parent activated actual helper before crashing");
        auto helperPid = marker.readAll().toUInt();
        marker.close();
        marker.remove();
        HANDLE orphan = OpenProcess(SYNCHRONIZE, FALSE, helperPid);
        if (orphan) {
            check(WaitForSingleObject(orphan, 6500) == WAIT_OBJECT_0, "helper detects parent death");
            CloseHandle(orphan);
        }
        assertHelperClean();
        check(before == existingAddresses(), "existing Ethernet VPN and other addresses unchanged");
        return result(
            "PASS: signed Wintun adapter; real Windows TCP FTP control and passive binary download and "
            "upload; host "
            "routes; helper disable/disconnect/heartbeat timeout/parent crash cleanup; existing addresses "
            "unchanged. No physical console or hardware gameplay was tested.\n",
            0);
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << std::endl;
        return result(QString("FAIL: ") + e.what() + "\n", 1);
    }
}
