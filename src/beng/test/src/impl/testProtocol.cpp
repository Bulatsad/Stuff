#include <blib/test/src/test.h>

#include <gravelands/common/config.h>
#include <gravelands/common/protocol.h>

// Тесты кодека команд Gravelands: roundtrip ввода игрока,
// строгость декодера. Состояние мира реплицирует движок
// (группа тестов replication) — собственного протокола снапшотов
// у игры больше нет (фаза 3, см. GRAVELANDS.md)

namespace
{
    // Буфер кодирования
    buint8 encodeBuffer[gravelands::commandPayloadSize * 2];
}

BLIB_TEST_CASE("protocol: command payload roundtrip")
{
    const gravelands::PlayerCommand command{ static_cast<bint8>(1), static_cast<bint8>(-1) };

    const buint32 size = gravelands::encodeCommandPayload(
        encodeBuffer, sizeof(encodeBuffer), command);
    BLIB_TEST_CHECK(size == gravelands::commandPayloadSize);
    BLIB_TEST_CHECK(encodeBuffer[0] == 1);
    BLIB_TEST_CHECK(encodeBuffer[1] == 0xFF); // -1 как сырой i8-байт

    gravelands::PlayerCommand decoded{ 0, 0 };
    BLIB_TEST_CHECK(gravelands::decodeCommandPayload(encodeBuffer, size, decoded));
    BLIB_TEST_CHECK(decoded.moveX == 1);
    BLIB_TEST_CHECK(decoded.moveZ == -1);
}

BLIB_TEST_CASE("protocol: command payload encodes zero vector")
{
    const gravelands::PlayerCommand command{ 0, 0 };
    const buint32 size = gravelands::encodeCommandPayload(
        encodeBuffer, sizeof(encodeBuffer), command);
    BLIB_TEST_CHECK(size == gravelands::commandPayloadSize);
    BLIB_TEST_CHECK(encodeBuffer[0] == 0 && encodeBuffer[1] == 0);
}

BLIB_TEST_CASE("protocol: command decoder rejects malformed payloads")
{
    gravelands::PlayerCommand command{ 9, 9 };

    // Неверный размер — отказ, out не меняется
    BLIB_TEST_CHECK(!gravelands::decodeCommandPayload(encodeBuffer, 3, command));
    BLIB_TEST_CHECK(!gravelands::decodeCommandPayload(encodeBuffer, 0, command));
    BLIB_TEST_CHECK(command.moveX == 9 && command.moveZ == 9);
}

BLIB_TEST_CASE("protocol: command encoder rejects undersized buffers")
{
    const gravelands::PlayerCommand command{ 0, 0 };
    BLIB_TEST_CHECK(gravelands::encodeCommandPayload(
        encodeBuffer, gravelands::commandPayloadSize - 1, command) == 0);
}
