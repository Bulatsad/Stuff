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

        class Socket
        {
            Status connetc(const Address& addr);
            Status bind(const Address& addr);
            Socket accept(Address);
            Status listen();
            Status send(const void*, size_t size);
            Status receive(void*, size_t size);
        };
 
    }
}
