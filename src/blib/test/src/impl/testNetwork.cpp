// WinSock2.h обязан быть ПЕРВЫМ системным инклюдом (до windows.h из
// прочих заголовков): иначе winsock1 конфликтует с winsock2
#include <WinSock2.h>

#include <blib/test/src/test.h>

#include <blib/network/address.h>
#include <blib/network/router.h>
#include <blib/network/socket.h>
#include <blib/network/tcpListener.h>
#include <blib/network/tcpSocket.h>
#include <blib/network/impl/win/winNetworkUtil.h>

#include <cstring>

// Тесты сети — loopback в одном процессе (детерминированные: не
// требуют внешнего сервера). Все сокеты — неблокирующие: ожидание
// состояний — опрос с таймаутом (loopback надёжен).

namespace
{
    using namespace blib::network::address;

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
    bool connectNonBlocking(blib::network::TcpSocket& client, blib::network::address::Tcp& endpoint)
    {
        if (!client.setBlocking(false))
        {
            return false;
        }

        for (int attempt = 0; attempt < maxPollAttempts; ++attempt)
        {
            const blib::network::SocketStatus status = client.connect(endpoint);
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

    // Loopback TCP-эндпоинт тестового порта (+ смещение для разнесения
    // параллельных тестов по портам)
    blib::network::address::Tcp makeLoopbackTcp(int portOffset = 0)
    {
        blib::network::address::Tcp endpoint;
        endpoint.ip = blib::network::address::Address::LocalhostIPv4;
        endpoint.port = static_cast<buint16>(testPort + portOffset);
        return endpoint;
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
    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));

    blib::network::address::Tcp listenAddress = makeLoopbackTcp();
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    // Клиент: подключение (неблокирующее)
    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp();
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

    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(1);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(1);
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

    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(2);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(2);
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

    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(3);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    // Подключений нет — accept возвращает WouldBlock (а не Error)
    blib::network::TcpSocket accepted;
    BLIB_TEST_CHECK(listener.accept(accepted) == blib::network::SocketStatus::WouldBlock);

    // Подключённый неблокирующий сокет без данных — recv WouldBlock
    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(3);
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

    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(4);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(4);
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
    blib::network::TcpListener listener(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(listener.setBlocking(false));
    blib::network::address::Tcp listenAddress = makeLoopbackTcp(5);
    BLIB_TEST_CHECK(listener.bind(listenAddress) == blib::network::SocketStatus::OK);
    BLIB_TEST_CHECK(listener.listen() == blib::network::SocketStatus::OK);

    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    blib::network::address::Tcp serverAddress = makeLoopbackTcp(5);
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

// =====================================================================
// Адресные типы: разбор/форматирование, вариант Address, подсети,
// эндпоинты Tcp/Udp
// =====================================================================

BLIB_TEST_CASE("network: Mac toString/fromString roundtrip")
{
    char buffer[maxAddressStringLength];

    // Нижний регистр, ':' — каноническая форма
    Mac mac;
    BLIB_TEST_CHECK(mac.fromString("aa:bb:cc:dd:ee:ff"));
    BLIB_TEST_CHECK(mac.getType() == AddressType::Mac);
    BLIB_TEST_CHECK(mac.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "aa:bb:cc:dd:ee:ff") == 0);
    BLIB_TEST_CHECK(mac.bytes[0] == 0xAA && mac.bytes[5] == 0xFF);

    // Верхний регистр и '-' — допустимы на входе, канонизируются
    BLIB_TEST_CHECK(mac.fromString("AA-BB-CC-DD-EE-FF"));
    BLIB_TEST_CHECK(mac.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "aa:bb:cc:dd:ee:ff") == 0);

    // Ошибки: смешанные разделители, 5 групп, 3 цифры в группе,
    // не-hex символ
    BLIB_TEST_CHECK(!mac.fromString("aa:bb-cc:dd:ee:ff"));
    BLIB_TEST_CHECK(!mac.fromString("aa:bb:cc:dd:ee"));
    BLIB_TEST_CHECK(!mac.fromString("aaa:bb:cc:dd:ee:ff"));
    BLIB_TEST_CHECK(!mac.fromString("gg:bb:cc:dd:ee:ff"));
    // После неудачи — объект сброшен к нулям
    Mac zero;
    BLIB_TEST_CHECK(mac == zero);

    // Слишком маленький буфер — отказ без записи
    char tinyBuffer[macStringLength] = {};
    BLIB_TEST_CHECK(!mac.toString(tinyBuffer, macStringLength));

    // Равенство
    Mac a;
    Mac b;
    BLIB_TEST_CHECK(a.fromString("00:11:22:33:44:55"));
    BLIB_TEST_CHECK(b.fromString("00:11:22:33:44:55"));
    BLIB_TEST_CHECK(a == b);
    b.bytes[5] = 0x56;
    BLIB_TEST_CHECK(a != b);
}

BLIB_TEST_CASE("network: IPv4 toString/fromString roundtrip")
{
    char buffer[maxAddressStringLength];

    IPv4 ipv4;
    BLIB_TEST_CHECK(ipv4.fromString("192.168.1.77"));
    BLIB_TEST_CHECK(ipv4.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(ipv4.bytes[0] == 192 && ipv4.bytes[3] == 77);
    BLIB_TEST_CHECK(ipv4.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "192.168.1.77") == 0);

    // Граничные значения
    BLIB_TEST_CHECK(ipv4.fromString("0.0.0.0"));
    BLIB_TEST_CHECK(ipv4.fromString("255.255.255.255"));

    // Ошибки: октет > 255, неполный, лишний, пустой октет, не-цифра
    BLIB_TEST_CHECK(!ipv4.fromString("256.1.1.1"));
    BLIB_TEST_CHECK(!ipv4.fromString("1.2.3"));
    BLIB_TEST_CHECK(!ipv4.fromString("1.2.3.4.5"));
    BLIB_TEST_CHECK(!ipv4.fromString("1..2.3"));
    BLIB_TEST_CHECK(!ipv4.fromString("1.2.3.x"));

    // Агрегатная инициализация байтов
    const IPv4 direct{ 10, 0, 0, 1 };
    BLIB_TEST_CHECK(direct.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "10.0.0.1") == 0);
}

