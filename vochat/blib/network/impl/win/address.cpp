#include <WinSock2.h>

#include <blib/network/address.h>
#include <blib/core/string.h>
#include <blib/network/impl/win/util.h>

#include <string>
#include <stdint.h>
#include <memory>

struct WinCtx
{
    sockaddr addr;

    WinCtx()
    {
        memset(&this->addr, 0, sizeof(this->addr));
    }
};

#define __blib_this_context(__this) reinterpret_cast<WinCtx*>((__this)->ctx)

blib::network::Address blib::network::Address::fromIPv4(const char* str, bool* ok)
{
    Address res;

    std::string ipv4(str);

    blib::core::StringList octets = blib::core::split(ipv4, ".");

    if (octets.size() != 4)
    {
        if (ok)
            *ok = false;

        return res;
    }

    for (size_t i = 0; i < octets.size(); ++i)
    {
        int iOctet = std::stoi(octets[i].c_str());
        if (iOctet <= 255 && iOctet >= 0)
        {
            __blib_this_context(&res)->addr.sa_data[i] = static_cast<uint8_t>(iOctet);
        }
        else
        {
            if (ok)
                *ok = false;

            return res;
        }
    }

    __blib_this_context(&res)->addr.sa_family = blibToWinApi(AddressType::IPv4);
    return res;
}

blib::network::Address::Address()
{
    this->ctx = new WinCtx;
}

blib::network::Address::Address(AddressType _type)
{
    this->ctx = new WinCtx;
    __blib_this_context(this)->addr.sa_family = blibToWinApi(_type);
}
