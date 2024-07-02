#pragma once

#include <WinSock2.h>

#include <blib/network/tcpListener.h>
#include <blib/network/impl/win/winNetworkUtil.h>

blib::network::SocketStatus blib::network::TcpListener::bind(Address& addr)
{
    return this->socket.bind(addr);
}

blib::network::SocketStatus blib::network::TcpListener::listen(int backlog)
{
    int result = ::listen(*__blib_cast_socket_handler(this->socket.__getHandler()), backlog);
    if (result == NO_ERROR)
        return SocketStatus::OK;

    //auto a = WSAGetLastError();
    return SocketStatus::Error;
}
