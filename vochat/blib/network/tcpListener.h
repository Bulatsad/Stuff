#pragma once

#include <blib/network/socket.h>
#include <blib/network/tcpSocket.h>

namespace blib
{
    namespace network
    {
        class TcpListener
        {
        private:
            Socket socket;
        public:
            TcpListener(AddressType type);

            bool setBlocking(bool isBlocking);

            SocketStatus bind(Address& addr);
            SocketStatus listen(int backlog = 16);
            TcpSocket accept();

            SocketStatus send(Address& addr, const void* data, int size);
            SocketStatus recv(Address& addr, void* data, int& szie);
        };
    }
}