BLIB_TEST_CASE("network: IPv6 toString/fromString roundtrip")
{
    char buffer[maxAddressStringLength];

    IPv6 ipv6;
    // Сжатая форма
    BLIB_TEST_CHECK(ipv6.fromString("2001:db8::1"));
    BLIB_TEST_CHECK(ipv6.getType() == AddressType::IPv6);
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "2001:db8::1") == 0);

    // Полная форма без сжатия
    BLIB_TEST_CHECK(ipv6.fromString("1:2:3:4:5:6:7:8"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "1:2:3:4:5:6:7:8") == 0);

    // Все нули → "::", loopback → "::1"
    BLIB_TEST_CHECK(ipv6.fromString("::"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::") == 0);
    BLIB_TEST_CHECK(ipv6.fromString("::1"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::1") == 0);

    // Верхний регистр канонизируется в нижний
    BLIB_TEST_CHECK(ipv6.fromString("FE80::1"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "fe80::1") == 0);

    // Сжимается САМЫЙ ДЛИННЫЙ прогон нулей; при равных — первый (RFC 5952)
    BLIB_TEST_CHECK(ipv6.fromString("1:0:0:0:2:0:0:3"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "1::2:0:0:3") == 0);

    // Ошибки: 9 групп, два "::", не-hex, >4 цифр в группе
    BLIB_TEST_CHECK(!ipv6.fromString("1:2:3:4:5:6:7:8:9"));
    BLIB_TEST_CHECK(!ipv6.fromString("1::2::3"));
    BLIB_TEST_CHECK(!ipv6.fromString("gggg::1"));
    BLIB_TEST_CHECK(!ipv6.fromString("1:2:3:4:5:6:7:8:"));
    BLIB_TEST_CHECK(!ipv6.fromString(":1:2:3:4:5:6:7"));

    // Embedded-IPv4 хвост (RFC 4291): "::ffff:192.168.1.1"
    // (IPv4-mapped — форму, которую winsock отдаёт IPv4-клиенту
    // на IPv6-сокете)
    BLIB_TEST_CHECK(ipv6.fromString("::ffff:192.168.1.1"));
    BLIB_TEST_CHECK(ipv6.bytes[10] == 0xFF && ipv6.bytes[11] == 0xFF);
    BLIB_TEST_CHECK(ipv6.bytes[12] == 192 && ipv6.bytes[13] == 168);
    BLIB_TEST_CHECK(ipv6.bytes[14] == 1 && ipv6.bytes[15] == 1);
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::ffff:192.168.1.1") == 0);

    // Та же форма в hex (::ffff:c0a8:101) — канонизируется в dotted
    BLIB_TEST_CHECK(ipv6.fromString("::ffff:c0a8:101"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::ffff:192.168.1.1") == 0);

    // Полная (несжатая) embedded-форма: 6 групп + хвост
    const IPv6 mappedFull = []() {
        IPv6 parsed;
        parsed.fromString("0:0:0:0:0:ffff:192.168.1.1");
        return parsed;
    }();
    BLIB_TEST_CHECK(ipv6.fromString("::ffff:192.168.1.1"));
    BLIB_TEST_CHECK(mappedFull == ipv6);
    BLIB_TEST_CHECK(mappedFull.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::ffff:192.168.1.1") == 0);

    // IPv4-compatible (deprecated, RFC 4291): парсится, но dotted-хвост
    // по RFC 5952 — только у mapped; печатается обычным hex
    BLIB_TEST_CHECK(ipv6.fromString("::192.168.1.1"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::c0a8:101") == 0);

    // Mapped с нулевым хвостом — печатается dotted (не "::")
    BLIB_TEST_CHECK(ipv6.fromString("::ffff:0.0.0.0"));
    BLIB_TEST_CHECK(ipv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::ffff:0.0.0.0") == 0);

    // Ошибки embedded-хвоста: >6 hex-групп, хвост не в конце,
    // неполный IPv4, октет > 255, лишний октет, голый IPv4
    BLIB_TEST_CHECK(!ipv6.fromString("1:2:3:4:5:6:7:192.168.1.1"));
    BLIB_TEST_CHECK(!ipv6.fromString("::ffff:1.2.3.4:5678"));
    BLIB_TEST_CHECK(!ipv6.fromString("::ffff:1.2.3"));
    BLIB_TEST_CHECK(!ipv6.fromString("::ffff:256.1.1.1"));
    BLIB_TEST_CHECK(!ipv6.fromString("::ffff:1.2.3.4.5"));
    BLIB_TEST_CHECK(!ipv6.fromString("192.168.1.1"));

    // Равенство
    IPv6 a;
    IPv6 b;
    BLIB_TEST_CHECK(a.fromString("2001:db8::1"));
    BLIB_TEST_CHECK(b.fromString("2001:0db8:0:0:0:0:0:0001"));
    BLIB_TEST_CHECK(a == b);
    b.bytes[15] = 2;
    BLIB_TEST_CHECK(a != b);
}

BLIB_TEST_CASE("network: Address variant dispatch and conversions")
{
    char buffer[maxAddressStringLength];

    // Диспетчеризация fromString
    const IPv4 expectedIpv4{ 1, 2, 3, 4 };
    bool ok = false;
    Address addr = Address::fromString("1.2.3.4", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(addr.toIPv4() == expectedIpv4);

    addr = Address::fromString("aa:bb:cc:dd:ee:ff", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::Mac);
    BLIB_TEST_CHECK(addr.toMac().bytes[0] == 0xAA);

    addr = Address::fromString("::1", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::IPv6);
    BLIB_TEST_CHECK(addr.toIPv6().bytes[15] == 1);

    const IPv4 subnetNetwork{ 192, 168, 0, 0 };
    addr = Address::fromString("192.168.0.0/16", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::IPv4Subnet);
    BLIB_TEST_CHECK(addr.toIPv4Subnet().prefix == 16);
    BLIB_TEST_CHECK(addr.toIPv4Subnet().network == subnetNetwork);

    addr = Address::fromString("2001:db8::/32", &ok);
    BLIB_TEST_CHECK(ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::IPv6Subnet);
    BLIB_TEST_CHECK(addr.toIPv6Subnet().prefix == 32);

    // Неразборная строка — UNDEFINED + ok = false
    addr = Address::fromString("not-an-address", &ok);
    BLIB_TEST_CHECK(!ok);
    BLIB_TEST_CHECK(addr.getType() == AddressType::UNDEFINED);
    BLIB_TEST_CHECK(!addr.toString(buffer, maxAddressStringLength));

    // Пустая строка и nullptr
    BLIB_TEST_CHECK(Address::fromString("").getType() == AddressType::UNDEFINED);
    BLIB_TEST_CHECK(Address::fromString(nullptr).getType() == AddressType::UNDEFINED);

    // toString варианта — формат активного члена
    BLIB_TEST_CHECK(Address::fromString("10.20.30.40").toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "10.20.30.40") == 0);

    // Равенство вариантов (теги различают типы)
    BLIB_TEST_CHECK(Address::fromString("1.2.3.4") == Address(expectedIpv4));
    BLIB_TEST_CHECK(Address::fromString("1.2.3.4") != Address::fromString("::1"));

    // Копирование value-семантики
    Address copy = Address::fromString("192.168.0.0/16");
    BLIB_TEST_CHECK(copy.toIPv4Subnet().prefix == 16);
    BLIB_TEST_CHECK(copy == Address::fromString("192.168.0.0/16"));
}

BLIB_TEST_CASE("network: address statics and endpoint conversions")
{
    char buffer[maxAddressStringLength];

    // Статические адреса-шаблоны
    BLIB_TEST_CHECK(Address::LocalhostIPv4.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(Address::LocalhostIPv4.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "127.0.0.1") == 0);
    BLIB_TEST_CHECK(Address::AnyIPv4.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "0.0.0.0") == 0);
    BLIB_TEST_CHECK(Address::LocalhostIPv6.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "::1") == 0);

    // Граница winsock: порт — сетевой порядок байт (htons), байты
    // адреса — как есть (сетевой порядок хранения)
    sockaddr_in sockAddress;
    BLIB_TEST_CHECK(blibToSockaddr(Address::LocalhostIPv4,
        static_cast<buint16>(testPort), sockAddress));
    BLIB_TEST_CHECK(sockAddress.sin_family == AF_INET);
    BLIB_TEST_CHECK(sockAddress.sin_port == htons(static_cast<u_short>(testPort)));

    // Обратная конвертация возвращает исходные адрес и порт (ntohs)
    Address backIp;
    buint16 backPort = 0;
    BLIB_TEST_CHECK(blibFromSockaddr(sockAddress, backIp, backPort));
    BLIB_TEST_CHECK(backIp.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(backIp == Address::LocalhostIPv4);
    BLIB_TEST_CHECK(backPort == static_cast<buint16>(testPort));

    // Не-IPv4 (Mac) в sockaddr не конвертируется
    BLIB_TEST_CHECK(!blibToSockaddr(Address::fromString("aa:bb:cc:dd:ee:ff"),
        static_cast<buint16>(testPort), sockAddress));
}

BLIB_TEST_CASE("network: IPv4Subnet isInSubnet")
{
    IPv4Subnet subnet;
    BLIB_TEST_CHECK(subnet.fromString("192.168.1.0/24"));
    BLIB_TEST_CHECK(subnet.getType() == AddressType::IPv4Subnet);

    // Внутри, на границах сети
    const IPv4 insideLow{ 192, 168, 1, 1 };
    const IPv4 insideHigh{ 192, 168, 1, 255 };
    const IPv4 outsideHigh{ 192, 168, 2, 1 };
    const IPv4 outsideLow{ 192, 167, 255, 255 };
    BLIB_TEST_CHECK(subnet.isInSubnet(insideLow));
    BLIB_TEST_CHECK(subnet.isInSubnet(insideHigh));
    BLIB_TEST_CHECK(!subnet.isInSubnet(outsideHigh));
    BLIB_TEST_CHECK(!subnet.isInSubnet(outsideLow));

    // /0 — совпадает всё; /32 — точное совпадение
    const IPv4 anyIp{ 255, 255, 255, 255 };
    const IPv4 exactIp{ 10, 0, 0, 5 };
    const IPv4 neighborIp{ 10, 0, 0, 6 };
    BLIB_TEST_CHECK(subnet.fromString("0.0.0.0/0"));
    BLIB_TEST_CHECK(subnet.isInSubnet(anyIp));
    BLIB_TEST_CHECK(subnet.fromString("10.0.0.5/32"));
    BLIB_TEST_CHECK(subnet.isInSubnet(exactIp));
    BLIB_TEST_CHECK(!subnet.isInSubnet(neighborIp));

    // Префикс больше 32 — отказ разбора
    BLIB_TEST_CHECK(!subnet.fromString("10.0.0.0/33"));
    // Нет префикса — отказ
    BLIB_TEST_CHECK(!subnet.fromString("10.0.0.0"));

    // Roundtrip строки
    char buffer[maxAddressStringLength];
    BLIB_TEST_CHECK(subnet.fromString("172.16.0.0/12"));
    BLIB_TEST_CHECK(subnet.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "172.16.0.0/12") == 0);
}

BLIB_TEST_CASE("network: IPv6Subnet isInSubnet")
{
    IPv6Subnet subnet;
    BLIB_TEST_CHECK(subnet.fromString("2001:db8::/32"));
    BLIB_TEST_CHECK(subnet.getType() == AddressType::IPv6Subnet);

    IPv6 inside;
    IPv6 outside;
    BLIB_TEST_CHECK(inside.fromString("2001:db8::1"));
    BLIB_TEST_CHECK(outside.fromString("2001:db9::1"));
    BLIB_TEST_CHECK(subnet.isInSubnet(inside));
    BLIB_TEST_CHECK(!subnet.isInSubnet(outside));

    // Границы префикса: /128 — точное совпадение; /0 — всё
    const IPv6 loopbackV6{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    BLIB_TEST_CHECK(subnet.fromString("::1/128"));
    BLIB_TEST_CHECK(subnet.isInSubnet(loopbackV6));
    BLIB_TEST_CHECK(!subnet.isInSubnet(IPv6{}));
    BLIB_TEST_CHECK(subnet.fromString("::/0"));
    BLIB_TEST_CHECK(subnet.isInSubnet(outside));

    // Префикс больше 128 — отказ разбора
    BLIB_TEST_CHECK(!subnet.fromString("2001:db8::/129"));

    // Roundtrip строки
    char buffer[maxAddressStringLength];
    BLIB_TEST_CHECK(subnet.fromString("fe80::/10"));
    BLIB_TEST_CHECK(subnet.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "fe80::/10") == 0);
}

BLIB_TEST_CASE("network: Tcp endpoint parse/format")
{
    char buffer[maxAddressStringLength];

    // IPv4-форма
    const IPv4 loopbackV4{ 127, 0, 0, 1 };
    Tcp endpoint;
    BLIB_TEST_CHECK(endpoint.fromString("127.0.0.1:8080"));
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(endpoint.getPort() == 8080);
    BLIB_TEST_CHECK(endpoint.getIP().toIPv4() == loopbackV4);
    BLIB_TEST_CHECK(endpoint.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "127.0.0.1:8080") == 0);

    // IPv6-форма (скобки)
    BLIB_TEST_CHECK(endpoint.fromString("[2001:db8::1]:443"));
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::IPv6);
    BLIB_TEST_CHECK(endpoint.getPort() == 443);
    BLIB_TEST_CHECK(endpoint.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "[2001:db8::1]:443") == 0);

    // IPv6-форма с mapped-адресом (хвост печатается dotted)
    BLIB_TEST_CHECK(endpoint.fromString("[::ffff:192.168.1.1]:443"));
    BLIB_TEST_CHECK(endpoint.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "[::ffff:192.168.1.1]:443") == 0);

    // Границы порта
    BLIB_TEST_CHECK(endpoint.fromString("1.2.3.4:0"));
    BLIB_TEST_CHECK(endpoint.getPort() == 0);
    BLIB_TEST_CHECK(endpoint.fromString("1.2.3.4:65535"));
    BLIB_TEST_CHECK(endpoint.getPort() == 65535);

    // Ошибки: порт > 65535, нет порта, голый IPv6 (неоднозначен),
    // пустой адрес, невалидный адрес
    BLIB_TEST_CHECK(!endpoint.fromString("1.2.3.4:65536"));
    BLIB_TEST_CHECK(!endpoint.fromString("127.0.0.1"));
    BLIB_TEST_CHECK(!endpoint.fromString("::1:8080"));
    BLIB_TEST_CHECK(!endpoint.fromString(":8080"));
    BLIB_TEST_CHECK(!endpoint.fromString("abc:8080"));
    // После неудачи — сброс к UNDEFINED/0
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::UNDEFINED);
    BLIB_TEST_CHECK(endpoint.getPort() == 0);

    // setPort/getPort + value-копирование
    Tcp base;
    BLIB_TEST_CHECK(base.fromString("127.0.0.1:8080"));
    Tcp copy = base;
    copy.setPort(9090);
    BLIB_TEST_CHECK(base.getPort() == 8080);
    BLIB_TEST_CHECK(copy.getPort() == 9090);
    BLIB_TEST_CHECK(base != copy);
}

BLIB_TEST_CASE("network: Udp endpoint parse/format")
{
    char buffer[maxAddressStringLength];

    Udp endpoint;
    BLIB_TEST_CHECK(endpoint.fromString("10.0.0.1:5353"));
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(endpoint.getPort() == 5353);
    BLIB_TEST_CHECK(endpoint.toString(buffer, maxAddressStringLength));
    BLIB_TEST_CHECK(std::strcmp(buffer, "10.0.0.1:5353") == 0);

    // Ошибка — сброс
    BLIB_TEST_CHECK(!endpoint.fromString("10.0.0.1:99999"));
    BLIB_TEST_CHECK(endpoint.getType() == AddressType::UNDEFINED);
}

// =====================================================================
// Router: загрузка таблиц маршрутизации (linux/windows) и эмуляция
// выбора маршрута (longest-prefix-match)
// =====================================================================

BLIB_TEST_CASE("network: Router linux table - default, on-link, src")
{
    using blib::network::Router;
    using blib::network::RouteResult;
    using blib::network::RouteTableFormat;
    using blib::network::RouterError;

    // Вывод "ip route show": default + напрямую подключённая сеть
    // (on-link, src — локальный адрес интерфейса) + статический маршрут
    const char* linuxTable =
        "default via 192.168.1.1 dev eth0 proto dhcp metric 100\n"
        "192.168.1.0/24 dev eth0 proto kernel scope link src 192.168.1.5 metric 100\n"
        "10.0.0.0/8 via 10.0.0.1 dev eth1 proto static metric 50\n";

    Router router;
    BLIB_TEST_CHECK(router.loadFromString(linuxTable, RouteTableFormat::Linux) == RouterError::None);
    BLIB_TEST_CHECK(router.getRouteCount() == 3);

    // Внешний адрес → default: шлюз 192.168.1.1 через eth0
    const IPv4 external{ 8, 8, 8, 8 };
    const IPv4 defaultGateway{ 192, 168, 1, 1 };
    RouteResult result;
    BLIB_TEST_CHECK(router.route(external, result));
    BLIB_TEST_CHECK(result.nextHop.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == defaultGateway);
    BLIB_TEST_CHECK(result.metric == 100);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "eth0") == 0);
    BLIB_TEST_CHECK(result.iface.localAddress.getType() == AddressType::UNDEFINED);

    // Локальная сеть → on-link: nextHop — сам адресат, интерфейс eth0
    // с локальным адресом из src
    const IPv4 lanHost{ 192, 168, 1, 50 };
    const IPv4 lanSrc{ 192, 168, 1, 5 };
    BLIB_TEST_CHECK(router.route(lanHost, result));
    BLIB_TEST_CHECK(result.nextHop.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == lanHost);
    BLIB_TEST_CHECK(result.metric == 100);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "eth0") == 0);
    BLIB_TEST_CHECK(result.iface.localAddress.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(result.iface.localAddress.toIPv4() == lanSrc);

    // 10/8 → шлюз 10.0.0.1 через eth1
    const IPv4 tenNetHost{ 10, 1, 2, 3 };
    const IPv4 tenGateway{ 10, 0, 0, 1 };
    BLIB_TEST_CHECK(router.route(tenNetHost, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == tenGateway);
    BLIB_TEST_CHECK(result.metric == 50);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "eth1") == 0);

    // Диспетчер по Address: IPv4 — работает, Mac/подсети — нет
    const Address asAddress = Address(external);
    BLIB_TEST_CHECK(router.route(asAddress, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == defaultGateway);
    BLIB_TEST_CHECK(!router.route(Address::fromString("aa:bb:cc:dd:ee:ff"), result));
    BLIB_TEST_CHECK(!router.route(Address::fromString("192.168.1.0/24"), result));
    BLIB_TEST_CHECK(!router.route(Address(), result));

    // IPv6-таблица пуста — IPv6-адресат не маршрутизируется
    const IPv6 ipv6Host{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    BLIB_TEST_CHECK(!router.route(ipv6Host, result));
}

BLIB_TEST_CASE("network: Router longest-prefix and metric tie-break")
{
    using blib::network::Router;
    using blib::network::RouteResult;
    using blib::network::RouteTableFormat;
    using blib::network::RouterError;

    // Вложенные подсети: выбор — самый длинный префикс
    const char* prefixTable =
        "10.0.0.0/8 via 10.0.0.1 dev eth0 metric 100\n"
        "10.1.0.0/16 via 10.1.0.1 dev eth1 metric 200\n"
        "10.1.2.0/24 via 10.1.2.1 dev eth2 metric 200\n";

    Router router;
    BLIB_TEST_CHECK(router.loadFromString(prefixTable, RouteTableFormat::Linux) == RouterError::None);
    BLIB_TEST_CHECK(router.getRouteCount() == 3);

    const IPv4 in24{ 10, 1, 2, 5 };
    const IPv4 in16{ 10, 1, 9, 9 };
    const IPv4 in8{ 10, 9, 9, 9 };
    const IPv4 gw24{ 10, 1, 2, 1 };
    const IPv4 gw16{ 10, 1, 0, 1 };
    const IPv4 gw8{ 10, 0, 0, 1 };
    RouteResult result;
    BLIB_TEST_CHECK(router.route(in24, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == gw24);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "eth2") == 0);
    BLIB_TEST_CHECK(router.route(in16, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == gw16);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "eth1") == 0);
    BLIB_TEST_CHECK(router.route(in8, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == gw8);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "eth0") == 0);

    // Равный префикс: меньшая метрика; при равной метрике — первый
    const char* tieBreakTable =
        "10.0.0.0/8 via 10.0.0.1 dev ethA metric 50\n"
        "10.0.0.0/8 via 10.0.0.2 dev ethB metric 10\n"
        "10.9.0.0/16 via 10.9.0.1 dev ethC metric 10\n"
        "10.9.0.0/16 via 10.9.0.2 dev ethD metric 10\n";
    BLIB_TEST_CHECK(router.loadFromString(tieBreakTable, RouteTableFormat::Linux) == RouterError::None);
    BLIB_TEST_CHECK(router.getRouteCount() == 4);

    const IPv4 tieHost{ 10, 5, 5, 5 };
    const IPv4 tieGwBetter{ 10, 0, 0, 2 };
    BLIB_TEST_CHECK(router.route(tieHost, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == tieGwBetter);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "ethB") == 0);
    BLIB_TEST_CHECK(result.metric == 10);

    const IPv4 tieEqualHost{ 10, 9, 9, 9 };
    const IPv4 tieGwFirst{ 10, 9, 0, 1 };
    BLIB_TEST_CHECK(router.route(tieEqualHost, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == tieGwFirst);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "ethC") == 0);
}

