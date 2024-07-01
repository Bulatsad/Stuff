#include <WinSock2.h>

#include <blib/network/socket.h>
#include <blib/network/impl/win/winNetworkUtil.h>

#include <stdio.h>

struct WinCtx
{
    SOCKET hSocket = INVALID_SOCKET;
};

#define __blib_this_context(__this) reinterpret_cast<WinCtx*>((__this)->ctx)

blib::network::Socket::Socket()
{
    this->closed = true;
    this->ctx = nullptr;
}

blib::network::SocketStatus blib::network::Socket::create(const AddressType af, const SocketType type, const SocketProtocol protocol)
{
    this->ctx = new WinCtx;
    __blib_this_context(this)->hSocket = ::socket(blibToWinApi(af), blibToWinApi(type), blibToWinApi(protocol));
    if (__blib_this_context(this)->hSocket != INVALID_SOCKET)
        return SocketStatus::OK;
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::Socket::close()
{
    int result = ::closesocket(__blib_this_context(this)->hSocket);
    if (result == NO_ERROR)
        return SocketStatus::OK;
    return SocketStatus::Error;
}

void blib::network::Socket::destroy()
{
    if (!this->closed)
        this->close();
    delete this->ctx;
}

blib::network::Socket::~Socket()
{
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
