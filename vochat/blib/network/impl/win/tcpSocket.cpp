#include <WinSock2.h>
#include <Ws2tcpip.h>

#include <blib/network/tcpSocket.h>
#include <blib/network/impl/win/winNetworkUtil.h>

blib::network::TcpSocket::TcpSocket()
{
}

blib::network::TcpSocket::TcpSocket(AddressType type)
{
    this->socket.create(type, SocketType::Stram, SocketProtocol::TCP);
    this->setBlocking(true);
}

bool blib::network::TcpSocket::setBlocking(bool isBlocking)
{
    return this->socket.setBlocking(isBlocking);
}

blib::network::SocketStatus blib::network::TcpSocket::bind(Address& addr)
{
    return this->socket.bind(addr);
}

blib::network::SocketStatus blib::network::TcpSocket::connect(Address& addr)
{
    AddressType type = addr.getType();
    int result = !NO_ERROR;

    switch (type)
    {
    case blib::network::AddressType::IPv4:
    {
        result = ::connect(*__blib_cast_socket_handler(this->socket.__getHandler()),
            __blib_cast_address_handler(addr.__getHandler()),
            sizeof(platform_socket_internet_address_handler_t)
        );
        break;
    }
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

    if (result != SOCKET_ERROR)
        return SocketStatus::OK;
    //auto a = WSAGetLastError();
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::TcpSocket::send(const void* data, int size)
{
    if (!data || (size == 0))
    {
        return SocketStatus::Error;
    }

    int result = 0;
    for (int sent = 0; sent < size; sent += result)
    {
        result = ::send(*__blib_cast_socket_handler(this->socket.__getHandler()),
            reinterpret_cast<const char*>(data) + sent,
            static_cast<int>(size - sent),
            0
        );

        if (result == SOCKET_ERROR)
        {
            //auto a = WSAGetLastError();
            return SocketStatus::Partial;
        }
    }

    return SocketStatus::OK;
}

blib::network::SocketStatus blib::network::TcpSocket::recv(void* data, int& size)
{
    int result = ::recv(*__blib_cast_socket_handler(this->socket.__getHandler()), reinterpret_cast<char*>(data), size, 0);

    if (result == SOCKET_ERROR)
    {
        auto a = WSAGetLastError();
        return SocketStatus::Error;
    }

    if (result == NO_ERROR)
    {
        return SocketStatus::Disconnected;
    }

    if (result == size)
    {
        return SocketStatus::OK;
    }

    return SocketStatus::Partial;
}

blib::network::Socket* blib::network::TcpSocket::getSocket()
{
    return &(this->socket);
}


