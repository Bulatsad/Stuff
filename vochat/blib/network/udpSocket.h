#pragma once

#include <blib/network/socket.h>

namespace blib
{
    namespace network
    {
        class UdpSocket
        {
        private:
            Socket socket;
        public:
            UdpSocket();
            SocketStatus bind(Address& addr);
            SocketStatus send(Address& addr, const void* data, int size);
            SocketStatus recv(Address& addr, void* data, int& szie);
        };
    }
}
