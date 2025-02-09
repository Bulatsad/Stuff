#include <WinSock2.h>

#include <blib/network/socket.h>
#include <blib/network/impl/win/winNetworkUtil.h>

#include <stdio.h>

blib::network::Socket::Socket()
{
    this->ctx = nullptr;
}

blib::network::SocketStatus blib::network::Socket::create(const AddressType af, const SocketType type, const SocketProtocol protocol)
{
    this->ctx = new platform_socket_handler_t;
    *__blib_cast_socket_handler(this->ctx) = INVALID_SOCKET;
    *__blib_cast_socket_handler(this->ctx) = ::socket(blibToWinApi(af), blibToWinApi(type), blibToWinApi(protocol));
    if (*__blib_cast_socket_handler(this->ctx) != INVALID_SOCKET)
        return SocketStatus::OK;
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::Socket::create(void* ctx)
{
    this->ctx = new platform_socket_handler_t;
    memcpy(this->ctx, ctx, sizeof(platform_socket_handler_t));
    return SocketStatus::OK;
}

bool blib::network::Socket::setBlocking(bool isBlocking)
{
    u_long arg = isBlocking ? 0 : 1;
    int res = ioctlsocket(*__blib_cast_socket_handler(this->ctx), FIONBIO, &arg);
    return !!res;
}

blib::network::SocketStatus blib::network::Socket::bind(Address& addr)
{
    AddressType addrtype = addr.getType();
    //this->socket.create(addrtype, SocketType::Dgram, SocketProtocol::UDP);

    int result = !NO_ERROR;
    switch (addrtype)
    {
    case blib::network::AddressType::IPv4:
    {
        result = ::bind(*__blib_cast_socket_handler(this->__getHandler()),
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

blib::network::SocketStatus blib::network::Socket::close()
{
    int result = ::closesocket(*__blib_cast_socket_handler(this->ctx));
    if (result == NO_ERROR)
        return SocketStatus::OK;
    return SocketStatus::Error;
}

void blib::network::Socket::destroy()
{
    delete this->ctx;
}

blib::network::Socket::~Socket()
{
    this->close();
    this->destroy();
}

void* blib::network::Socket::__getHandler()
{
    return this->ctx;
}

#undef __blib_this_context

void blib::network::InitBlibSocket()
{
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR)
    {
        fprintf(stderr, "WSAStartup failed with error: %d\n", iResult);
    }
}
