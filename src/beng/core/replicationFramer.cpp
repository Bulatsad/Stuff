#include <beng/core/replicationFramer.h>

#include <cstring>

namespace beng
{
    bool ReplicationFramer::pushBytes(_In const buint8* data, buint32 size)
    {
        if (size > maxReplicationPacketBytes - this->bufferSize)
        {
            // Переполнение буфера — поток рассинхронизирован
            return false;
        }
        std::memcpy(this->buffer + this->bufferSize, data, size);
        this->bufferSize += size;
        return true;
    }

    bool ReplicationFramer::nextMessage(
        _Out buint8* outPayload, buint32 payloadCapacity,
        _Out buint32& outPayloadSize, _Out ReplicationPacketType& outType)
    {
        if (this->bufferSize < replicationHeaderSize)
        {
            return false;
        }

        const buint8 typeByte = this->buffer[0];
        if (typeByte < static_cast<buint8>(ReplicationPacketType::Command) ||
            typeByte > static_cast<buint8>(ReplicationPacketType::Welcome))
        {
            // Неизвестный тип — поток битый: сбрасываем буфер
            this->bufferSize = 0;
            return false;
        }

        const buint32 payloadSize =
            static_cast<buint32>(this->buffer[1]) | (static_cast<buint32>(this->buffer[2]) << 8);
        if (payloadSize > replicationPayloadMaxSize || payloadSize > payloadCapacity)
        {
            this->bufferSize = 0;
            return false;
        }

        if (this->bufferSize < replicationHeaderSize + payloadSize)
        {
            return false; // сообщение ещё не пришло целиком
        }

        outType = static_cast<ReplicationPacketType>(typeByte);
        outPayloadSize = payloadSize;

        // Копия payload наружу (внутренний буфер сейчас сдвинется —
        // указатели в него не должны жить между вызовами)
        std::memcpy(outPayload, this->buffer + replicationHeaderSize, payloadSize);

        // Сдвиг остатка в начало (следующие сообщения потока)
        const buint32 remaining = this->bufferSize - replicationHeaderSize - payloadSize;
        if (remaining > 0)
        {
            std::memmove(this->buffer, this->buffer + replicationHeaderSize + payloadSize, remaining);
        }
        this->bufferSize = remaining;
        return true;
    }

} // namespace beng
