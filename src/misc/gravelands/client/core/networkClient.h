#pragma once

#include <gravelands/common/config.h>
#include <gravelands/common/protocol.h>

#include <blib/network/tcpSocket.h>

namespace gravelands
{
    /**
     * NetworkClient — сетевая сторона клиента Gravelands (loopback,
     * неблокирующие сокеты + опрос в кадре — без потоков, см.
     * GRAVELANDS.md, «Сетевой цикл»).
     *
     * Обязанности:
     * - подключение к серверу (асинхронное, неблокирующий connect);
     * - накопление входящего потока (MessageFramer), извлечение
     *   приветствия сессии и снапшотов;
     * - отправка команд игрока.
     */
    class NetworkClient
    {
    public:
        // Состояние подключения
        enum class State : buint8
        {
            Disconnected, // нет соединения (или потеряно)
            Connecting,   // connect в процессе (WouldBlock-цикл)
            Connected
        };

        NetworkClient();
        ~NetworkClient();

        NetworkClient(const NetworkClient&) = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;

        /**
         * Начать подключение к loopback-серверу на порту.
         */
        void connect(buint32 port);

        /**
         * Опрос: продвижение подключения, чтение снапшотов/приветствия.
         * Вызывается каждый кадр клиента.
         */
        void poll();

        /**
         * Состояние подключения.
         */
        State getState() const { return state; }

        /**
         * Забрать последнее приветствие сессии (после подключения).
         * @return false — приветствия ещё не было
         */
        bool takeWelcome(_Out WelcomePacket& outWelcome);

        /**
         * Забрать последний снапшот (записи в буфер вызывающего).
         * @return false — новых снапшотов нет
         */
        bool takeSnapshot(
            _Out buint32& outTickNumber,
            _Out SnapshotEntry* outEntries, buint32 entryCapacity,
            _Out buint32& outEntryCount);

        /**
         * Отправить команду игрока (при WouldBlock — пропускается,
         * следующая команда придёт в следующем кадре).
         */
        void sendCommand(_In const PlayerCommand& command);

        /**
         * Закрыть соединение (идемпотентно).
         */
        void shutdown();

    private:
        State state;
        blib::network::TcpSocket socket;
        buint32 serverPort;
        MessageFramer framer;

        buint8 recvBuffer[maxPacketBytes];
        buint8 sendBuffer[maxPacketBytes];

        // Последние полученные данные (до взятия хостом)
        WelcomePacket pendingWelcome;
        bool pendingWelcomeValid;
        SnapshotEntry pendingEntries[maxNetworkUnits];
        buint32 pendingEntryCount;
        buint32 pendingTickNumber;
        bool pendingSnapshotValid;
    };

} // namespace gravelands
