#pragma once

#include <gravelands/common/config.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

#include <cstring>

namespace gravelands
{
    /**
     * Протокол Gravelands (header-only бинарный кодек, без аллокаций):
     *
     * Сообщение: [type:u8][payloadSize:u16 LE][payload...].
     * Типы пакетов:
     * - Command  (клиент → сервер): ввод игрока (направление X/Z);
     * - Snapshot (сервер → клиент): позиции юнитов авторитетной сцены;
     * - Welcome  (сервер → клиент): параметры сессии (тикрейт, свой юнит).
     *
     * Кодирование — в буфер вызывающего (encode* возвращает размер,
     * 0 — не хватило места); декодирование — строгое (битый пакет →
     * false, состояние out не меняется). Числа — little-endian.
     */

    // Тип сообщения (заголовок)
    enum class PacketType : buint8
    {
        None = 0,
        Command = 1,
        Snapshot = 2,
        Welcome = 3
    };

    // Размер заголовка сообщения (type + payloadSize)
    constexpr buint32 protocolHeaderSize = 3;

    // Максимальный размер полезной нагрузки одного сообщения
    constexpr buint32 protocolPayloadMaxSize = maxPacketBytes - protocolHeaderSize;

    /**
     * Ввод игрока: нормализованное направление по осям X/Z
     * (каждая компонента -1/0/+1; диагонали — по 1 по обеим осям,
     * нормализует симуляция при необходимости).
     */
    struct PlayerCommand
    {
        bint8 moveX;
        bint8 moveZ;
    };

    /**
     * Запись позиции юнита в снапшоте.
     */
    struct SnapshotEntry
    {
        buint64 entityId;
        bfloat positionX;
        bfloat positionY;
        bfloat positionZ;
    };

    /**
     * Приветствие сессии (декодированное).
     */
    struct WelcomePacket
    {
        buint32 serverTickRate;
        buint64 playerEntityId;
    };

    // ========== Кодирование (в буфер вызывающего) ==========

    buint32 encodeCommandPacket(
        _Out buint8* out, buint32 capacity, _In const PlayerCommand& command);

    buint32 encodeSnapshotPacket(
        _Out buint8* out, buint32 capacity,
        buint32 tickNumber, _In const SnapshotEntry* entries, buint32 entryCount);

    buint32 encodeWelcomePacket(
        _Out buint8* out, buint32 capacity,
        buint32 serverTickRate, buint64 playerEntityId);

    // ========== Декодирование (строгое; false — пакет битый) ==========

    bool decodeCommandPacket(
        _In const buint8* payload, buint32 payloadSize, _Out PlayerCommand& outCommand);

    /**
     * Декодирование снапшота: записи пишутся в массив вызывающего
     * (без аллокаций и невыровненного reinterpret_cast). Битый пакет —
     * false, out-параметры не меняются.
     */
    bool decodeSnapshotPacket(
        _In const buint8* payload, buint32 payloadSize,
        _Out buint32& outTickNumber, _Out buint32& outEntryCount,
        _Out SnapshotEntry* outEntries, buint32 entryCapacity);

    bool decodeWelcomePacket(
        _In const buint8* payload, buint32 payloadSize, _Out WelcomePacket& outWelcome);

    /**
     * MessageFramer — накопление байтов TCP-потока и разбор целых
     * сообщений (TCP не гарантирует границ сообщений). Без аллокаций:
     * внутренний буфер maxPacketBytes. Один фреймер на соединение.
     *
     * nextMessage КОПИРУЕТ payload в буфер вызывающего (внутренний
     * буфер сдвигается — указатели в него не живут между вызовами).
     */
    class MessageFramer
    {
    private:
        buint8 buffer[maxPacketBytes];
        buint32 bufferSize;

    public:
        MessageFramer()
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
         * @return false — полного сообщения ещё нет
         */
        bool nextMessage(
            _Out buint8* outPayload, buint32 payloadCapacity,
            _Out buint32& outPayloadSize, _Out PacketType& outType);

        /**
         * Сброс накопленного буфера (переподключение).
         */
        void reset()
        {
            this->bufferSize = 0;
        }
    };

    // ========== Реализация (header-only) ==========

    namespace protocolimpl
    {
        // Запись примитивов little-endian с проверкой границ
        inline bool putU8(_In buint8*& cursor, _In buint32& remaining, buint8 value)
        {
            if (remaining < 1)
            {
                return false;
            }
            *cursor++ = value;
            --remaining;
            return true;
        }

        inline bool putU16(_In buint8*& cursor, _In buint32& remaining, buint16 value)
        {
            if (remaining < 2)
            {
                return false;
            }
            cursor[0] = static_cast<buint8>(value & 0xFF);
            cursor[1] = static_cast<buint8>((value >> 8) & 0xFF);
            cursor += 2;
            remaining -= 2;
            return true;
        }

