// WinSock2.h обязан быть ПЕРВЫМ системным инклюдом (до windows.h из
// прочих заголовков): иначе winsock1 конфликтует с winsock2
#include <WinSock2.h>

#include <blib/test/src/test.h>

#include <blib/network/address.h>
#include <blib/network/socket.h>
#include <blib/network/tcpListener.h>
#include <blib/network/tcpSocket.h>

#include <cstring>

// Тесты сети — loopback в одном процессе (детерминированные: не
// требуют внешнего сервера). Все сокеты — неблокирующие: ожидание
// состояний — опрос с таймаутом (loopback надёжен).

namespace
{
    // Тестовый порт (маловероятно занятый)
    constexpr int testPort = 44251;

    // Буферы и сообщения
    constexpr int messageBufferSize = 256;
    constexpr int bigMessageSize = 10240;

    // Таймауты ожидания (попыток × пауза)
    constexpr int maxPollAttempts = 1000;
    constexpr int pollSleepMs = 1;

    // Ожидание условия с таймаутом (опрашивает fn до maxAttempts раз)
    template<typename Fn>
    bool waitFor(Fn fn)
    {
        for (int attempt = 0; attempt < maxPollAttempts; ++attempt)
        {
            if (fn())
            {
                return true;
            }
            Sleep(static_cast<DWORD>(pollSleepMs));
        }
        return false;
    }

    // Неблокирующее подключение клиента к слушателю: connect в
    // неблокирующем режиме асинхронен — ждём OK (WSAEISCONN)
    bool connectNonBlocking(blib::network::TcpSocket& client, blib::network::Address& address)
    {
        if (!client.setBlocking(false))
        {
            return false;
        }

        for (int attempt = 0; attempt < maxPollAttempts; ++attempt)
        {
            const blib::network::SocketStatus status = client.connect(address);
            if (status == blib::network::SocketStatus::OK)
            {
                return true;
            }
            if (status == blib::network::SocketStatus::Error)
            {
                return false;
            }
            // WouldBlock — соединение устанавливается
            Sleep(static_cast<DWORD>(pollSleepMs));
        }
        return false;
    }

    // Принятие подключения неблокирующим слушателем (WouldBlock —
    // подключений нет)
    bool acceptNonBlocking(blib::network::TcpListener& listener, blib::network::TcpSocket& accepted)
    {
        for (int attempt = 0; attempt < maxPollAttempts; ++attempt)
        {
            const blib::network::SocketStatus status = listener.accept(accepted);
            if (status == blib::network::SocketStatus::OK)
            {
                return true;
            }
            if (status == blib::network::SocketStatus::Error)
            {
                return false;
            }
            Sleep(static_cast<DWORD>(pollSleepMs));
        }
        return false;
    }
}

BLIB_TEST_CASE("network: InitBlibSocket is idempotent and succeeds")
{
    BLIB_TEST_CHECK(blib::network::InitBlibSocket());
    BLIB_TEST_CHECK(blib::network::InitBlibSocket());
}

BLIB_TEST_CASE("network: tcp loopback send/recv roundtrip")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    // Сервер: слушатель на loopback (неблокирующий accept)
    blib::network::TcpListener listener(blib::network::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));

    blib::network::Address listenAddress = blib::network::Address::LocalhostIPv4;
    listenAddress.setPort(testPort);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    // Клиент: подключение (неблокирующее)
    blib::network::TcpSocket client(blib::network::AddressType::IPv4);
    blib::network::Address serverAddress = blib::network::Address::LocalhostIPv4;
    serverAddress.setPort(testPort);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    // Сервер принимает
    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    // Roundtrip: клиент → сервер
    const char* clientMessage = "hello server";
    BLIB_TEST_CHECK(client.send(clientMessage, static_cast<int>(std::strlen(clientMessage))) == blib::network::SocketStatus::OK);

    char receiveBuffer[messageBufferSize] = {};
    int received = messageBufferSize;
    BLIB_TEST_CHECK(waitFor([&]() {
        received = messageBufferSize;
        return accepted.recv(receiveBuffer, received) == blib::network::SocketStatus::OK;
    }));
    BLIB_TEST_CHECK(received == static_cast<int>(std::strlen(clientMessage)));
    BLIB_TEST_CHECK(std::strcmp(receiveBuffer, clientMessage) == 0);

    // Roundtrip: сервер → клиент
    const char* serverMessage = "hello client";
    BLIB_TEST_CHECK(accepted.send(serverMessage, static_cast<int>(std::strlen(serverMessage))) == blib::network::SocketStatus::OK);

    received = messageBufferSize;
    BLIB_TEST_CHECK(waitFor([&]() {
        received = messageBufferSize;
        return client.recv(receiveBuffer, received) == blib::network::SocketStatus::OK;
    }));
    BLIB_TEST_CHECK(received == static_cast<int>(std::strlen(serverMessage)));
    BLIB_TEST_CHECK(std::strcmp(receiveBuffer, serverMessage) == 0);
}

