#pragma once

namespace blib
{
    namespace network
    {
        enum class AddressType
        {
            Mac,
            IPv4,
            IPv6,

            END_OF_ENUM
        }; 

        class Address
        {
            Address();
            Address(AddressType _type);
            
        private:
            AddressType type;
            void* ctx;
        };
    }
}
