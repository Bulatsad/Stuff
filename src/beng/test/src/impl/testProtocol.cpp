#include <blib/test/src/test.h>

#include <gravelands/common/config.h>
#include <gravelands/common/protocol.h>

#include <cstring>

// Тесты бинарного протокола Gravelands: roundtrip всех пакетов,
// строгость декодеров, фреймер поверх TCP-потока

namespace
{
    // Буферы кодирования
    buint8 encodeBuffer[gravelands::maxPacketBytes];

    // Предел записей в декодированном снапшоте
    constexpr buint32 maxDecodedEntries = 64;
    gravelands::SnapshotEntry decodedEntries[maxDecodedEntries];
}

BLIB_TEST_CASE("protocol: command packet roundtrip")
{
    const gravelands::PlayerCommand command{ static_cast<bint8>(1), static_cast<bint8>(-1) };

    const buint32 size = gravelands::encodeCommandPacket(encodeBuffer, gravelands::maxPacketBytes, command);
    BLIB_TEST_CHECK(size > 0);
    BLIB_TEST_CHECK(encodeBuffer[0] == static_cast<buint8>(gravelands::PacketType::Command));

    gravelands::PlayerCommand decoded{ 0, 0 };
    BLIB_TEST_CHECK(gravelands::decodeCommandPacket(
        encodeBuffer + gravelands::protocolHeaderSize, size - gravelands::protocolHeaderSize, decoded));
    BLIB_TEST_CHECK(decoded.moveX == 1);
    BLIB_TEST_CHECK(decoded.moveZ == -1);
}

