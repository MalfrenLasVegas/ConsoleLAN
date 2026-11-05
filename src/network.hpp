#pragma once
#include "common.hpp"
#include "room.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
namespace cl {
struct Adapter {
    QString id, name, description;
    quint32 ip = 0, index = 0;
    int prefix = 0;
    bool up = false, physical = false;
    QString mac;
};
QList<Adapter> adapters();
QString subnetConflict(quint32 subnet, const QList<Adapter> &all);
class UdpSocket {
    SOCKET socket_ = INVALID_SOCKET;
    LPFN_WSARECVMSG recvMsg_ = nullptr;
    quint32 interface_ = 0;

  public:
    Endpoint bound;
    QString error;
    UdpSocket();
    ~UdpSocket();
    bool open(const Adapter &adapter, quint16 port = 32190, bool discovery = false);
    void close();
    bool send(const Datagram &d);
    std::optional<Datagram> receive();
    bool isOpen() const {
        return socket_ != INVALID_SOCKET;
    }
};
} // namespace cl
