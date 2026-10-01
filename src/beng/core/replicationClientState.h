#pragma once

#include <beng/config.h>
#include <beng/core/replicationCodec.h>
#include <beng/core/replicationSchema.h>

#include <blib/blibint.h>
#include <blib/system/memory/allocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

#include <vector>

namespace beng
{
    class Scene;

    // ========== Клиентская интерполяция снапшотов (beng-core) ==========

    // Задержка рендера интерполяции в тиках сервера: зеркало рисует
    // состояния на N тиков ПОЗАДИ новейшего снапшота — постоянный лаг,
    // приём-джиттер поглощается буфером, скорость интерполяции ровная
    constexpr buint32 interpolationDelayTicks = 2;

    // Глубина кольца сэмплов одного поля (delay + 2: пара, охватывающая
    // целевое время рендера, плюс запас на джиттер приёма)
    constexpr buint32 maxInterpolationSamples = interpolationDelayTicks + 2;

    // Пул состояний интерполяции: максимум (сущность × компонент × поле)
    // слотов. Слоты выделяются ЛЕНИВО — только интерполируемые поля
    // реально попадают в кольца (см. InterpolationFieldState)
    constexpr buint32 interpolationSlotPoolSize =
        maxSnapshotEntities * maxSnapshotComponentsPerEntity * maxReplicatedFields;

    /**
     * ReplicationClientState — клиентская сторона рефлексивной
     * репликации (beng-core, без сети/графики): принимает декодированные
     * снапшоты, применяет их к mirror-сцене клиента и интерполирует
     * непрерывные поля между снапшотами.
     *
     * Зеркала: сущности на клиентской сцене воспроизводят СЕРВЕРНЫЕ
     * EntityID (Scene::createEntityWithId — монотонные ID с пропуском
     * невиданных диапазонов); компоненты создаются/обновляются через
     * рефлексию (IComponentField::setValue) — без compile-time T.
     *
     * Протокол:
     * - acceptWelcome сверяет схему пира со СВОЕЙ (по стабильным
     *   именам типов + хешам наборов полей) и строит wire-id →
     *   локальный ComponentType; рассинхрон — false (сессию не
     *   открывать);
     * - applySnapshot: destroy — удаляет зеркало; full — создаёт
     *   недостающие сущности/компоненты и переписывает состояние
     *   (ресинк); delta — применяет только изменённые поля.
     *
     * Интерполяция (см. SERVER.md, «Клиентская интерполяция»):
     * - поля с флагом IComponentField::isInterpolated (position) НЕ
     *   пишутся в сцену при applySnapshot — значения буферизуются в
     *   per-field кольца сэмплов с временем приёма;
     * - renderMirror пишет в сцену интерполированные значения на
     *   nowSeconds − delay (delay = interpolationDelayTicks / tickRate);
     *   без экстраполяции: время рендера вне диапазона сэмплов —
     *   держим ближайший;
     * - интерполяция поддерживает kind Vector3 (позиция); иной kind с
     *   флагом interpolated — применяется новейший сэмпл без lerp.
     *
     * События зеркала: спавн/уничтожение зеркальных сущностей копятся
     * в буферы (takeSpawnEvents/takeDestroyEvents) — игра сливает их в
     * своём кадре (повесить визуал на зеркало и т.п.). Не слитые
     * события живут до следующего слива.
     *
     * Ограничения (MVP, задокументированы в SERVER.md):
     * - удаление КОМПОНЕНТА у живой сущности не реплицируется
     *   (ресинк только создаёт/обновляет); сущности — только целиком;
     * - лимиты структур — maxSnapshotEntities и т.д. (см. replicationCodec.h).
     */
    class __beng_api ReplicationClientState
    {
    public:
        ReplicationClientState();
        ~ReplicationClientState();

        ReplicationClientState(const ReplicationClientState&) = delete;
        ReplicationClientState& operator=(const ReplicationClientState&) = delete;

        /**
         * Принять Welcome и построить wire-маппинг (сверка схем).
         * @param scene Mirror-сцена клиента (типы уже зарегистрированы)
         * @return false — рассинхрон схем/неизвестный тип (сессия не открывается)
         */
        bool acceptWelcome(_In const Scene& scene, _In const DecodedReplicationWelcome& welcome,
            _Out buint32& outTickRate, _Out EntityID& outPlayerEntity);

        /**
         * Применить снапшот к mirror-сцене: неинтерполируемые поля —
         * сразу, интерполируемые — в кольца сэмплов (receiveTime),
         * события спавна/уничтожения — в take-буферы.
         *
         * @param receiveTime Клиентские секунды приёма снапшота
         *        (монотонны в рамках сессии; из них строится
         *        интерполяция в renderMirror)
         * @return false — структурная ошибка протокола (поток не лечится)
         */
        bool applySnapshot(_In Scene& scene, _In const DecodedReplicationSnapshot& snapshot,
            bfloat receiveTime);

        /**
         * Записать интерполированные значения полей в mirror-сцену на
         * момент nowSeconds − delay (см. комментарий к классу).
         * Вызывать каждый кадр после applySnapshot-потока (игры и
         * системы читают сцену после этого вызова).
         */
        void renderMirror(_In Scene& scene, bfloat nowSeconds);