BLIB_TEST_CASE("network: recv returns actual size (partial reads are not errors)")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    blib::network::TcpListener listener(blib::network::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::Address listenAddress = blib::network::Address::LocalhostIPv4;
    listenAddress.setPort(testPort + 1);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::AddressType::IPv4);
    blib::network::Address serverAddress = blib::network::Address::LocalhostIPv4;
    serverAddress.setPort(testPort + 1);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    // Два сообщения подряд — TCP-потоком (приёмщик видит их слитно)
    const char* first = "first";
    const char* second = "second";
    const int expectedSize = static_cast<int>(std::strlen(first) + std::strlen(second));
    BLIB_TEST_CHECK(client.send(first, static_cast<int>(std::strlen(first))) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(client.send(second, static_cast<int>(std::strlen(second))) == blib::network::SocketStatus::OK);

    // Читаем поток МАЛЫМИ кусками: частичный приём — не ошибка
    char smallBuffer[3] = {};
    int smallReceived = 3;
    BLIB_TEST_CHECK(waitFor([&]() {
        smallReceived = 3;
        return accepted.recv(smallBuffer, smallReceived) == blib::network::SocketStatus::OK;
    }));
    BLIB_TEST_CHECK(smallReceived == 3);
    BLIB_TEST_CHECK(std::strncmp(smallBuffer, "fir", 3) == 0);

    char restBuffer[messageBufferSize] = {};
    int restReceived = messageBufferSize;
    BLIB_TEST_CHECK(waitFor([&]() {
        restReceived = messageBufferSize;
        return accepted.recv(restBuffer, restReceived) == blib::network::SocketStatus::OK;
    }));
    const int restExpected = expectedSize - 3;
    BLIB_TEST_CHECK(restReceived == restExpected);
    BLIB_TEST_CHECK(std::strncmp(restBuffer, "stsecond", static_cast<size_t>(restExpected)) == 0);
}

BLIB_TEST_CASE("network: disconnect is reported to the peer")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    blib::network::TcpListener listener(blib::network::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::Address listenAddress = blib::network::Address::LocalhostIPv4;
    listenAddress.setPort(testPort + 2);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::AddressType::IPv4);
    blib::network::Address serverAddress = blib::network::Address::LocalhostIPv4;
    serverAddress.setPort(testPort + 2);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    // Клиент закрывает соединение — сервер видит Disconnected
    client.getSocket()->close();

    // Сервер видит Disconnected
    char buffer[messageBufferSize] = {};
    int received = messageBufferSize;
    const blib::network::SocketStatus status = [&]() {
        blib::network::SocketStatus s = blib::network::SocketStatus::WouldBlock;
        waitFor([&]() {
            received = messageBufferSize;
            s = accepted.recv(buffer, received);
            return s != blib::network::SocketStatus::WouldBlock;
        });
        return s;
    }();
    BLIB_TEST_CHECK(status == blib::network::SocketStatus::Disconnected);
}

BLIB_TEST_CASE("network: non-blocking accept/recv return WouldBlock when idle")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    blib::network::TcpListener listener(blib::network::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::Address listenAddress = blib::network::Address::LocalhostIPv4;
    listenAddress.setPort(testPort + 3);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    // Подключений нет — accept возвращает WouldBlock (а не Error)
    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(listener.accept(accepted) == blib::network::SocketStatus::WouldBlock);

    // Подключённый неблокирующий сокет без данных — recv WouldBlock
    blib::network::TcpSocket client(blib::network::AddressType::IPv4);
    blib::network::Address serverAddress = blib::network::Address::LocalhostIPv4;
    serverAddress.setPort(testPort + 3);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    char buffer[16] = {};
    int received = 16;
    BLIB_TEST_CHECK(accepted.recv(buffer, received) == blib::network::SocketStatus::WouldBlock);
}

