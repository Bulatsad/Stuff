#include <WinSock2.h>

#include <blib/network/socket.h>
#include <blib/network/impl/win/util.h>

struct WinCtx
{
    SOCKET hSocket = INVALID_SOCKET;
};

#define __blib_this_context(__this) reinterpret_cast<WinCtx*>((__this)->ctx)

blib::network::Socket::Socket(const AddressType af, const SocketType type, const SocketProtocol protocol)
{
    this->ctx = new WinCtx;
    __blib_this_context(this)->hSocket = ::socket(blibToWinApi(af), blibToWinApi(type), blibToWinApi(protocol));
}

blib::network::Socket::~Socket()
{
    delete this->ctx;
}

blib::network::Status blib::network::Socket::connetc(const Address& addr)
{
    int result = ::connect(__blib_this_context(this)->hSocket,&addr,sizeof(sockaddr))
}

#undef __blib_this_context