        inline bool putU32(_In buint8*& cursor, _In buint32& remaining, buint32 value)
        {
            if (remaining < 4)
            {
                return false;
            }
            cursor[0] = static_cast<buint8>(value & 0xFF);
            cursor[1] = static_cast<buint8>((value >> 8) & 0xFF);
            cursor[2] = static_cast<buint8>((value >> 16) & 0xFF);
            cursor[3] = static_cast<buint8>((value >> 24) & 0xFF);
            cursor += 4;
            remaining -= 4;
            return true;
        }

        inline bool putU64(_In buint8*& cursor, _In buint32& remaining, buint64 value)
        {
            if (remaining < 8)
            {
                return false;
            }
            for (buint32 i = 0; i < 8; ++i)
            {
                cursor[i] = static_cast<buint8>((value >> (i * 8)) & 0xFF);
            }
            cursor += 8;
            remaining -= 8;
            return true;
        }

        inline bool putF32(_In buint8*& cursor, _In buint32& remaining, bfloat value)
        {
            buint32 bits = 0;
            static_assert(sizeof(bfloat) == sizeof(buint32), "protocol expects 32-bit bfloat");
            std::memcpy(&bits, &value, sizeof(bits));
            return putU32(cursor, remaining, bits);
        }

        // Чтение примитивов little-endian с проверкой границ
        inline bool getU8(_In const buint8*& cursor, _In buint32& remaining, _Out buint8& outValue)
        {
            if (remaining < 1)
            {
                return false;
            }
            outValue = *cursor++;
            --remaining;
            return true;
        }

        inline bool getU16(_In const buint8*& cursor, _In buint32& remaining, _Out buint16& outValue)
        {
            if (remaining < 2)
            {
                return false;
            }
            outValue = static_cast<buint16>(cursor[0]) | static_cast<buint16>(cursor[1] << 8);
            cursor += 2;
            remaining -= 2;
            return true;
        }

        inline bool getU32(_In const buint8*& cursor, _In buint32& remaining, _Out buint32& outValue)
        {
            if (remaining < 4)
            {
                return false;
            }
            outValue = static_cast<buint32>(cursor[0])
                | (static_cast<buint32>(cursor[1]) << 8)
                | (static_cast<buint32>(cursor[2]) << 16)
                | (static_cast<buint32>(cursor[3]) << 24);
            cursor += 4;
            remaining -= 4;
            return true;
        }

        inline bool getU64(_In const buint8*& cursor, _In buint32& remaining, _Out buint64& outValue)
        {
            if (remaining < 8)
            {
                return false;
            }
            outValue = 0;
            for (buint32 i = 0; i < 8; ++i)
            {
                outValue |= static_cast<buint64>(cursor[i]) << (i * 8);
            }
            cursor += 8;
            remaining -= 8;
            return true;
        }

        inline bool getF32(_In const buint8*& cursor, _In buint32& remaining, _Out bfloat& outValue)
        {
            buint32 bits = 0;
            if (!getU32(cursor, remaining, bits))
            {
                return false;
            }
            static_assert(sizeof(bfloat) == sizeof(buint32), "protocol expects 32-bit bfloat");
            std::memcpy(&outValue, &bits, sizeof(outValue));
            return true;
        }
    }

    inline buint32 encodeCommandPacket(
        _Out buint8* out, buint32 capacity, _In const PlayerCommand& command)
    {
        if (capacity < protocolHeaderSize + 2)
        {
            return 0;
        }

        // Заголовок
        out[0] = static_cast<buint8>(PacketType::Command);
        out[1] = static_cast<buint8>(2);
        out[2] = 0;

        // Полезная нагрузка: moveX, moveZ (сырые bint8-байты)
        out[3] = static_cast<buint8>(command.moveX);
        out[4] = static_cast<buint8>(command.moveZ);

        return protocolHeaderSize + 2;
    }

    inline buint32 encodeSnapshotPacket(
        _Out buint8* out, buint32 capacity,
        buint32 tickNumber, _In const SnapshotEntry* entries, buint32 entryCount)
    {
        // Полезная нагрузка: tickNumber(4) + entryCount(2) + 20*N
        const buint64 payloadSize64 = 6 + static_cast<buint64>(entryCount) * 20;
        if (payloadSize64 > protocolPayloadMaxSize || capacity < protocolHeaderSize + static_cast<buint32>(payloadSize64))
        {
            return 0;
        }

        buint8* cursor = out;
        buint32 remaining = capacity;

        if (!protocolimpl::putU8(cursor, remaining, static_cast<buint8>(PacketType::Snapshot)) ||
            !protocolimpl::putU16(cursor, remaining, static_cast<buint16>(payloadSize64)))
        {
            return 0;
        }

        if (!protocolimpl::putU32(cursor, remaining, tickNumber) ||
            !protocolimpl::putU16(cursor, remaining, static_cast<buint16>(entryCount)))
        {
            return 0;
        }

        for (buint32 i = 0; i < entryCount; ++i)
        {
            if (!protocolimpl::putU64(cursor, remaining, entries[i].entityId) ||
                !protocolimpl::putF32(cursor, remaining, entries[i].positionX) ||
                !protocolimpl::putF32(cursor, remaining, entries[i].positionY) ||
                !protocolimpl::putF32(cursor, remaining, entries[i].positionZ))
            {
                return 0;
            }
        }

        return static_cast<buint32>(cursor - out);
    }

