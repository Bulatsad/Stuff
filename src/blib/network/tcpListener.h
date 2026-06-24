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
            TcpListener();
            TcpListener(AddressType type);
            TcpListener(const TcpListener&) = delete;
            TcpListener(TcpListener&&) = delete;

            bool setBlocking(bool isBlocking);

            SocketStatus bind(Address& addr);
            SocketStatus listen(int backlog = 16);
            SocketStatus accept(TcpSocket& accepted);
        };
    }
}
