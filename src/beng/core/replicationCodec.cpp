#include <beng/core/replicationCodec.h>

#include <blib/core/console/console.h>

#include <cstring>

namespace beng
{
    namespace replicationimpl
    {
        // ========== Примитивы курсорной записи little-endian ==========

        struct WriteCursor
        {
            buint8* ptr;
            buint32 remaining;

            WriteCursor(_In buint8* data, buint32 size)
                : ptr(data)
                , remaining(size)
            {
            }
        };

        struct ReadCursor
        {
            const buint8* ptr;
            buint32 remaining;

            ReadCursor(_In const buint8* data, buint32 size)
                : ptr(data)
                , remaining(size)
            {
            }
        };

        inline bool putU8(_In_Out WriteCursor& c, buint8 value)
        {
            if (c.remaining < 1)
            {
                return false;
            }
            *c.ptr++ = value;
            --c.remaining;
            return true;
        }

        inline bool putU16(_In_Out WriteCursor& c, buint16 value)
        {
            if (c.remaining < 2)
            {
                return false;
            }
            c.ptr[0] = static_cast<buint8>(value & 0xFF);
            c.ptr[1] = static_cast<buint8>((value >> 8) & 0xFF);
            c.ptr += 2;
            c.remaining -= 2;
            return true;
        }

        inline bool putU32(_In_Out WriteCursor& c, buint32 value)
        {
            if (c.remaining < 4)
            {
                return false;
            }
            c.ptr[0] = static_cast<buint8>(value & 0xFF);
            c.ptr[1] = static_cast<buint8>((value >> 8) & 0xFF);
            c.ptr[2] = static_cast<buint8>((value >> 16) & 0xFF);
            c.ptr[3] = static_cast<buint8>((value >> 24) & 0xFF);
            c.ptr += 4;
            c.remaining -= 4;
            return true;
        }

        inline bool putU64(_In_Out WriteCursor& c, buint64 value)
        {
            if (c.remaining < 8)
            {
                return false;
            }
            for (buint32 i = 0; i < 8; ++i)
            {
                c.ptr[i] = static_cast<buint8>((value >> (i * 8)) & 0xFF);
            }
            c.ptr += 8;
            c.remaining -= 8;
            return true;
        }

        inline bool putF32(_In_Out WriteCursor& c, bfloat value)
        {
            buint32 bits = 0;
            static_assert(sizeof(bfloat) == sizeof(buint32), "replication codec expects 32-bit bfloat");
            std::memcpy(&bits, &value, sizeof(bits));
            return putU32(c, bits);
        }

        inline bool getU8(_In_Out ReadCursor& c, _Out buint8& outValue)
        {
            if (c.remaining < 1)
            {
                return false;
            }
            outValue = *c.ptr++;
            --c.remaining;
            return true;
        }

        inline bool getU16(_In_Out ReadCursor& c, _Out buint16& outValue)
        {
            if (c.remaining < 2)
            {
                return false;
            }
            outValue = static_cast<buint16>(c.ptr[0]) | static_cast<buint16>(c.ptr[1] << 8);
            c.ptr += 2;
            c.remaining -= 2;
            return true;
        }

        inline bool getU32(_In_Out ReadCursor& c, _Out buint32& outValue)
        {
            if (c.remaining < 4)
            {
                return false;
            }
            outValue = static_cast<buint32>(c.ptr[0])
                | (static_cast<buint32>(c.ptr[1]) << 8)
                | (static_cast<buint32>(c.ptr[2]) << 16)
                | (static_cast<buint32>(c.ptr[3]) << 24);
            c.ptr += 4;
            c.remaining -= 4;
            return true;
        }

        inline bool getU64(_In_Out ReadCursor& c, _Out buint64& outValue)
        {
            if (c.remaining < 8)
            {
                return false;
            }
            outValue = 0;
            for (buint32 i = 0; i < 8; ++i)
            {
                outValue |= static_cast<buint64>(c.ptr[i]) << (i * 8);
            }
            c.ptr += 8;
            c.remaining -= 8;
            return true;
        }

        inline bool getF32(_In_Out ReadCursor& c, _Out bfloat& outValue)
        {
            buint32 bits = 0;
            if (!getU32(c, bits))
            {
                return false;
            }
            static_assert(sizeof(bfloat) == sizeof(buint32), "replication codec expects 32-bit bfloat");
            std::memcpy(&outValue, &bits, sizeof(outValue));
            return true;
        }