    inline buint32 encodeWelcomePacket(
        _Out buint8* out, buint32 capacity,
        buint32 serverTickRate, buint64 playerEntityId)
    {
        constexpr buint32 payloadSize = 12;

        buint8* cursor = out;
        buint32 remaining = capacity;

        if (!protocolimpl::putU8(cursor, remaining, static_cast<buint8>(PacketType::Welcome)) ||
            !protocolimpl::putU16(cursor, remaining, payloadSize) ||
            !protocolimpl::putU32(cursor, remaining, serverTickRate) ||
            !protocolimpl::putU64(cursor, remaining, playerEntityId))
        {
            return 0;
        }

        return static_cast<buint32>(cursor - out);
    }

    inline bool decodeCommandPacket(
        _In const buint8* payload, buint32 payloadSize, _Out PlayerCommand& outCommand)
    {
        if (payloadSize != 2)
        {
            return false;
        }
        outCommand.moveX = static_cast<bint8>(payload[0]);
        outCommand.moveZ = static_cast<bint8>(payload[1]);
        return true;
    }

    inline bool decodeSnapshotPacket(
        _In const buint8* payload, buint32 payloadSize,
        _Out buint32& outTickNumber, _Out buint32& outEntryCount,
        _Out SnapshotEntry* outEntries, buint32 entryCapacity)
    {
        const buint8* cursor = payload;
        buint32 remaining = payloadSize;

        buint16 entryCount16 = 0;
        if (!protocolimpl::getU32(cursor, remaining, outTickNumber) ||
            !protocolimpl::getU16(cursor, remaining, entryCount16))
        {
            return false;
        }

        const buint32 entryCount = entryCount16;
        if (entryCount > entryCapacity || remaining != entryCount * 20)
        {
            return false;
        }

        // Чтение по примитивам — без невыровненного reinterpret_cast
        for (buint32 i = 0; i < entryCount; ++i)
        {
            if (!protocolimpl::getU64(cursor, remaining, outEntries[i].entityId) ||
                !protocolimpl::getF32(cursor, remaining, outEntries[i].positionX) ||
                !protocolimpl::getF32(cursor, remaining, outEntries[i].positionY) ||
                !protocolimpl::getF32(cursor, remaining, outEntries[i].positionZ))
            {
                return false;
            }
        }

        outEntryCount = entryCount;
        return remaining == 0;
    }

    inline bool decodeWelcomePacket(
        _In const buint8* payload, buint32 payloadSize, _Out WelcomePacket& outWelcome)
    {
        const buint8* cursor = payload;
        buint32 remaining = payloadSize;

        if (!protocolimpl::getU32(cursor, remaining, outWelcome.serverTickRate) ||
            !protocolimpl::getU64(cursor, remaining, outWelcome.playerEntityId))
        {
            return false;
        }
        return remaining == 0;
    }

    inline bool MessageFramer::pushBytes(_In const buint8* data, buint32 size)
    {
        if (size > maxPacketBytes - this->bufferSize)
        {
            // Переполнение буфера — поток рассинхронизирован
            return false;
        }
        std::memcpy(this->buffer + this->bufferSize, data, size);
        this->bufferSize += size;
        return true;
    }

    inline bool MessageFramer::nextMessage(
        _Out buint8* outPayload, buint32 payloadCapacity,
        _Out buint32& outPayloadSize, _Out PacketType& outType)
    {
        if (this->bufferSize < protocolHeaderSize)
        {
            return false;
        }

        const buint8 typeByte = this->buffer[0];
        if (typeByte < static_cast<buint8>(PacketType::Command) ||
            typeByte > static_cast<buint8>(PacketType::Welcome))
        {
            // Неизвестный тип — поток битый: сбрасываем буфер
            this->bufferSize = 0;
            return false;
        }

        const buint32 payloadSize =
            static_cast<buint32>(this->buffer[1]) | (static_cast<buint32>(this->buffer[2]) << 8);
        if (payloadSize > protocolPayloadMaxSize || payloadSize > payloadCapacity)
        {
            this->bufferSize = 0;
            return false;
        }

        if (this->bufferSize < protocolHeaderSize + payloadSize)
        {
            return false; // сообщение ещё не пришло целиком
        }

        outType = static_cast<PacketType>(typeByte);
        outPayloadSize = payloadSize;

        // Копия payload наружу (внутренний буфер сейчас сдвинется —
        // указатели в него не должны жить между вызовами)
        std::memcpy(outPayload, this->buffer + protocolHeaderSize, payloadSize);

        // Сдвиг остатка в начало (следующие сообщения потока)
        const buint32 remaining = this->bufferSize - protocolHeaderSize - payloadSize;
        if (remaining > 0)
        {
            std::memmove(this->buffer, this->buffer + protocolHeaderSize + payloadSize, remaining);
        }
        this->bufferSize = remaining;
        return true;
    }

} // namespace gravelands
