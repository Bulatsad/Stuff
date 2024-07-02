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
            TcpSocket(AddressType type);

            bool setBlocking(bool isBlocking);

            SocketStatus bind(Address& addr);
            SocketStatus connect(Address& addr);
            SocketStatus send(Address& addr, const void* data, int size);
            SocketStatus recv(Address& addr, void* data, int& szie);
        };
    }
}
