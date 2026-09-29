#include <WinSock2.h>

#include <blib/network/address.h>
#include <blib/network/impl/win/winNetworkUtil.h>
#include <blib/system/memory/globalAllocator.h>

const blib::network::Address blib::network::Address::AnyIPv4       = blib::network::Address::fromIPv4("0.0.0.0");
const blib::network::Address blib::network::Address::NoneIPv4      = blib::network::Address::fromIPv4("255.255.255.255");
const blib::network::Address blib::network::Address::LocalhostIPv4 = blib::network::Address::fromIPv4("127.0.0.1");
const blib::network::Address blib::network::Address::BroadcastIPv4 = blib::network::Address::fromIPv4("255.255.255.255");

#define __blib_this_context(__this) reinterpret_cast<platform_socket_address_handler_t*>((__this)->ctx)

namespace
{
    // Аллокация платформенного хендла адреса через GlobalAllocator
    // (правило проекта: выделяющие new/delete запрещены)
    void* allocateAddressHandler()
    {
        void* memory = blib::memory::GlobalAllocator::instance().allocate(sizeof(platform_socket_address_handler_t));
        if (memory != nullptr)
        {
            memset(memory, 0, sizeof(platform_socket_address_handler_t));
        }
        return memory;
    }
}

blib::network::Address blib::network::Address::fromIPv4(const char* str, bool* ok)
{
    if (ok)
    {
        *ok = false;
    }

    const u_long inetAddr = inet_addr(str);
    if (inetAddr == INADDR_NONE)
    {
        // Пустой адрес (ANY): статические AnyIPv4/NoneIPv4 и т.п. в этот
        // момент могут быть ещё не инициализированы (static-init order) —
        // возвращать их нельзя
        return Address();
    }

    Address res;
    __blib_cast_internet_address_handler(res.__getHandler())->sin_family = blibToWinApi(AddressType::IPv4);
    __blib_cast_internet_address_handler(res.__getHandler())->sin_addr.s_addr = inetAddr;

    if (ok)
    {
        *ok = true;
    }

    return res;
}

blib::network::Address::Address()
{
    this->ctx = allocateAddressHandler();
}

blib::network::Address::Address(AddressType _type)
{
    this->ctx = allocateAddressHandler();
    __blib_this_context(this)->sa_family = blibToWinApi(_type);
}

blib::network::Address::Address(const Address& other)
{
    this->ctx = allocateAddressHandler();
    if (this->ctx != nullptr && other.ctx != nullptr)
    {
        memcpy(this->ctx, other.ctx, sizeof(platform_socket_address_handler_t));
    }
}

blib::network::Address& blib::network::Address::operator=(const Address& other)
{
    if (this != &other)
    {
        memcpy(this->ctx, other.ctx, sizeof(platform_socket_address_handler_t));
    }
    return *this;
}

blib::network::Address::Address(Address&& other) noexcept
{
    // Перенос владения: хендл переезжает, источник обнуляется
    this->ctx = other.ctx;
    other.ctx = nullptr;
}

blib::network::Address& blib::network::Address::operator=(Address&& other) noexcept
{
    if (this != &other)
    {
        blib::memory::GlobalAllocator::instance().deallocate(
            this->ctx, sizeof(platform_socket_address_handler_t));
        this->ctx = other.ctx;
        other.ctx = nullptr;
    }
    return *this;
}

blib::network::Address::~Address()
{
    if (this->ctx != nullptr)
    {
        blib::memory::GlobalAllocator::instance().deallocate(
            this->ctx, sizeof(platform_socket_address_handler_t));
        this->ctx = nullptr;
    }
}

void blib::network::Address::setPort(int port)
{
    // Сетевой порядок байт (ранее порт записывался сырым — баг)
    reinterpret_cast<sockaddr_in*>(__blib_this_context(this))->sin_port = htons(static_cast<u_short>(port));
}

blib::network::AddressType blib::network::Address::getType() const
{
    return blibWinApiToBlib(__blib_this_context(this)->sa_family);
}

void* blib::network::Address::__getHandler()
{
    return __blib_this_context(this);
}
