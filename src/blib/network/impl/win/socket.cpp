#include <WinSock2.h>
#include <Ws2tcpip.h>

#include <blib/core/console/console.h>
#include <blib/network/socket.h>
#include <blib/network/impl/win/winNetworkUtil.h>
#include <blib/system/memory/globalAllocator.h>

namespace
{
    // Флаг инициализации winsock (InitBlibSocket идемпотентна)
    bool s_socketSystemInitialized = false;
}

blib::network::Socket::Socket()
{
    this->ctx = nullptr;
    this->lastError = NetworkError::None;
}

blib::network::SocketStatus blib::network::Socket::create(const address::AddressType af, const SocketType type, const SocketProtocol protocol)
{
    // Самодостаточность: конструкторы TcpSocket/TcpListener с типом
    // создают сокет ДО явного InitBlibSocket у вызывающего —
    // инициализируем систему здесь (InitBlibSocket идемпотентна)
    if (!InitBlibSocket())
    {
        this->lastError = NetworkError::NotInitialized;
        return SocketStatus::Error;
    }

    // Хендл — через GlobalAllocator (правило проекта: без new/delete)
    this->ctx = blib::memory::GlobalAllocator::instance().allocate(sizeof(platform_socket_handler_t));
    if (this->ctx == nullptr)
    {
        this->lastError = NetworkError::CreateFailed;
        return SocketStatus::Error;
    }

    *__blib_cast_socket_handler(this->ctx) = INVALID_SOCKET;
    *__blib_cast_socket_handler(this->ctx) = ::socket(blibToWinApi(af), blibToWinApi(type), blibToWinApi(protocol));
    if (*__blib_cast_socket_handler(this->ctx) != INVALID_SOCKET)
    {
        return SocketStatus::OK;
    }

    this->lastError = NetworkError::CreateFailed;
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::Socket::create(void* ctx)
{
    this->ctx = blib::memory::GlobalAllocator::instance().allocate(sizeof(platform_socket_handler_t));
    if (this->ctx == nullptr)
    {
        this->lastError = NetworkError::CreateFailed;
        return SocketStatus::Error;
    }
    memcpy(this->ctx, ctx, sizeof(platform_socket_handler_t));
    return SocketStatus::OK;
}

bool blib::network::Socket::setBlocking(bool isBlocking)
{
    u_long arg = isBlocking ? 0 : 1;
    int res = ioctlsocket(*__blib_cast_socket_handler(this->ctx), FIONBIO, &arg);
    if (res == NO_ERROR)
    {
        return true;
    }
    this->lastError = NetworkError::Unknown;
    return false;
}

bool blib::network::Socket::setTcpNoDelay(bool enable)
{
    if (this->ctx == nullptr)
    {
        this->lastError = NetworkError::Unknown;
        return false;
    }

    // TCP_NODELAY = выключить алгоритм Нейгла: мелкие real-time пакеты
    // (команды ввода, снапшоты) не ждут накопления/ACK
    const BOOL value = enable ? TRUE : FALSE;
    const int result = ::setsockopt(
        *__blib_cast_socket_handler(this->ctx),
        IPPROTO_TCP,
        TCP_NODELAY,
        reinterpret_cast<const char*>(&value),
        sizeof(value));
    if (result == NO_ERROR)
    {
        return true;
    }
    this->lastError = NetworkError::Unknown;
    return false;
}

blib::network::SocketStatus blib::network::Socket::bind(_In const address::Address& ip, _In buint16 port)
{
    // Конвертация в sockaddr_in (только IPv4; IPv6 — TODO в NETWORK.md)
    sockaddr_in sockAddress;
    if (!blibToSockaddr(ip, port, sockAddress))
    {
        this->lastError = NetworkError::BindFailed;
        return SocketStatus::Error;
    }

    const int result = ::bind(
        *__blib_cast_socket_handler(this->__getHandler()),
        reinterpret_cast<const sockaddr*>(&sockAddress),
        sizeof(sockaddr_in)
    );

    if (result != SOCKET_ERROR)
    {
        return SocketStatus::OK;
    }
    this->lastError = NetworkError::BindFailed;
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::Socket::close()
{
    if (this->ctx == nullptr)
    {
        return SocketStatus::OK;
    }

    int result = ::closesocket(*__blib_cast_socket_handler(this->ctx));

    // Хендл инвалидируем ВСЕГДА, даже при ошибке closesocket: ОС
    // переиспользует значения SOCKET — повторный close() на закрытом
    // сокете закрыл бы ЧУЖОЙ сокет, которому достался тот же номер
    // (классика: клиент закрыл «свой» хендл → слушатель сервера,
    // созданный позже с тем же значением, падает с WSAENOTSOCK)
    *__blib_cast_socket_handler(this->ctx) = INVALID_SOCKET;

    if (result == NO_ERROR)
    {
        return SocketStatus::OK;
    }
    this->lastError = NetworkError::Unknown;
    return SocketStatus::Error;
}

void blib::network::Socket::destroy()
{
    if (this->ctx != nullptr)
    {
        blib::memory::GlobalAllocator::instance().deallocate(this->ctx, sizeof(platform_socket_handler_t));
        this->ctx = nullptr;
    }
}

blib::network::Socket::~Socket()
{
    this->close();
    this->destroy();
}

void* blib::network::Socket::__getHandler()
{
    return this->ctx;
}

bool blib::network::InitBlibSocket()
{
    if (s_socketSystemInitialized)
    {
        return true;
    }

    WSADATA wsaData;
    const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != NO_ERROR)
    {
        __blib_log_error("WSAStartup failed with error: %d", result);
        return false;
    }

    s_socketSystemInitialized = true;
    return true;
}