        /**
         * Записать значение поля: [kind:u8][payload] (курсорная форма —
         * единый формат для снапшотов и потоков).
         */
        bool putFieldValue(_In_Out WriteCursor& c, _In const FieldValue& value)
        {
            const buint8 kindByte = static_cast<buint8>(value.kind);
            switch (value.kind)
            {
                case FieldValue::Kind::Float:
                    return putU8(c, kindByte) && putF32(c, value.floatValue);
                case FieldValue::Kind::Int:
                    return putU8(c, kindByte) && putU32(c, static_cast<buint32>(value.intValue));
                case FieldValue::Kind::Bool:
                    return putU8(c, kindByte) && putU8(c, value.boolValue ? 1 : 0);
                case FieldValue::Kind::Vector3:
                    return putU8(c, kindByte)
                        && putF32(c, value.vector3Value.x)
                        && putF32(c, value.vector3Value.y)
                        && putF32(c, value.vector3Value.z);
                case FieldValue::Kind::Entity:
                    return putU8(c, kindByte) && putU64(c, value.entityValue);
                default:
                    // Unset/неизвестный kind — не реплицируется
                    return false;
            }
        }

        /**
         * Прочитать значение поля: [kind:u8][payload] (курсорная форма).
         */
        bool getFieldValue(_In_Out ReadCursor& c, _Out FieldValue& outValue)
        {
            buint8 kindByte = 0;
            if (!getU8(c, kindByte))
            {
                return false;
            }

            switch (static_cast<FieldValue::Kind>(kindByte))
            {
                case FieldValue::Kind::Float:
                {
                    bfloat v = 0.0f;
                    if (!getF32(c, v))
                    {
                        return false;
                    }
                    outValue = FieldValue::fromFloat(v);
                    return true;
                }
                case FieldValue::Kind::Int:
                {
                    buint32 v = 0;
                    if (!getU32(c, v))
                    {
                        return false;
                    }
                    outValue = FieldValue::fromInt(static_cast<bint32>(v));
                    return true;
                }
                case FieldValue::Kind::Bool:
                {
                    buint8 v = 0;
                    if (!getU8(c, v))
                    {
                        return false;
                    }
                    outValue = FieldValue::fromBool(v != 0);
                    return true;
                }
                case FieldValue::Kind::Vector3:
                {
                    bfloat x = 0.0f;
                    bfloat y = 0.0f;
                    bfloat z = 0.0f;
                    if (!getF32(c, x) || !getF32(c, y) || !getF32(c, z))
                    {
                        return false;
                    }
                    outValue = FieldValue::fromVector3(blib::math::Vector<float, 3>(x, y, z));
                    return true;
                }
                case FieldValue::Kind::Entity:
                {
                    buint64 v = 0;
                    if (!getU64(c, v))
                    {
                        return false;
                    }
                    outValue = FieldValue::fromEntity(v);
                    return true;
                }
                default:
                    // Неизвестный kind — протокол нарушен
                    return false;
            }
        }
    }

    bool writeFieldValue(_In blib::core::IOutputStream& os, _In const FieldValue& value)
    {
        // Максимум байтов значения: kind(1) + Vector3(12)
        constexpr buint32 maxFieldValueBytes = 13;
        buint8 buffer[maxFieldValueBytes];
        replicationimpl::WriteCursor cursor(buffer, maxFieldValueBytes);

        if (!replicationimpl::putFieldValue(cursor, value))
        {
            return false;
        }

        const buint32 written = maxFieldValueBytes - cursor.remaining;
        return os.write(buffer, written) == written;
    }

    bool readFieldValue(_In blib::core::IInputStream& is, _Out FieldValue& outValue)
    {
        // Читаем kind-байт, затем payload по типу — строго, без
        // перечитывания (частичный read — обрыв потока). Курсор
        // строится над полным значением [kind][payload...]
        buint8 buffer[13];
        if (is.read(buffer, 1) != 1)
        {
            return false;
        }

        buint32 payloadSize = 0;
        switch (static_cast<FieldValue::Kind>(buffer[0]))
        {
            case FieldValue::Kind::Float:
            case FieldValue::Kind::Int:
                payloadSize = 4;
                break;
            case FieldValue::Kind::Bool:
                payloadSize = 1;
                break;
            case FieldValue::Kind::Vector3:
                payloadSize = 12;
                break;
            case FieldValue::Kind::Entity:
                payloadSize = 8;
                break;
            default:
                return false;
        }

        // Полное чтение payload (stream может вернуть меньше)
        buint32 readTotal = 0;
        while (readTotal < payloadSize)
        {
            const size_t chunk = is.read(buffer + 1 + readTotal, payloadSize - readTotal);
            if (chunk == 0)
            {
                return false;
            }
            readTotal += static_cast<buint32>(chunk);
        }

        replicationimpl::ReadCursor cursor(buffer, 1 + payloadSize);
        return replicationimpl::getFieldValue(cursor, outValue);
    }