BLIB_TEST_CASE("protocol: snapshot packet roundtrip")
{
    gravelands::SnapshotEntry entries[3] = {
        { 1, 10.0f, 0.0f, 20.0f },
        { 2, -5.5f, 2.0f, 30.25f },
        { 3, 0.0f, 0.0f, 0.0f }
    };

    const buint32 size = gravelands::encodeSnapshotPacket(
        encodeBuffer, gravelands::maxPacketBytes, 12345, entries, 3);
    BLIB_TEST_CHECK(size > 0);

    buint32 tickNumber = 0;
    buint32 entryCount = 0;
    BLIB_TEST_CHECK(gravelands::decodeSnapshotPacket(
        encodeBuffer + gravelands::protocolHeaderSize, size - gravelands::protocolHeaderSize,
        tickNumber, entryCount, decodedEntries, maxDecodedEntries));
    BLIB_TEST_CHECK(tickNumber == 12345);
    BLIB_TEST_CHECK(entryCount == 3);
    BLIB_TEST_CHECK(decodedEntries[1].entityId == 2);
    BLIB_TEST_CHECK_CLOSE(decodedEntries[1].positionX, -5.5f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(decodedEntries[1].positionY, 2.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(decodedEntries[1].positionZ, 30.25f, 0.0001f);
}

BLIB_TEST_CASE("protocol: welcome packet roundtrip")
{
    const buint32 size = gravelands::encodeWelcomePacket(
        encodeBuffer, gravelands::maxPacketBytes, gravelands::serverTickRate, 42);
    BLIB_TEST_CHECK(size > 0);

    gravelands::WelcomePacket welcome{ 0, 0 };
    BLIB_TEST_CHECK(gravelands::decodeWelcomePacket(
        encodeBuffer + gravelands::protocolHeaderSize, size - gravelands::protocolHeaderSize, welcome));
    BLIB_TEST_CHECK(welcome.serverTickRate == gravelands::serverTickRate);
    BLIB_TEST_CHECK(welcome.playerEntityId == 42);
}

BLIB_TEST_CASE("protocol: decoders reject malformed payloads")
{
    buint8 payload[32] = {};
    std::memset(payload, 0, sizeof(payload));

    // Команда: неверный размер
    gravelands::PlayerCommand command{ 0, 0 };
    BLIB_TEST_CHECK(!gravelands::decodeCommandPacket(payload, 3, command));
    BLIB_TEST_CHECK(!gravelands::decodeCommandPacket(payload, 0, command));

    // Снапшот: неверная раскладка (tickNumber + count + неполные записи)
    buint32 tickNumber = 0;
    buint32 entryCount = 0;
    BLIB_TEST_CHECK(!gravelands::decodeSnapshotPacket(
        payload, 10, tickNumber, entryCount, decodedEntries, maxDecodedEntries));

    // Снапшот: записей больше, чем вмещает буфер вызывающего
    gravelands::SnapshotEntry entries[2] = { { 1, 0.0f, 0.0f, 0.0f }, { 2, 0.0f, 0.0f, 0.0f } };
    const buint32 size = gravelands::encodeSnapshotPacket(encodeBuffer, gravelands::maxPacketBytes, 0, entries, 2);
    BLIB_TEST_CHECK(!gravelands::decodeSnapshotPacket(
        encodeBuffer + gravelands::protocolHeaderSize, size - gravelands::protocolHeaderSize,
        tickNumber, entryCount, decodedEntries, 1));

    // Welcome: лишний байт — отказ
    gravelands::WelcomePacket welcome{ 0, 0 };
    BLIB_TEST_CHECK(!gravelands::decodeWelcomePacket(payload, 13, welcome));
}

BLIB_TEST_CASE("protocol: encode rejects undersized buffers")
{
    gravelands::PlayerCommand command{ 0, 0 };
    BLIB_TEST_CHECK(gravelands::encodeCommandPacket(encodeBuffer, 1, command) == 0);

    gravelands::SnapshotEntry entries[1] = { { 1, 0.0f, 0.0f, 0.0f } };
    BLIB_TEST_CHECK(gravelands::encodeSnapshotPacket(encodeBuffer, 10, 0, entries, 1) == 0);
}

BLIB_TEST_CASE("protocol: framer assembles streamed and batched messages")
{
    gravelands::MessageFramer framer;

    // Два сообщения одним куском
    gravelands::PlayerCommand commandA{ 1, 0 };
    const buint32 sizeA = gravelands::encodeCommandPacket(encodeBuffer, gravelands::maxPacketBytes, commandA);
    gravelands::PlayerCommand commandB{ -1, 1 };
    const buint32 sizeB = gravelands::encodeCommandPacket(encodeBuffer + sizeA, gravelands::maxPacketBytes - sizeA, commandB);

    BLIB_TEST_CHECK(framer.pushBytes(encodeBuffer, sizeA + sizeB));

    buint8 payloadBuffer[gravelands::maxPacketBytes];
    buint32 payloadSize = 0;
    gravelands::PacketType type = gravelands::PacketType::None;

    BLIB_TEST_CHECK(framer.nextMessage(payloadBuffer, gravelands::maxPacketBytes, payloadSize, type));
    BLIB_TEST_CHECK(type == gravelands::PacketType::Command);
    gravelands::PlayerCommand decodedA{ 0, 0 };
    BLIB_TEST_CHECK(payloadSize == 2);
    BLIB_TEST_CHECK(gravelands::decodeCommandPacket(payloadBuffer, payloadSize, decodedA));
    BLIB_TEST_CHECK(decodedA.moveX == 1);
    BLIB_TEST_CHECK(decodedA.moveZ == 0);

    BLIB_TEST_CHECK(framer.nextMessage(payloadBuffer, gravelands::maxPacketBytes, payloadSize, type));
    BLIB_TEST_CHECK(type == gravelands::PacketType::Command);
    gravelands::PlayerCommand decodedB{ 0, 0 };
    BLIB_TEST_CHECK(gravelands::decodeCommandPacket(payloadBuffer, payloadSize, decodedB));
    BLIB_TEST_CHECK(decodedB.moveX == -1);
    BLIB_TEST_CHECK(decodedB.moveZ == 1);
    BLIB_TEST_CHECK(!framer.nextMessage(payloadBuffer, gravelands::maxPacketBytes, payloadSize, type)); // больше нет
}

BLIB_TEST_CASE("protocol: framer handles split messages and rejects bad type")
{
    gravelands::MessageFramer framer;

    // Сообщение по одному байту — собирается целиком
    gravelands::PlayerCommand command{ 0, -1 };
    const buint32 size = gravelands::encodeCommandPacket(encodeBuffer, gravelands::maxPacketBytes, command);

    buint8 payloadBuffer[gravelands::maxPacketBytes];
    buint32 payloadSize = 0;
    gravelands::PacketType type = gravelands::PacketType::None;

    for (buint32 i = 0; i < size - 1; ++i)
    {
        BLIB_TEST_CHECK(framer.pushBytes(encodeBuffer + i, 1));
        BLIB_TEST_CHECK(!framer.nextMessage(payloadBuffer, gravelands::maxPacketBytes, payloadSize, type)); // ещё не целиком
    }
    BLIB_TEST_CHECK(framer.pushBytes(encodeBuffer + size - 1, 1));
    BLIB_TEST_CHECK(framer.nextMessage(payloadBuffer, gravelands::maxPacketBytes, payloadSize, type));
    BLIB_TEST_CHECK(type == gravelands::PacketType::Command);
    gravelands::PlayerCommand decoded{ 0, 0 };
    BLIB_TEST_CHECK(gravelands::decodeCommandPacket(payloadBuffer, payloadSize, decoded));
    BLIB_TEST_CHECK(decoded.moveX == 0 && decoded.moveZ == -1);

    // Неизвестный тип в потоке — фреймер сбрасывает буфер (не зависает)
    const buint8 badByte = 0xFF;
    BLIB_TEST_CHECK(framer.pushBytes(&badByte, 1));
    BLIB_TEST_CHECK(framer.pushBytes(&badByte, 2));
    BLIB_TEST_CHECK(!framer.nextMessage(payloadBuffer, gravelands::maxPacketBytes, payloadSize, type));
}
