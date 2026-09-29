#pragma once

#include <gravelands/common/config.h>
#include <gravelands/common/protocol.h>

#include <blib/network/tcpListener.h>
#include <blib/network/tcpSocket.h>

namespace gravelands
{
    /**
     * NetworkServer — сетевая сторона сервера Gravelands (MVP: один
     * клиент, loopback). Неблокирующие сокеты + опрос в tick сервера
     * (без потоков — см. GRAVELANDS.md, «Сетевой цикл»).
     *
     * Обязанности:
     * - приём подключения (accept в poll) + приветствие сессии;
     * - накопление входящего потока (MessageFramer) и извлечение
     *   команд игрока;
     * - отправка снапшотов авторитетного состояния.
     *
     * Отправка — non-blocking: при WouldBlock снапшот пропускается
     * (клиент догонит следующим тиком) — буферы loopback глубокие,
     * на практике не случается.
     */
    class NetworkServer
    {
    private:
        // Слушатель (неблокирующий accept)
        blib::network::TcpListener listener;
        bool listening;

        // Подключённый клиент (MVP: один)
        blib::network::TcpSocket client;
        bool clientConnected;
        // Клиент подключился в этом poll (хост шлёт Welcome)
        bool clientJustConnected;

        // Накопление входящего потока клиента
        MessageFramer framer;

        // Буферы recv/отправки
        buint8 recvBuffer[maxPacketBytes];
        buint8 sendBuffer[maxPacketBytes];

        // Последняя принятая команда игрока (пока не забрана хостом)
        PlayerCommand pendingCommand;
        bool pendingCommandValid;

    public:
        NetworkServer();
        ~NetworkServer();

        NetworkServer(const NetworkServer&) = delete;
        NetworkServer& operator=(const NetworkServer&) = delete;

        /**
         * Инициализация: слушатель на loopback-порту.
         * @return false — порт занят/недоступен
         */
        bool initialize(buint32 port);

        /**
         * Опрос сети: принять подключение, прочитать команды клиента.
         * Вызывается каждый тик/кадр сервера.
         */
        void poll();

        /**
         * Подключён ли клиент.
         */
        bool hasClient() const { return clientConnected; }

        /**
         * Клиент подключился в последнем poll (одноразовый флаг —
         * хост шлёт Welcome).
         */
        bool takeClientConnectedEvent();

        /**
         * Забрать последнюю команду игрока.
         * @return false — новых команд не было
         */
        bool takePendingCommand(_Out PlayerCommand& outCommand);

        /**
         * Отправить приветствие сессии (тикрейт + EntityID игрока).
         */
        void sendWelcome(buint32 serverTickRate, buint64 playerEntityId);

        /**
         * Отправить снапшот (позиции юнитов) клиенту.
         */
        void sendSnapshot(buint32 tickNumber, _In const SnapshotEntry* entries, buint32 entryCount);

        /**
         * Корректное гашение (закрытие сокетов).
         */
        void shutdown();
    };

} // namespace gravelands
