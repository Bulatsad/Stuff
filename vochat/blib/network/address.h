#pragma once

namespace blib
{
    namespace network
    {
        enum class AddressType
        {
            IPv4,
            IPv6,
            AppleTalk,
            NetBios,
            IRDA,
            Bluetooth,

            END_OF_ENUM
        }; 

        class Address
        {
        public:
            static Address fromIPv4(const char* str, bool* ok = nullptr);

            Address();
            Address(AddressType _type);
        private:
            AddressType type;
            void* ctx;
        };
    }
}
