#pragma once

#include <gravelands/common/config.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace gravelands
{
    /**
     * Протокол Gravelands (header-only кодек КОМАНД игры).
     *
     * Состояние мира реплицирует ДВИЖОК (рефлексивная репликация
     * beng-server: снапшоты Transform и т.д. — см. SERVER.md); игра
     * отвечает только за свой кодек ввода. Команды возит движковый
     * сетевой слой как непрозрачный payload (ReplicationClient::
     * sendCommand / IServerGame::onClientCommand).
     *
     * Собственный протокол снапшотов/Welcome (PacketType/MessageFramer/
     * SnapshotEntry) удалён в фазе 3 вместе с рукописной сетью —
     * вместо них движковый ReplicationPacketType/ReplicationFramer.
     */

    /**
     * Ввод игрока: направление по осям X/Z
     * (каждая компонента -1/0/+1; диагонали — по 1 по обеим осям,
     * нормализует симуляция при необходимости).
     */
    struct PlayerCommand
    {
        bint8 moveX;
        bint8 moveZ;
    };

    // Размер payload команды в байтах (moveX + moveZ, сырые i8)
    constexpr buint32 commandPayloadSize = 2;

    /**
     * Закодировать команду в буфер вызывающего (без аллокаций).
     * @return размер payload; 0 — не хватило места
     */
    inline buint32 encodeCommandPayload(
        _Out buint8* out, buint32 capacity, _In const PlayerCommand& command)
    {
        if (capacity < commandPayloadSize)
        {
            return 0;
        }
        out[0] = static_cast<buint8>(command.moveX);
        out[1] = static_cast<buint8>(command.moveZ);
        return commandPayloadSize;
    }

    /**
     * Декодировать команду (строго: неверный размер — false,
     * out-параметр не меняется).
     */
    inline bool decodeCommandPayload(
        _In const buint8* payload, buint32 payloadSize, _Out PlayerCommand& outCommand)
    {
        if (payloadSize != commandPayloadSize)
        {
            return false;
        }
        outCommand.moveX = static_cast<bint8>(payload[0]);
        outCommand.moveZ = static_cast<bint8>(payload[1]);
        return true;
    }

} // namespace gravelands
