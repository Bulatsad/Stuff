#pragma once

#include <blib/network/socket.h>

namespace blib
{
    namespace network
    {
        class TcpSocket
        {
        private:
            Socket socket;
        public:
            TcpSocket();
            TcpSocket(AddressType type);
            TcpSocket(const TcpSocket&) = delete;
            TcpSocket(TcpSocket&&) = delete;

            bool setBlocking(bool isBlocking);

            /**
             * См. Socket::setTcpNoDelay: отключение алгоритма Нейгла для
             * real-time трафика. Вызывать на подключённом сокете.
             */
            bool setTcpNoDelay(bool enable);

            SocketStatus bind(Address& addr);

            /**
             * Подключение к адресу. В неблокирующем режиме соединение
             * устанавливается асинхронно: WouldBlock = «в процессе» —
             * вызывающий повторяет connect позже (WSAEISCONN → OK).
             */
            SocketStatus connect(Address& addr);

            /**
             * Отправка данных. Цикл до полной отправки; при WouldBlock —
             * сколько реально ушло лежит в sentOut, вызывающий продолжает
             * с того же места (данные частично уже в сети).
             *
             * @param data Буфер
             * @param size Размер в байтах
             * @param sentOut Реально отправлено байт (nullptr — не нужно)
             */
            SocketStatus send(const void* data, int size, int* sentOut = nullptr);

            /**
             * Приём данных. size — вход: размер буфера; выход:
             * фактически принятые байты (может быть меньше запрошенного —
             * это НЕ ошибка, TCP — поток).
             *
             * @return Disconnected — соединение закрыто; WouldBlock —
             *         данных нет (неблокирующий режим); Error — сбой
             */
            SocketStatus recv(void* data, int& size);

            /**
             * Причина последнего отказа (см. Socket::getLastError).
             */
            NetworkError getLastError() const { return this->socket.getLastError(); }

            Socket* getSocket();
        };
    }
}
