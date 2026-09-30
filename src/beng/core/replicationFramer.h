#pragma once

#include <beng/config.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace beng
{
    // Максимальный размер одного сетевого сообщения (заголовок + payload);
    // общий для фреймера и кодека репликации
    constexpr buint32 maxReplicationPacketBytes = 2048;

    /**
     * Типы пакетов движкового сетевого слоя (beng-server ↔ клиент игры).
     * Заголовок пакета: [type:u8][payloadSize:u16 LE] + payload.
     */
    enum class ReplicationPacketType : buint8
    {
        None = 0,

        // Клиент → сервер: команда игрока (payload — игра-специфичный
        // кодек; движок возит непрозрачные байты)
        Command = 1,

        // Сервер → клиент: снапшот репликации (payload — replicationCodec)
        Snapshot = 2,

        // Сервер → клиент: рукопожатие сессии (payload — replicationCodec)
        Welcome = 3
    };

    // Размер заголовка сообщения (type + payloadSize)
    constexpr buint32 replicationHeaderSize = 3;

    // Максимальный размер полезной нагрузки одного сообщения
    constexpr buint32 replicationPayloadMaxSize = maxReplicationPacketBytes - replicationHeaderSize;

    /**
     * ReplicationFramer — накопление байтов TCP-потока и разбор целых
     * сообщений (TCP не гарантирует границ сообщений). Без аллокаций:
     * внутренний буфер maxReplicationPacketBytes. Один фреймер на
     * соединение (и на клиенте, и на сервере).
     *
     * nextMessage КОПИРУЕТ payload в буфер вызывающего (внутренний
     * буфер сдвигается — указатели в него не живут между вызовами).
     */
    class __beng_api ReplicationFramer
    {
    private:
        buint8 buffer[maxReplicationPacketBytes];
        buint32 bufferSize;

    public:
        ReplicationFramer()
            : bufferSize(0)
        {
        }

        /**
         * Добавить принятые байты потока. Лишние (сверх буфера) —
         * отбрасываются с возвратом false (протокол нарушен).
         */
        bool pushBytes(_In const buint8* data, buint32 size);

        /**
         * Извлечь следующее целое сообщение (payload копируется в
         * outPayload; битый поток — false, буфер сбрасывается).
         * @return false — полного сообщения ещё нет (или поток битый)
         */
        bool nextMessage(
            _Out buint8* outPayload, buint32 payloadCapacity,
            _Out buint32& outPayloadSize, _Out ReplicationPacketType& outType);

        /**
         * Сброс накопленного буфера (переподключение).
         */
        void reset()
        {
            this->bufferSize = 0;
        }
    };

} // namespace beng