    buint32 encodeReplicationWelcome(
        _Out buint8* out, buint32 capacity,
        buint32 tickRate, EntityID playerEntityId, _In const ReplicationSchema& schema)
    {
        replicationimpl::WriteCursor cursor(out, capacity);

        if (!replicationimpl::putU32(cursor, tickRate) ||
            !replicationimpl::putU64(cursor, playerEntityId))
        {
            return 0;
        }

        const buint32 typeCount = schema.getWireTypeCount();
        if (!replicationimpl::putU8(cursor, static_cast<buint8>(typeCount)))
        {
            return 0;
        }

        for (buint32 w = 0; w < typeCount; ++w)
        {
            const ReplicationSchema::WireType* wire = schema.getWireType(w);
            const buint32 nameLength = static_cast<buint32>(std::strlen(wire->name));
            if (nameLength > maxWireTypeNameLength ||
                !replicationimpl::putU8(cursor, static_cast<buint8>(nameLength)))
            {
                return 0;
            }
            if (cursor.remaining < nameLength)
            {
                return 0;
            }
            for (buint32 i = 0; i < nameLength; ++i)
            {
                cursor.ptr[i] = static_cast<buint8>(wire->name[i]);
            }
            cursor.ptr += nameLength;
            cursor.remaining -= nameLength;

            if (!replicationimpl::putU32(cursor, wire->schemaHash))
            {
                return 0;
            }
        }

        return capacity - cursor.remaining;
    }

    bool decodeReplicationWelcome(
        _In const buint8* payload, buint32 payloadSize, _Out DecodedReplicationWelcome& outWelcome)
    {
        replicationimpl::ReadCursor cursor(payload, payloadSize);

        if (!replicationimpl::getU32(cursor, outWelcome.tickRate) ||
            !replicationimpl::getU64(cursor, outWelcome.playerEntityId))
        {
            return false;
        }

        buint8 typeCount = 0;
        if (!replicationimpl::getU8(cursor, typeCount))
        {
            return false;
        }

        if (typeCount > maxWireTypes)
        {
            return false;
        }

        for (buint32 w = 0; w < typeCount; ++w)
        {
            buint8 nameLength = 0;
            if (!replicationimpl::getU8(cursor, nameLength) || nameLength > maxWireTypeNameLength)
            {
                return false;
            }
            if (cursor.remaining < nameLength)
            {
                return false;
            }

            DecodedReplicationWelcome::TypeEntry& entry = outWelcome.types[w];
            for (buint32 i = 0; i < nameLength; ++i)
            {
                entry.name[i] = static_cast<char>(cursor.ptr[i]);
            }
            entry.name[nameLength] = '\0';
            cursor.ptr += nameLength;
            cursor.remaining -= nameLength;

            if (!replicationimpl::getU32(cursor, entry.schemaHash))
            {
                return false;
            }
        }

        outWelcome.typeCount = typeCount;
        return cursor.remaining == 0;
    }

