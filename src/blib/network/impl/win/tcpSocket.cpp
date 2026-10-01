#include <WinSock2.h>
#include <Ws2tcpip.h>

#include <blib/network/tcpSocket.h>
#include <blib/network/impl/win/winNetworkUtil.h>

namespace
{
    // Трансляция wsa-кода в enum-ошибку модуля (контекстная часть —
    // в вызывающем: connect/send/recv добавляют свою специфику)
    blib::network::NetworkError blibFromWsaError(int wsaError)
    {
        switch (wsaError)
        {
        case WSAEWOULDBLOCK:
            return blib::network::NetworkError::WouldBlock;
        case WSAECONNRESET:
        case WSAENOTCONN:
        case WSAECONNABORTED:
        case WSAESHUTDOWN:
            return blib::network::NetworkError::Closed;
        default:
            return blib::network::NetworkError::Unknown;
        }
    }
}

blib::network::TcpSocket::TcpSocket()
{
}

blib::network::TcpSocket::TcpSocket(address::AddressType type)
{
    this->socket.create(type, SocketType::Stream, SocketProtocol::TCP);
    this->setBlocking(true);
}

bool blib::network::TcpSocket::setBlocking(bool isBlocking)
{
    return this->socket.setBlocking(isBlocking);
}

bool blib::network::TcpSocket::setTcpNoDelay(bool enable)
{
    return this->socket.setTcpNoDelay(enable);
}

blib::network::SocketStatus blib::network::TcpSocket::bind(_In const address::Tcp& endpoint)
{
    return this->socket.bind(endpoint.ip, endpoint.port);
}

blib::network::SocketStatus blib::network::TcpSocket::connect(_In const address::Tcp& endpoint)
{
    // Конвертация в sockaddr_in (только IPv4; IPv6 — TODO в NETWORK.md)
    sockaddr_in sockAddress;
    if (!blibToSockaddr(endpoint.ip, endpoint.port, sockAddress))
    {
        this->socket.__setLastError(NetworkError::ConnectFailed);
        return SocketStatus::Error;
    }

    int result = ::connect(
        *__blib_cast_socket_handler(this->socket.__getHandler()),
        reinterpret_cast<const sockaddr*>(&sockAddress),
        sizeof(sockaddr_in)
    );

    if (result != SOCKET_ERROR)
    {
        return SocketStatus::OK;
    }

    const int wsaError = WSAGetLastError();
    if (wsaError == WSAEWOULDBLOCK)
    {
        // Неблокирующий режим: соединение устанавливается асинхронно —
        // вызывающий повторяет connect позже
        this->socket.__setLastError(NetworkError::WouldBlock);
        return SocketStatus::WouldBlock;
    }
    if (wsaError == WSAEISCONN)
    {
        // Уже подключены (повторный connect после WouldBlock)
        return SocketStatus::OK;
    }

    this->socket.__setLastError(blibFromWsaError(wsaError) == NetworkError::Closed
        ? NetworkError::Closed : NetworkError::ConnectFailed);
    return SocketStatus::Error;
}

blib::network::SocketStatus blib::network::TcpSocket::send(const void* data, int size, int* sentOut)
{
    if (!data || size <= 0)
    {
        this->socket.__setLastError(NetworkError::Unknown);
        return SocketStatus::Error;
    }

    int totalSent = 0;
    while (totalSent < size)
    {
        int result = ::send(*__blib_cast_socket_handler(this->socket.__getHandler()),
            reinterpret_cast<const char*>(data) + totalSent,
            static_cast<int>(size - totalSent),
            0
        );

        if (result == SOCKET_ERROR)
        {
            const int wsaError = WSAGetLastError();
            this->socket.__setLastError(blibFromWsaError(wsaError) == NetworkError::Unknown
                ? NetworkError::SendFailed : blibFromWsaError(wsaError));
            if (sentOut)
            {
                *sentOut = totalSent;
            }
            if (wsaError == WSAEWOULDBLOCK)
            {
                return SocketStatus::WouldBlock;
            }
            return SocketStatus::Error;
        }

        if (result == 0)
        {
            // Нулевая отправка — соединение не продвигается
            this->socket.__setLastError(NetworkError::SendFailed);
            if (sentOut)
            {
                *sentOut = totalSent;
            }
            return SocketStatus::Error;
        }

        totalSent += result;
    }

    if (sentOut)
    {
        *sentOut = totalSent;
    }
    return SocketStatus::OK;
}

blib::network::SocketStatus blib::network::TcpSocket::recv(void* data, int& size)
{
    if (!data || size <= 0)
    {
        size = 0;
        this->socket.__setLastError(NetworkError::Unknown);
        return SocketStatus::Error;
    }

    int result = ::recv(*__blib_cast_socket_handler(this->socket.__getHandler()),
        reinterpret_cast<char*>(data), size, 0);

    if (result == SOCKET_ERROR)
    {
        size = 0;
        const int wsaError = WSAGetLastError();
        this->socket.__setLastError(blibFromWsaError(wsaError) == NetworkError::Unknown
            ? NetworkError::RecvFailed : blibFromWsaError(wsaError));
        if (wsaError == WSAEWOULDBLOCK)
        {
            return SocketStatus::WouldBlock;
        }
        return SocketStatus::Error;
    }

    if (result == 0)
    {
        size = 0;
        this->socket.__setLastError(NetworkError::Closed);
        return SocketStatus::Disconnected;
    }

    // Фактический размер принятых данных (частичный приём — НЕ ошибка:
    // TCP — поток, сообщения собирает вызывающий)
    size = result;
    return SocketStatus::OK;
}

blib::network::Socket* blib::network::TcpSocket::getSocket()
{
    return &(this->socket);
}
