#include <WinSock2.h>

#include <blib/network/address.h>
#include <blib/core/string.h>
#include <blib/network/impl/win/winNetworkUtil.h>

#include <string>
#include <stdint.h>
#include <memory>
#include <stdio.h>

const blib::network::Address blib::network::Address::AnyIPv4       = blib::network::Address::fromIPv4("0.0.0.0");
const blib::network::Address blib::network::Address::NoneIPv4      = blib::network::Address::fromIPv4("255.255.255.255");
const blib::network::Address blib::network::Address::LocalhostIPv4 = blib::network::Address::fromIPv4("127.0.0.1");
const blib::network::Address blib::network::Address::BroadcastIPv4 = blib::network::Address::fromIPv4("255.255.255.255");

#define __blib_this_context(__this) reinterpret_cast<platform_socket_address_handler_t*>((__this)->ctx)

blib::network::Address blib::network::Address::fromIPv4(const char* str, bool* ok)
{
    auto inetAddr = inet_addr(str);

    if (inetAddr == INADDR_NONE)
    {
        if (ok)
            *ok = false;
        Address::NoneIPv4;
    }
    
    Address res;

    __blib_cast_internet_address_handler(res.__getHandler())->sin_family = blibToWinApi(AddressType::IPv4);
    __blib_cast_internet_address_handler(res.__getHandler())->sin_addr.s_addr = inetAddr;

    if (ok)
        *ok = true;

    return res;
}

blib::network::Address::Address()
{
    this->ctx = new platform_socket_address_handler_t;
    memset(this->ctx, 0, sizeof(platform_socket_address_handler_t));
}

blib::network::Address::Address(AddressType _type)
{
    this->ctx = new platform_socket_address_handler_t;
    memset(this->ctx, 0, sizeof(platform_socket_address_handler_t));
    __blib_this_context(this)->sa_family = blibToWinApi(_type);
}

void blib::network::Address::setPort(int port)
{
    reinterpret_cast<sockaddr_in*>(__blib_this_context(this))->sin_port = port;
}

blib::network::AddressType blib::network::Address::getType() const
{
    return blibWinApiToBlib(__blib_this_context(this)->sa_family);
}

void* blib::network::Address::__getHandler()
{
    return __blib_this_context(this);
}
