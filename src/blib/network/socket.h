#pragma once

#include <blib/blibint.h>

#include <blib/network/address.h>

namespace blib
{
    namespace network
    {
        /**
         * NetworkError — ошибки сетевых операций (enum-код модуля,
         * паттерн ошибок проекта: None = 0 — успех). Хранится сокетом
         * (getLastError) — детализирующая причина последнего отказа.
         */
        enum class NetworkError : buint32
        {
            None = 0,
            Unknown,          // нераспознанная причина (диагностики нет)
            NotInitialized,   // winsock не инициализирован (InitBlibSocket не вызван/провалился)
            CreateFailed,     // socket() вернул INVALID_SOCKET
            BindFailed,
            ListenFailed,
            ConnectFailed,
            SendFailed,
            RecvFailed,
            WouldBlock,       // неблокирующий сокет: операция не готова (WSAEWOULDBLOCK)
            Closed            // соединение закрыто/сброшено
        };

        enum class SocketStatus
        {
            OK,
            Partial,     // отправлена только часть данных (см. sentOut в send)
            Disconnected,
            WouldBlock,  // неблокирующий сокет: операция не готова — повторить позже
            Error,

            END_OF_ENUM
        };

        enum class SocketType
        {
            Stream,      // TCP
            Dgram,       // UDP
            Raw,
            RDM,
            SeqPacket,

            END_OF_ENUM
        };

        enum class SocketProtocol
        {
            ICMP = 1,
            IGMP,
            RFCOMM,
            TCP,
            UDP,
            ICMPv6,
            RM,

            END_OF_ENUM
        };

        /**
         * Инициализация сетевой подсистемы (WSAStartup). Идемпотентна:
         * повторный вызов — no-op с true. Вызывать до создания сокетов.
         *
         * @return false — инициализация провалилась (сеть недоступна)
         */
        bool InitBlibSocket();

        class Socket
        {
        public:
            Socket();
            SocketStatus create(const address::AddressType af, const SocketType type, const SocketProtocol protocol);
            SocketStatus create(void* ctx);

            /**
             * Режим блокировки. @return true — успех (семантика исправлена:
             * раньше возвращался инвертированный результат ioctlsocket)
             */
            bool setBlocking(bool isBlocking);

            /**
             * Алгоритм Нейгла (TCP_NODELAY). Включён по умолчанию
             * системой: мелкие пакеты накапливаются и уходят с
             * задержкой (Nagle + delayed ACK) — для real-time сообщений
             * (команды, снапшоты) это заметная латентность.
             * Вызывать на ПОДКЛЮЧЁННОМ/принятом сокете.
             *
             * @param enable true — пакеты отправляются немедленно
             * @return true — опция установлена
             */
            bool setTcpNoDelay(bool enable);

            /**
             * Привязать сокет к IP-адресу и порту (реализовано для
             * IPv4; прочие семейства — Error).
             */
            SocketStatus bind(_In const address::Address& ip, _In buint16 port);

            SocketStatus close();
            void destroy();
            ~Socket();

            /**
             * Причина последнего отказа операции (enum-ошибка модуля).
             */
            NetworkError getLastError() const { return lastError; }

            /**
             * INTERNAL: записать причину отказа (используется impl/win
             * после неудачной операции). Не вызывать извне.
             */
            void __setLastError(NetworkError error) { this->lastError = error; }

            Socket(const Socket&) = delete;
            Socket(Socket&&) = delete;

            void* __getHandler();
        private:
            void* ctx;
            NetworkError lastError;
        };
 
    }
}
