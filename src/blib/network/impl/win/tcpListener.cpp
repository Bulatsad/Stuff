#pragma once

#include <WinSock2.h>

#include <blib/network/tcpListener.h>
#include <blib/network/impl/win/winNetworkUtil.h>

blib::network::TcpListener::TcpListener()
{
}

blib::network::TcpListener::TcpListener(AddressType type)
{
    this->socket.create(type, SocketType::Stram, SocketProtocol::TCP);
    setBlocking(true);
}

bool blib::network::TcpListener::setBlocking(bool isBlocking)
{
    return this->socket.setBlocking(isBlocking);
}

blib::network::SocketStatus blib::network::TcpListener::bind(Address& addr)
{
    return this->socket.bind(addr);
}

blib::network::SocketStatus blib::network::TcpListener::listen(int backlog)
{
    int result = ::listen(*__blib_cast_socket_handler(this->socket.__getHandler()), backlog);
    if (result == NO_ERROR)
        return SocketStatus::OK;

    auto a = WSAGetLastError();
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::TcpListener::accept(blib::network::TcpSocket&accepted)
{
    platform_socket_address_handler_t addr;
    int addrlen = sizeof(platform_socket_internet_address_handler_t);
    platform_socket_handler_t socket = ::accept(*__blib_cast_socket_handler(this->socket.__getHandler()),
        &addr, 
        &addrlen
    );

    if (socket == INVALID_SOCKET)
    {
        //auto a = WSAGetLastError();
        return SocketStatus::Error;
    }

    accepted.getSocket()->create(&socket);
    return SocketStatus::OK;
}