BLIB_TEST_CASE("network: large message send/recv roundtrip")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    blib::network::TcpListener listener(blib::network::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::Address listenAddress = blib::network::Address::LocalhostIPv4;
    listenAddress.setPort(testPort + 4);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::AddressType::IPv4);
    blib::network::Address serverAddress = blib::network::Address::LocalhostIPv4;
    serverAddress.setPort(testPort + 4);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    // Большое сообщение: send крутит до полной отправки
    char bigSend[bigMessageSize];
    char bigRecv[bigMessageSize] = {};
    for (int i = 0; i < bigMessageSize; ++i)
    {
        bigSend[i] = static_cast<char>(i % 251);
    }

    BLIB_TEST_CHECK(client.send(bigSend, bigMessageSize) == blib::network::SocketStatus::OK);

    int totalReceived = 0;
    BLIB_TEST_CHECK(waitFor([&]() {
        int received = bigMessageSize - totalReceived;
        const blib::network::SocketStatus status = accepted.recv(bigRecv + totalReceived, received);
        if (status == blib::network::SocketStatus::OK)
        {
            totalReceived += received;
        }
        return totalReceived == bigMessageSize;
    }));

    BLIB_TEST_CHECK(std::memcmp(bigSend, bigRecv, bigMessageSize) == 0);
}

BLIB_TEST_CASE("network: setTcpNoDelay succeeds on connected loopback pair")
{
    BLIB_TEST_REQUIRE(blib::network::InitBlibSocket());

    // Nagle — причина латентности мелких real-time пакетов; опция
    // должна выставляться на обоих концах соединения без ошибки
    blib::network::TcpListener listener(blib::network::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::Address listenAddress = blib::network::Address::LocalhostIPv4;
    listenAddress.setPort(testPort + 5);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::AddressType::IPv4);
    blib::network::Address serverAddress = blib::network::Address::LocalhostIPv4;
    serverAddress.setPort(testPort + 5);
    BLIB_TEST_CHECK(connectNonBlocking(client, serverAddress));

    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(acceptNonBlocking(listener, accepted));
    BLIB_TEST_CHECK(accepted.setBlocking(false));

    BLIB_TEST_CHECK(client.setTcpNoDelay(true));
    BLIB_TEST_CHECK(accepted.setTcpNoDelay(true));

    // Roundtrip с включённым NODELAY — передача по-прежнему корректна
    const char* message = "nodelay";
    BLIB_TEST_CHECK(client.send(message, static_cast<int>(std::strlen(message))) == blib::network::SocketStatus::OK);
    char buffer[messageBufferSize] = {};
    int received = messageBufferSize;
    BLIB_TEST_CHECK(waitFor([&]() {
        received = messageBufferSize;
        return accepted.recv(buffer, received) == blib::network::SocketStatus::OK;
    }));
    BLIB_TEST_CHECK(received == static_cast<int>(std::strlen(message)));
    BLIB_TEST_CHECK(std::strcmp(buffer, message) == 0);
}

BLIB_TEST_CASE("network: address ipv4 parse and port (network byte order)")
{
    // Разбор адреса
    bool ok = false;
    const blib::network::Address address = blib::network::Address::fromIPv4("127.0.0.1", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(address.getType() == blib::network::AddressType::IPv4);

    // Неверная строка — отказ
    const blib::network::Address invalid = blib::network::Address::fromIPv4("not-an-ip", &ok);
    BLIB_TEST_CHECK(!ok);

    // Порт — сетевой порядок байт (htons)
    blib::network::Address ported = blib::network::Address::LocalhostIPv4;
    ported.setPort(testPort);
    const auto* sockAddress = reinterpret_cast<const sockaddr_in*>(ported.__getHandler());
    BLIB_TEST_CHECK(sockAddress->sin_port == htons(static_cast<u_short>(testPort)));

    // Копия глубокая: изменения оригинала не трогают копию
    blib::network::Address copy = ported;
    ported.setPort(testPort + 10);
    const auto* copySockAddress = reinterpret_cast<const sockaddr_in*>(copy.__getHandler());
    BLIB_TEST_CHECK(copySockAddress->sin_port == htons(static_cast<u_short>(testPort)));
}
