#include <WinSock2.h>
#include <Ws2tcpip.h>

#include <blib/network/udpSocket.h>
#include <blib/network/impl/win/winNetworkUtil.h>

blib::network::UdpSocket::UdpSocket()
{
    this->socket.create(AddressType::IPv4, SocketType::Dgram, SocketProtocol::UDP);
    setBlocking(true);
}

bool blib::network::UdpSocket::setBlocking(bool isBlocking)
{
    u_long arg = isBlocking ? 0 : 1;
    int res = ioctlsocket(*__blib_cast_socket_handler(this->socket.__getHandler()), FIONBIO, &arg);
    return !!res;
}

blib::network::SocketStatus blib::network::UdpSocket::bind(Address& addr)
{
    AddressType addrtype = addr.getType();
    //this->socket.create(addrtype, SocketType::Dgram, SocketProtocol::UDP);

    int result = !NO_ERROR;
    switch (addrtype)
    {
    case blib::network::AddressType::IPv4:
    {
        result = ::bind(*__blib_cast_socket_handler(this->socket.__getHandler()),
            __blib_cast_address_handler(addr.__getHandler()),
            sizeof(sockaddr_in)
        );
    }
        break;
    case blib::network::AddressType::IPv6:
        break;
    case blib::network::AddressType::AppleTalk:
        break;
    case blib::network::AddressType::NetBios:
        break;
    case blib::network::AddressType::IRDA:
        break;
    case blib::network::AddressType::Bluetooth:
        break;
    case blib::network::AddressType::UNDEFINED:
        break;
    case blib::network::AddressType::END_OF_ENUM:
        break;
    default:
        break;
    }

    auto a = WSAGetLastError();

    if (result == NO_ERROR)
        return SocketStatus::OK;
    return SocketStatus::Error;

}

blib::network::SocketStatus blib::network::UdpSocket::send(Address& addr,const void* data, int size)
{
    int result = ::sendto(*(__blib_cast_socket_handler(this->socket.__getHandler())),
        (const char*)data, (int)size,
        0,
        __blib_cast_address_handler(addr.__getHandler()),
        sizeof(sockaddr_in)
    );

    if (result == NO_ERROR)
        return SocketStatus::OK;
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::UdpSocket::recv(Address& addr, void* data, int& size)
{
    sockaddr from;
    int fromlen = sizeof(sockaddr);
    int result = recvfrom(*(__blib_cast_socket_handler(this->socket.__getHandler())), (char*)data, size, 0, &from, &fromlen);
    
    auto a = WSAGetLastError();

    if (blibWinApiToBlib(from.sa_family) == AddressType::IPv4)
    {
        memcpy(addr.__getHandler(), &from, fromlen);
    }

    if (result == NO_ERROR)
        return SocketStatus::OK;
    return SocketStatus::Error;
}