BLIB_TEST_CASE("network: Router linux IPv6 default and on-link")
{
    using blib::network::Router;
    using blib::network::RouteResult;
    using blib::network::RouteTableFormat;
    using blib::network::RouterError;

    // IPv6: default через link-local шлюз + напрямую подключённая сеть
    const char* linuxV6Table =
        "default via fe80::1 dev eth0 proto ra metric 1024\n"
        "2001:db8::/64 dev eth0 proto kernel metric 256\n";

    Router router;
    BLIB_TEST_CHECK(router.loadFromString(linuxV6Table, RouteTableFormat::Linux) == RouterError::None);
    BLIB_TEST_CHECK(router.getRouteCount() == 2);

    // 2001:db8::/64 — on-link: nextHop — сам адресат
    IPv6 lanHost;
    BLIB_TEST_CHECK(lanHost.fromString("2001:db8::5"));
    RouteResult result;
    BLIB_TEST_CHECK(router.route(lanHost, result));
    BLIB_TEST_CHECK(result.nextHop.getType() == AddressType::IPv6);
    BLIB_TEST_CHECK(result.nextHop.toIPv6() == lanHost);
    BLIB_TEST_CHECK(result.metric == 256);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "eth0") == 0);

    // Внешний адрес → default через fe80::1
    IPv6 external;
    BLIB_TEST_CHECK(external.fromString("2606:4700::1111"));
    IPv6 defaultGateway;
    BLIB_TEST_CHECK(defaultGateway.fromString("fe80::1"));
    BLIB_TEST_CHECK(router.route(external, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv6() == defaultGateway);
    BLIB_TEST_CHECK(result.metric == 1024);

    // Диспетчер по Address — IPv6 тоже работает
    BLIB_TEST_CHECK(router.route(Address(external), result));
    BLIB_TEST_CHECK(result.nextHop.toIPv6() == defaultGateway);
}