    buint32 encodeReplicationSnapshot(
        _Out buint8* out, buint32 capacity,
        buint32 tickNumber,
        _In const ReplicationEntityPatch* entities, buint32 entityCount,
        _In const ReplicationSchema& schema)
    {
        if (entityCount > maxSnapshotEntities)
        {
            return 0;
        }

        replicationimpl::WriteCursor cursor(out, capacity);

        if (!replicationimpl::putU32(cursor, tickNumber) ||
            !replicationimpl::putU16(cursor, static_cast<buint16>(entityCount)))
        {
            return 0;
        }

        for (buint32 e = 0; e < entityCount; ++e)
        {
            const ReplicationEntityPatch& entity = entities[e];

            if (!replicationimpl::putU64(cursor, entity.entityId) ||
                !replicationimpl::putU8(cursor, entity.flags))
            {
                return 0;
            }

            if ((entity.flags & replicationEntityDestroy) != 0)
            {
                // Destroy-запись: компонентов нет
                continue;
            }

            if (entity.componentCount > maxSnapshotComponentsPerEntity ||
                !replicationimpl::putU8(cursor, entity.componentCount))
            {
                return 0;
            }

            for (buint32 comp = 0; comp < entity.componentCount; ++comp)
            {
                const ReplicationComponentPatch& patch = entity.components[comp];

                // Локальный тип → wire-id; нереплицируемый тип — ошибка писателя
                const ComponentType wireId = schema.getWireIdForLocalType(patch.localTypeId);
                if (wireId == invalidComponentType)
                {
                    __blib_log_warning("ReplicationCodec: component patch with non-replicated local type %u",
                        static_cast<buint32>(patch.localTypeId));
                    return 0;
                }

                const bool full = (entity.flags & replicationEntityFull) != 0;
                if (!replicationimpl::putU8(cursor, wireId) ||
                    !replicationimpl::putU8(cursor, full ? 1 : 0) ||
                    !replicationimpl::putU8(cursor, patch.fieldCount))
                {
                    return 0;
                }

                for (buint32 f = 0; f < patch.fieldCount; ++f)
                {
                    if (full)
                    {
                        // Полное состояние: индексы полей — 0..count-1,
                        // на проводе только значения
                        if (!replicationimpl::putFieldValue(cursor, patch.fieldValues[f]))
                        {
                            return 0;
                        }
                    }
                    else
                    {
                        // Дельта: явные пары (индекс, значение)
                        if (!replicationimpl::putU8(cursor, patch.fieldIndices[f]) ||
                            !replicationimpl::putFieldValue(cursor, patch.fieldValues[f]))
                        {
                            return 0;
                        }
                    }
                }
            }
        }

        return capacity - cursor.remaining;
    }

    bool decodeReplicationSnapshot(
        _In const buint8* payload, buint32 payloadSize, _Out DecodedReplicationSnapshot& outSnapshot)
    {
        replicationimpl::ReadCursor cursor(payload, payloadSize);

        buint16 entityCount16 = 0;
        if (!replicationimpl::getU32(cursor, outSnapshot.tickNumber) ||
            !replicationimpl::getU16(cursor, entityCount16))
        {
            return false;
        }

        if (entityCount16 > maxSnapshotEntities)
        {
            return false;
        }

        for (buint32 e = 0; e < entityCount16; ++e)
        {
            DecodedReplicationEntity& entity = outSnapshot.entities[e];

            buint8 flags = 0;
            if (!replicationimpl::getU64(cursor, entity.entityId) ||
                !replicationimpl::getU8(cursor, flags))
            {
                return false;
            }

            entity.destroy = (flags & replicationEntityDestroy) != 0;
            entity.full = (flags & replicationEntityFull) != 0;
            entity.componentCount = 0;

            if (entity.destroy)
            {
                continue;
            }

            buint8 componentCount = 0;
            if (!replicationimpl::getU8(cursor, componentCount) ||
                componentCount > maxSnapshotComponentsPerEntity)
            {
                return false;
            }

            for (buint32 comp = 0; comp < componentCount; ++comp)
            {
                DecodedReplicationComponent& component = entity.components[comp];

                buint8 fullByte = 0;
                buint8 fieldCount = 0;
                if (!replicationimpl::getU8(cursor, component.wireTypeId) ||
                    !replicationimpl::getU8(cursor, fullByte) ||
                    !replicationimpl::getU8(cursor, fieldCount) ||
                    fieldCount > maxReplicatedFields)
                {
                    return false;
                }

                component.full = fullByte != 0;
                component.fieldCount = fieldCount;

                for (buint32 f = 0; f < fieldCount; ++f)
                {
                    ReplicationFieldPatch& field = component.fields[f];
                    if (component.full)
                    {
                        field.fieldIndex = static_cast<buint8>(f);
                        if (!replicationimpl::getFieldValue(cursor, field.value))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (!replicationimpl::getU8(cursor, field.fieldIndex) ||
                            !replicationimpl::getFieldValue(cursor, field.value))
                        {
                            return false;
                        }
                    }
                }
            }

            entity.componentCount = componentCount;
        }

        outSnapshot.entityCount = entityCount16;
        return cursor.remaining == 0;
    }

} // namespace beng
