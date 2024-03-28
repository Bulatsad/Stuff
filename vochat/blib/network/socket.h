#pragma once

#include <blib/network/address.h>

namespace blib
{
    namespace network
    {
        enum class Status
        {
            OK,

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
            ICMP,
            IGMP,
            RFCOMM,
            TCP,
            UDP,
            ICMPv6,
            RM,

            END_OF_ENUM
        };

        class Socket
        {
        public:
            Socket(const AddressType af, const SocketType type, const SocketProtocol protocol);
            ~Socket();
            Status connetc(const Address& addr);
            Status bind(const Address& addr);
            Socket accept(Address);
            Status listen();
            Status send(const void*, size_t size);
            Status receive(void*, size_t size, size_t& received);

        private:
            void* ctx;
        };
 
    }
}