BLIB_TEST_CASE("network: Router windows route print")
{
    using blib::network::Router;
    using blib::network::RouteResult;
    using blib::network::RouteTableFormat;
    using blib::network::RouterError;

    // Полный вывод "route print" (английская локаль): Interface List,
    // секции IPv4 (destination/netmask/gateway/interface/metric) и IPv6
    // (if/metric/destination/gateway)
    const char* windowsTable =
        "===========================================================================\n"
        "Interface List\n"
        "  1...XX XX XX XX XX XX ......Software Loopback Interface 1\n"
        "  5...00 1b 21 5c 4e 7f ......Realtek PCIe GbE Family Controller\n"
        "===========================================================================\n"
        "\n"
        "IPv4 Route Table\n"
        "===========================================================================\n"
        "Active Routes:\n"
        "Network Destination        Netmask          Gateway       Interface  Metric\n"
        "          0.0.0.0          0.0.0.0      192.168.1.1     192.168.1.5     25\n"
        "      192.168.1.0    255.255.255.0         On-link      192.168.1.5    281\n"
        "===========================================================================\n"
        "Persistent Routes:\n"
        "  None\n"
        "\n"
        "IPv6 Route Table\n"
        "===========================================================================\n"
        "Active Routes:\n"
        " If Metric Network Destination      Gateway\n"
        "  1    331 ::1/128                  On-link\n"
        "  5    281 2001:db8::/64            On-link\n"
        "  5    281 ::/0                     fe80::1\n"
        "===========================================================================\n"
        "Persistent Routes:\n"
        "  None\n";

    Router router;
    BLIB_TEST_CHECK(router.loadFromString(windowsTable, RouteTableFormat::Windows) == RouterError::None);
    BLIB_TEST_CHECK(router.getRouteCount() == 5);

    // IPv4 default: шлюз 192.168.1.1, локальный адрес интерфейса —
    // колонка Interface (имени в IPv4-таблице нет)
    const IPv4 external{ 8, 8, 8, 8 };
    const IPv4 defaultGateway{ 192, 168, 1, 1 };
    const IPv4 ifaceAddress{ 192, 168, 1, 5 };
    RouteResult result;
    BLIB_TEST_CHECK(router.route(external, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == defaultGateway);
    BLIB_TEST_CHECK(result.metric == 25);
    BLIB_TEST_CHECK(result.iface.localAddress.getType() == AddressType::IPv4);
    BLIB_TEST_CHECK(result.iface.localAddress.toIPv4() == ifaceAddress);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "") == 0);

    // 192.168.1.0/24 — On-link
    const IPv4 lanHost{ 192, 168, 1, 77 };
    BLIB_TEST_CHECK(router.route(lanHost, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv4() == lanHost);
    BLIB_TEST_CHECK(result.metric == 281);

    // IPv6 ::/0 → fe80::1 через интерфейс 5 (имя из Interface List)
    IPv6 v6External;
    BLIB_TEST_CHECK(v6External.fromString("2606:4700::1"));
    IPv6 v6Gateway;
    BLIB_TEST_CHECK(v6Gateway.fromString("fe80::1"));
    BLIB_TEST_CHECK(router.route(v6External, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv6() == v6Gateway);
    BLIB_TEST_CHECK(result.metric == 281);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "Realtek PCIe GbE Family Controller") == 0);
    BLIB_TEST_CHECK(result.iface.localAddress.getType() == AddressType::UNDEFINED);

    // 2001:db8::/64 — On-link (тот же интерфейс)
    IPv6 v6LanHost;
    BLIB_TEST_CHECK(v6LanHost.fromString("2001:db8::9"));
    BLIB_TEST_CHECK(router.route(v6LanHost, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv6() == v6LanHost);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "Realtek PCIe GbE Family Controller") == 0);

    // ::1/128 — On-link через loopback (интерфейс 1)
    IPv6 v6Loopback;
    BLIB_TEST_CHECK(v6Loopback.fromString("::1"));
    BLIB_TEST_CHECK(router.route(v6Loopback, result));
    BLIB_TEST_CHECK(result.nextHop.toIPv6() == v6Loopback);
    BLIB_TEST_CHECK(result.metric == 331);
    BLIB_TEST_CHECK(std::strcmp(result.iface.name, "Software Loopback Interface 1") == 0);
}

