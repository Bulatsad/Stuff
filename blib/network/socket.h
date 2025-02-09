#pragma once

#include <blib/network/address.h>

namespace blib
{
    namespace network
    {
        enum class SocketStatus
        {
            OK,
            Partial, //Socket sent only part of data
            Disconnected,

            Error,

            END_OF_ENUM
        };

        enum class SocketType
        {
            Stram,
            Dgram,
            Raw,
            RDM,
            SeqPacket,

            END_OF_ENUM
        };

        enum class SocketProtocol
        {
            ICMP = 1,
            IGMP,
            RFCOMM,
            TCP,
            UDP,
            ICMPv6,
            RM,

            END_OF_ENUM
        };

        void InitBlibSocket();

        class Socket
        {
        public:
            Socket();
            SocketStatus create(const AddressType af, const SocketType type, const SocketProtocol protocol);
            SocketStatus create(void* ctx);

            bool setBlocking(bool isBlocking);

            SocketStatus bind(Address& addr);

            SocketStatus close();
            void destroy();
            ~Socket();

            //SocketStatus connetc(Address& addr); //TODO: make const arg
            //SocketStatus bind(Address& addr); //TODO: make const arg
            //Socket accept(Address& addr);
            //SocketStatus listen(int backlog = 200);
            //SocketStatus send(const void* , size_t size);
            //SocketStatus receive(void*, size_t size, size_t& received);

            Socket(const Socket&) = delete;
            Socket(Socket&&) = delete;

            void* __getHandler();
        private:
            void* ctx;
        };
 
    }
}
