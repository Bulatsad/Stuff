#pragma once

#include <blib/network/socket.h>
#include <blib/network/tcpSocket.h>

namespace blib
{
    namespace network
    {
        class TcpListener
        {
        private:
            Socket socket;

        public:
            TcpListener();
            TcpListener(address::AddressType type);
            TcpListener(const TcpListener&) = delete;
            TcpListener(TcpListener&&) = delete;

            bool setBlocking(bool isBlocking);

            SocketStatus bind(_In const address::Tcp& endpoint);
            SocketStatus listen(int backlog = 16);

            /**
             * Открыть (пересоздать) слушающий сокет. Нужен повторному
             * запуску сервера (PIE: Play → Stop → Play): shutdown
             * закрыл сокет — bind на закрытом хендле не сработает.
             * Идемпотентно пригоден и для первого запуска: старый
             * сокет (если был) освобождается.
             */
            SocketStatus open(address::AddressType type);

            /**
             * Закрыть слушающий сокет (идемпотентно). Память хендла
             * освобождается при следующем open()/деструкторе.
             */
            SocketStatus close();

            /**
             * Принять входящее подключение. В неблокирующем режиме
             * WouldBlock = «подключений нет» — вызывающий повторяет позже.
             */
            SocketStatus accept(TcpSocket& accepted);

            /**
             * Причина последнего отказа (см. Socket::getLastError).
             */
            NetworkError getLastError() const { return this->socket.getLastError(); }
        };
    }
}
