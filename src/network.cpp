#include "network.hpp"
#include <iphlpapi.h>
#include <netioapi.h>
namespace cl {
static QString socketError(QString action) {
    return action + " (Winsock " + QString::number(WSAGetLastError()) + ")";
}
QList<Adapter> adapters() {
    ULONG size = 16384;
    QByteArray bytes(size, 0);
    ULONG rc = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr,
                                    reinterpret_cast<IP_ADAPTER_ADDRESSES *>(bytes.data()), &size);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        bytes.resize(size);
        rc = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr,
                                  reinterpret_cast<IP_ADAPTER_ADDRESSES *>(bytes.data()), &size);
    }
    QList<Adapter> out;
    if (rc != NO_ERROR)
        return out;
    for (auto a = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(bytes.data()); a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;
        auto label = QString::fromWCharArray(a->Description);
        auto low = label.toLower();
        bool phy = a->IfType == IF_TYPE_ETHERNET_CSMACD && !low.contains("virtual") && !low.contains("vpn") &&
                   !low.contains("tap-") && !low.contains("wireguard") && !low.contains("zerotier");
        bool added = false;
        for (auto u = a->FirstUnicastAddress; u; u = u->Next)
            if (u->Address.lpSockaddr && u->Address.lpSockaddr->sa_family == AF_INET) {
                auto sin = reinterpret_cast<sockaddr_in *>(u->Address.lpSockaddr);
                auto ip = ntohl(sin->sin_addr.s_addr);
                out.append({QString::fromLatin1(a->AdapterName), QString::fromWCharArray(a->FriendlyName),
                            label, ip, a->IfIndex, int(u->OnLinkPrefixLength),
                            a->OperStatus == IfOperStatusUp, phy,
                            QByteArray(reinterpret_cast<char *>(a->PhysicalAddress), a->PhysicalAddressLength)
                                .toHex(':')});
                added = true;
            }
        if (!added)
            out.append({QString::fromLatin1(a->AdapterName),
                        QString::fromWCharArray(a->FriendlyName),
                        label,
                        0,
                        a->IfIndex,
                        0,
                        a->OperStatus == IfOperStatusUp,
                        phy,
                        {}});
    }
    return out;
}
QString subnetConflict(quint32 subnet, const QList<Adapter> &all) {
    auto privateIp = (subnet >> 24) == 10 || (subnet >> 20) == 0xac1 || (subnet >> 16) == 0xc0a8;
    if (!privateIp || (subnet & 255))
        return "Use una red privada /24 con último octeto 0, por ejemplo 10.253.42.0.";
    for (auto &a : all) {
        if (!a.ip || a.prefix == 0)
            continue;
        quint32 mask = 0xffffffffu << (32 - std::clamp(a.prefix, 1, 32));
        if ((a.ip & mask) == (subnet & mask) || (a.ip & 0xffffff00) == subnet)
            return "La subred coincide con " + a.name + " (" + ipText(a.ip) + "/" +
                   QString::number(a.prefix) + ").";
    }
    PMIB_IPFORWARD_TABLE2 table = nullptr;
    if (GetIpForwardTable2(AF_INET, &table) == NO_ERROR) {
        QString conflict;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            auto &r = table->Table[i];
            auto len = r.DestinationPrefix.PrefixLength;
            if (len == 0)
                continue;
            auto route = ntohl(r.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr);
            auto mask = 0xffffffffu << (32 - std::clamp(int(len), 1, 32));
            if ((subnet & mask) == (route & mask) || (route & 0xffffff00) == subnet) {
                conflict =
                    "La subred solapa una ruta de Windows: " + ipText(route) + "/" + QString::number(len);
                break;
            }
        }
        FreeMibTable(table);
        if (!conflict.isEmpty())
            return conflict;
    }
    return {};
}
UdpSocket::UdpSocket() {
    WSADATA d{};
    if (WSAStartup(MAKEWORD(2, 2), &d))
        error = "WSAStartup falló";
}
UdpSocket::~UdpSocket() {
    close();
    WSACleanup();
}
void UdpSocket::close() {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
}
bool UdpSocket::open(const Adapter &a, quint16 port, bool discovery) {
    close();
    error.clear();
    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        error = socketError("socket");
        return false;
    }
    auto fail = [&](QString text) {
        error = socketError(text);
        close();
        return false;
    };
    BOOL yes = TRUE;
    if (setsockopt(socket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char *>(&yes), sizeof(yes)))
        return fail("SO_EXCLUSIVEADDRUSE");
    if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char *>(&yes), sizeof(yes)))
        return fail("SO_BROADCAST");
    DWORD idx = htonl(a.index);
    if (setsockopt(socket_, IPPROTO_IP, IP_UNICAST_IF, reinterpret_cast<char *>(&idx), sizeof(idx)))
        return fail("IP_UNICAST_IF");
    if (setsockopt(socket_, IPPROTO_IP, IP_PKTINFO, reinterpret_cast<char *>(&yes), sizeof(yes)))
        return fail("IP_PKTINFO");
    GUID guid = WSAID_WSARECVMSG;
    DWORD returned = 0;
    if (WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &recvMsg_,
                 sizeof(recvMsg_), &returned, nullptr, nullptr))
        return fail("WSARecvMsg no disponible");
    interface_ = a.index;
    if (setsockopt(socket_, IPPROTO_IP, IP_DONTFRAGMENT, reinterpret_cast<char *>(&yes), sizeof(yes)))
        return fail("IP_DONTFRAGMENT");
    int buffer = 262144;
    setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char *>(&buffer), sizeof(buffer));
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = discovery ? INADDR_ANY : htonl(a.ip);
    local.sin_port = htons(port);
    if (bind(socket_, reinterpret_cast<sockaddr *>(&local), sizeof(local)))
        return fail("bind (IP no disponible o puerto ocupado)");
    u_long nonblock = 1;
    if (ioctlsocket(socket_, FIONBIO, &nonblock))
        return fail("FIONBIO");
    int size = sizeof(local);
    getsockname(socket_, reinterpret_cast<sockaddr *>(&local), &size);
    bound = {a.ip, ntohs(local.sin_port)};
    return true;
}
bool UdpSocket::send(const Datagram &d) {
    if (socket_ == INVALID_SOCKET)
        return false;
    sockaddr_in ep{};
    ep.sin_family = AF_INET;
    ep.sin_addr.s_addr = htonl(d.endpoint.ip);
    ep.sin_port = htons(d.endpoint.port);
    auto n = sendto(socket_, d.bytes.constData(), int(d.bytes.size()), 0, reinterpret_cast<sockaddr *>(&ep),
                    sizeof(ep));
    if (n != d.bytes.size()) {
        error = socketError("sendto: compruebe VPN y presupuesto MTU");
        return false;
    }
    return true;
}
std::optional<Datagram> UdpSocket::receive() {
    if (socket_ == INVALID_SOCKET)
        return {};
    char b[2048];
    sockaddr_in ep{};
    alignas(WSACMSGHDR) char control[WSA_CMSG_SPACE(sizeof(IN_PKTINFO))]{};
    WSABUF buffer{sizeof(b), b};
    WSAMSG message{};
    message.name = reinterpret_cast<sockaddr *>(&ep);
    message.namelen = sizeof(ep);
    message.lpBuffers = &buffer;
    message.dwBufferCount = 1;
    message.Control = {sizeof(control), control};
    DWORD received = 0;
    if (recvMsg_(socket_, &message, &received, nullptr, nullptr) == SOCKET_ERROR) {
        auto e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK && e != WSAECONNRESET && e != WSAEMSGSIZE)
            error = socketError("WSARecvMsg");
        return {};
    }
    bool correct = false;
    for (auto c = WSA_CMSG_FIRSTHDR(&message); c; c = WSA_CMSG_NXTHDR(&message, c))
        if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_PKTINFO &&
            c->cmsg_len >= WSA_CMSG_LEN(sizeof(IN_PKTINFO))) {
            auto info = reinterpret_cast<IN_PKTINFO *>(WSA_CMSG_DATA(c));
            correct = info->ipi_ifindex == interface_;
        }
    if (!correct || (message.dwFlags & (MSG_TRUNC | MSG_CTRUNC)))
        return {};
    return Datagram{{ntohl(ep.sin_addr.s_addr), ntohs(ep.sin_port)}, QByteArray(b, int(received))};
}
} // namespace cl
