#include <WinSock2.h>
#include <Ws2tcpip.h>

#include <blib/network/udpSocket.h>
#include <blib/network/impl/win/winNetworkUtil.h>

blib::network::UdpSocket::UdpSocket(address::AddressType type)
{
    this->socket.create(type, SocketType::Dgram, SocketProtocol::UDP);
    setBlocking(true);
}

bool blib::network::UdpSocket::setBlocking(bool isBlocking)
{
    return this->socket.setBlocking(isBlocking);
}

blib::network::SocketStatus blib::network::UdpSocket::bind(_In const address::Udp& endpoint)
{
    return this->socket.bind(endpoint.ip, endpoint.port);
}

blib::network::SocketStatus blib::network::UdpSocket::send(_In const address::Udp& endpoint, const void* data, int size)
{
    // Конвертация в sockaddr_in (только IPv4; IPv6 — TODO в NETWORK.md)
    sockaddr_in sockAddress;
    if (!blibToSockaddr(endpoint.ip, endpoint.port, sockAddress))
    {
        return SocketStatus::Error;
    }

    int result = ::sendto(
        *(__blib_cast_socket_handler(this->socket.__getHandler())),
        (const char*)data, (int)size,
        0,
        reinterpret_cast<const sockaddr*>(&sockAddress),
        sizeof(sockaddr_in)
    );

    if (result != SOCKET_ERROR)
        return SocketStatus::OK;
    //auto a = WSAGetLastError();
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::UdpSocket::recv(_In_Out address::Udp& endpoint, void* data, int& size)
{
    sockaddr_in from;
    int fromlen = sizeof(from);
    int result = recvfrom(
        *(__blib_cast_socket_handler(this->socket.__getHandler())),
        (char*)data, size, 0,
        reinterpret_cast<sockaddr*>(&from), &fromlen
    );

    // Адрес и порт отправителя (IPv4; прочие семейства — UNDEFINED/0)
    blibFromSockaddr(from, endpoint.ip, endpoint.port);

    if (result != SOCKET_ERROR)
        return SocketStatus::OK;

    //auto a = WSAGetLastError();
    return SocketStatus::Error;
}