BLIB_TEST_CASE("network: Router errors, replacement and clear")
{
    using blib::network::Router;
    using blib::network::RouteResult;
    using blib::network::RouteTableFormat;
    using blib::network::RouterError;

    Router router;
    BLIB_TEST_CHECK(router.getRouteCount() == 0);
    const IPv4 external{ 8, 8, 8, 8 };
    RouteResult result;
    BLIB_TEST_CHECK(!router.route(external, result));

    // Пустой ввод
    BLIB_TEST_CHECK(router.loadFromString("", RouteTableFormat::Linux) == RouterError::EmptyTable);
    BLIB_TEST_CHECK(router.loadFromString(nullptr, RouteTableFormat::Linux) == RouterError::EmptyTable);

    // Нераспознанные строки пропускаются — маршрутов ноль
    BLIB_TEST_CHECK(router.loadFromString("junk line here", RouteTableFormat::Linux) == RouterError::EmptyTable);
    BLIB_TEST_CHECK(router.getRouteCount() == 0);

    // JSON — пока не реализован (TODO)
    BLIB_TEST_CHECK(router.loadFromString("{}", RouteTableFormat::Json) == RouterError::NotSupported);

    // Несуществующий файл
    BLIB_TEST_CHECK(router.loadFromFile("__router_missing_file__.txt",
        RouteTableFormat::Linux) == RouterError::FileNotFound);

    // Успешная загрузка, затем неудачная — прежняя таблица не тронута
    const char* linuxTable =
        "default via 192.168.1.1 dev eth0 metric 100\n"
        "10.0.0.0/8 via 10.0.0.1 dev eth1 metric 50\n";
    BLIB_TEST_CHECK(router.loadFromString(linuxTable, RouteTableFormat::Linux) == RouterError::None);
    BLIB_TEST_CHECK(router.getRouteCount() == 2);
    BLIB_TEST_CHECK(router.loadFromString("", RouteTableFormat::Linux) == RouterError::EmptyTable);
    BLIB_TEST_CHECK(router.getRouteCount() == 2);
    BLIB_TEST_CHECK(router.route(external, result));

    // Повторная успешная загрузка заменяет таблицу
    const char* v6OnlyTable = "default via fe80::1 dev eth0 metric 1024\n";
    BLIB_TEST_CHECK(router.loadFromString(v6OnlyTable, RouteTableFormat::Linux) == RouterError::None);
    BLIB_TEST_CHECK(router.getRouteCount() == 1);
    BLIB_TEST_CHECK(!router.route(external, result)); // IPv4-таблица теперь пуста

    // clear сбрасывает обе таблицы
    router.clear();
    BLIB_TEST_CHECK(router.getRouteCount() == 0);
    IPv6 v6External;
    BLIB_TEST_CHECK(v6External.fromString("2606:4700::1"));
    BLIB_TEST_CHECK(!router.route(v6External, result));
}
