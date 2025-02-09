#include <WinSock2.h>
#include <Ws2tcpip.h>

#include <blib/network/udpSocket.h>
#include <blib/network/impl/win/winNetworkUtil.h>

blib::network::UdpSocket::UdpSocket(AddressType type)
{
    this->socket.create(type, SocketType::Dgram, SocketProtocol::UDP);
    setBlocking(true);
}

bool blib::network::UdpSocket::setBlocking(bool isBlocking)
{
    return this->socket.setBlocking(isBlocking);
}

blib::network::SocketStatus blib::network::UdpSocket::bind(Address& addr)
{
    return this->socket.bind(addr);
}

blib::network::SocketStatus blib::network::UdpSocket::send(Address& addr,const void* data, int size)
{
    int result = ::sendto(*(__blib_cast_socket_handler(this->socket.__getHandler())),
        (const char*)data, (int)size,
        0,
        __blib_cast_address_handler(addr.__getHandler()),
        sizeof(sockaddr_in)
    );

    if (result != SOCKET_ERROR)
        return SocketStatus::OK;
    //auto a = WSAGetLastError();
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::UdpSocket::recv(Address& addr, void* data, int& size)
{
    platform_socket_address_handler_t from;
    int fromlen = sizeof(platform_socket_address_handler_t);
    int result = recvfrom(*(__blib_cast_socket_handler(this->socket.__getHandler())), (char*)data, size, 0, &from, &fromlen);
    
    if (blibWinApiToBlib(from.sa_family) == AddressType::IPv4)
    {
        memcpy(addr.__getHandler(), &from, fromlen);
    }

    if (result != SOCKET_ERROR)
        return SocketStatus::OK;

    //auto a = WSAGetLastError();
    return SocketStatus::Error;
}