        /**
         * Сколько зеркальных сущностей знает клиент (диагностика).
         */
        buint32 getKnownEntityCount() const { return static_cast<buint32>(this->knownEntities.size()); }

        /**
         * Слить события спавна зеркальных сущностей (копятся в
         * applySnapshot). Копирует до capacity штук в out (nullptr
         * допустим при capacity == 0) и убирает их из буфера.
         * @return Сколько событий скопировано
         */
        buint32 takeSpawnEvents(_Out_opt EntityID* out, buint32 capacity);

        /**
         * Слить события уничтожения зеркальных сущностей (семантика
         * как у takeSpawnEvents).
         */
        buint32 takeDestroyEvents(_Out_opt EntityID* out, buint32 capacity);

        /**
         * Новейший известный СЕРВЕРНЫЙ сэмпл поля зеркальной сущности —
         * для реконсиляции движкового client-side prediction игрока
         * (см. CLIENT.md): сравнение с последним снапшотом, а не с
         * интерполированным значением сцены — то отстаёт на
         * interpolationDelayTicks. Резолв по стабильным именам: имя
         * типа → wire-id (схема Welcome), имя поля → индекс
         * реплицируемого поля (дескриптор рефлексии).
         *
         * @return false — тип/поле не реплицируются, зеркало неизвестно
         *         или сэмплов ещё не было (снапшот не пришёл)
         */
        bool getLatestFieldSample(EntityID entityId, _In const char* componentTypeName,
            _In const char* fieldName, _Out FieldValue& outValue) const;

        /**
         * Сброс состояния (переподключение к серверу): маппинг, зеркала,
         * кольца интерполяции, события.
         */
        void reset();

    private:
        // Один сэмпл интерполируемого поля: значение на времени приёма
        struct InterpolationSample
        {
            bfloat receiveTime; // клиентские секунды приёма снапшота
            FieldValue value;
        };

        // Кольцо сэмплов одного интерполируемого поля зеркальной
        // сущности. Слоты пула выделяются лениво; свободные связаны
        // списком через nextFree (entityId == invalidEntity = свободен)
        struct InterpolationFieldState
        {
            EntityID entityId;
            buint8 wireTypeId;
            buint8 fieldIndex;
            buint32 start; // индекс старейшего сэмпла в кольце
            buint32 count;
            buint32 nextFree; // только для свободных слотов
            InterpolationSample samples[maxInterpolationSamples];
        };

        // wire-id → локальный ComponentType (0xFF — нет соответствия)
        buint8 wireToLocal[maxWireTypes];
        buint32 wireTypeCount;

        // Своя схема (из mirror-сцены — build в acceptWelcome)
        ReplicationSchema schema;

        // Известные зеркальные сущности (линейный поиск — MVP-лимиты
        // малы; контейнер через blib-аллокатор, см. AGENTS.md)
        blib::memory::Allocator containerAllocator;
        std::vector<EntityID, blib::memory::StdAllocatorAdapter<EntityID>> knownEntities;

        // Тикрейт сессии (из Welcome) — для задержки рендера
        buint32 serverTickRate;

        // Пул состояний интерполяции — в куче (GlobalAllocator):
        // размер не для стека (~interpolationSlotPoolSize слотов)
        InterpolationFieldState* slotPool;

        // Голова свободного списка пула; interpolationSlotPoolSize —
        // сентинел «пул пуст» (невалидный индекс)
        buint32 freeSlotHead;

        // События зеркала (спавн/уничтожение сущностей) — сливает игра
        EntityID spawnEvents[maxSnapshotEntities];
        buint32 spawnEventCount;
        EntityID destroyEvents[maxSnapshotEntities];
        buint32 destroyEventCount;

        bool isKnown(EntityID id) const;
        void rememberEntity(EntityID id);
        void forgetEntity(EntityID id);

        /**
         * Применить компоненты сущности (full/delta) к mirror-сцене.
         * Сущность обязана уже существовать (создаётся вызывающим).
         */
        bool applyEntityComponents(_In Scene& scene, EntityID entityId,
            _In const DecodedReplicationEntity& entity, bfloat receiveTime);

        // ===== Интерполяция =====

        InterpolationFieldState* findSlot(EntityID entityId, buint8 wireTypeId, buint8 fieldIndex);
        const InterpolationFieldState* findSlot(EntityID entityId, buint8 wireTypeId, buint8 fieldIndex) const;
        InterpolationFieldState* allocSlot(EntityID entityId, buint8 wireTypeId, buint8 fieldIndex);

        // Очистить кольца всех полей типа сущности (full-ресинк)
        void clearTypeSlots(EntityID entityId, buint8 wireTypeId);

        // Вернуть слоты сущности в свободный список (уничтожение зеркала)
        void freeEntitySlots(EntityID entityId);

        // Вернуть ВСЕ слоты в свободный список (reset)
        void releaseAllSlots();

        // Добавить сэмпл в кольцо (старейший вытесняется при переполнении)
        void pushSample(_In_Out InterpolationFieldState& slot, bfloat receiveTime,
            _In const FieldValue& value);
    };

} // namespace beng
