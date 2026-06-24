#pragma once

#include <blib/network/socket.h>

namespace blib
{
    namespace network
    {
        class TcpSocket
        {
        private:
            Socket socket;
        public:
            TcpSocket();
            TcpSocket(AddressType type);
            TcpSocket(const TcpSocket&) = delete;
            TcpSocket(TcpSocket&&) = delete;

            bool setBlocking(bool isBlocking);

            SocketStatus bind(Address& addr);
            SocketStatus connect(Address& addr);
            SocketStatus send(const void* data, int size);
            SocketStatus recv(void* data, int& szie);

            Socket* getSocket();
        };
    }
}
