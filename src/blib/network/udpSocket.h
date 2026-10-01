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
            UdpSocket(address::AddressType type);

            bool setBlocking(bool isBlocking);

            SocketStatus bind(_In const address::Udp& endpoint);
            SocketStatus send(_In const address::Udp& endpoint, const void* data, int size);
            SocketStatus recv(_In_Out address::Udp& endpoint, void* data, int& szie);
        };
    }
}
