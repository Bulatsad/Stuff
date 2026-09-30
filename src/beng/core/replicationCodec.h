#pragma once

#include <beng/config.h>
#include <beng/core/componentReflection.h>
#include <beng/core/replicationFramer.h>
#include <beng/core/replicationSchema.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace beng
{
    // ========== Лимиты снапшот-структур (без аллокаций на проводе) ==========
    //
    // Лимиты подобраны так, чтобы ДЕКОДИРОВАННЫЙ снапшот умещался на
    // стеке потока (~128 КБ при текущих капах) — он материализуется
    // на стеке вызывающего (ReplicationClientState::applySnapshot,
    // тесты). Рост мира за лимиты — не фатально: сервер шлёт снапшот
    // по частям не может (MVP), поэтому лимиты честные и проверяются.

    // Максимум сущностей в одном снапшоте
    constexpr buint32 maxSnapshotEntities = 64;

    // Максимум компонентов одной сущности в снапшоте
    constexpr buint32 maxSnapshotComponentsPerEntity = 8;

    // Максимальная длина имени wire-типа в Welcome (стековый буфер)
    constexpr buint32 maxWireTypeNameLength = 63;

    /**
     * Пакетный флаг сущности в снапшоте.
     */
    enum ReplicationEntityFlags : buint8
    {
        // Полное состояние: все реплицируемые поля всех компонентов
        // (спавн или принудительный ресинк клиента)
        replicationEntityFull = 1u << 0,

        // Уничтожение: у записи только entityId, компонентов нет
        replicationEntityDestroy = 1u << 1
    };

    /**
     * Компонент сущности в снапшоте (позиция писателя): либо полное
     * состояние (все поля схемы), либо дельта (только изменённые).
     */
    struct ReplicationComponentPatch
    {
        // Локальный ComponentType ЭТОЙ сцены (писатель знает его);
        // на провод пишется wire-id из схемы
        ComponentType localTypeId;

        // Индексы полей в списке реплицируемых полей схемы типа
        // (для full — 0..count-1 подряд; для delta — изменённые)
        buint8 fieldIndices[maxReplicatedFields];

        // Значения полей (параллельно fieldIndices)
        FieldValue fieldValues[maxReplicatedFields];

        // Число полей (для full обязано совпадать со схемой)
        buint8 fieldCount;
    };

    /**
     * Сущность в снапшоте (позиция писателя).
     */
    struct ReplicationEntityPatch
    {
        EntityID entityId;

        // Флаги ReplicationEntityFlags (full/destroy)
        buint8 flags;

        // Компоненты (для destroy — пусто)
        ReplicationComponentPatch components[maxSnapshotComponentsPerEntity];
        buint8 componentCount;
    };

    /**
     * Поле компонента в ДЕКОДИРОВАННОМ снапшоте.
     */
    struct ReplicationFieldPatch
    {
        // Индекс в списке реплицируемых полей схемы типа
        // (для full — 0..count-1 по порядку)
        buint8 fieldIndex;
        FieldValue value;
    };

    /**
     * Компонент в ДЕКОДИРОВАННОМ снапшоте.
     */
    struct DecodedReplicationComponent
    {
        // Wire-id типа (резолв в локальный тип — на стороне
        // ReplicationClientState по Welcome-маппингу)
        buint8 wireTypeId;

        // Полное состояние (все поля схемы) или дельта
        bool full;

        ReplicationFieldPatch fields[maxReplicatedFields];
        buint32 fieldCount;
    };

    /**
     * Сущность в ДЕКОДИРОВАННОМ снапшоте.
     */
    struct DecodedReplicationEntity
    {
        EntityID entityId;
        bool destroy;
        bool full;
        DecodedReplicationComponent components[maxSnapshotComponentsPerEntity];
        buint32 componentCount;
    };

    /**
     * Декодированный снапшот (клиент применяет его к mirror-сцене —
     * см. replicationClientState.h).
     */
    struct DecodedReplicationSnapshot
    {
        buint32 tickNumber;
        DecodedReplicationEntity entities[maxSnapshotEntities];
        buint32 entityCount;
    };

    /**
     * Декодированный Welcome (рукопожатие сессии).
     */
    struct DecodedReplicationWelcome
    {
        buint32 tickRate;
        EntityID playerEntityId;

        struct TypeEntry
        {
            char name[maxWireTypeNameLength + 1];
            buint32 schemaHash;
        } types[maxWireTypes];

        buint32 typeCount;
    };

    // ========== Примитивы поля (значение + kind-байт) ==========

    /**
     * Записать значение поля в поток: [kind:u8][payload]. Используется
     * и в снапшотах, и в тестах round-trip.
     * @return false — невалидный kind (Unset и т.п.)
     */
    bool writeFieldValue(_In blib::core::IOutputStream& os, _In const FieldValue& value);

    /**
     * Прочитать значение поля из потока (строго: неизвестный kind /
     * обрыв потока — false).
     */
    bool readFieldValue(_In blib::core::IInputStream& is, _Out FieldValue& outValue);

    // ========== Welcome ==========

    /**
     * Закодировать Welcome в буфер вызывающего (без аллокаций).
     * @return размер сообщения; 0 — не влезло/ошибка
     */
    buint32 encodeReplicationWelcome(
        _Out buint8* out, buint32 capacity,
        buint32 tickRate, EntityID playerEntityId, _In const ReplicationSchema& schema);

    /**
     * Декодировать Welcome (строго; битый пакет — false, out не меняется).
     */
    bool decodeReplicationWelcome(
        _In const buint8* payload, buint32 payloadSize, _Out DecodedReplicationWelcome& outWelcome);

    // ========== Снапшот ==========

    /**
     * Закодировать снапшот в буфер вызывающего (без аллокаций).
     * wire-id берётся из схемы по локальному типу патча.
     * @return размер сообщения; 0 — не влезло/ошибка
     */
    buint32 encodeReplicationSnapshot(
        _Out buint8* out, buint32 capacity,
        buint32 tickNumber,
        _In const ReplicationEntityPatch* entities, buint32 entityCount,
        _In const ReplicationSchema& schema);

    /**
     * Декодировать снапшот (строго; битый пакет — false, out не меняется).
     * Структуры фиксированных размеров — без аллокаций.
     */
    bool decodeReplicationSnapshot(
        _In const buint8* payload, buint32 payloadSize, _Out DecodedReplicationSnapshot& outSnapshot);

} // namespace beng
