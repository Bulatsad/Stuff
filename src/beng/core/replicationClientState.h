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

    /**
     * ReplicationClientState — клиентская сторона рефлексивной
     * репликации (beng-core, без сети/графики): применяет декодированные
     * снапшоты к mirror-сцене клиента.
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
     *   недостающие сущности/компоненты и применяет ВСЕ поля схемы
     *   (ресинк); delta — применяет только изменённые поля.
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
        ~ReplicationClientState() = default;

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
         * Применить снапшот к mirror-сцене.
         * @return false — структурная ошибка протокола (поток не лечится)
         */
        bool applySnapshot(_In Scene& scene, _In const DecodedReplicationSnapshot& snapshot);

        /**
         * Сколько зеркальных сущностей знает клиент (диагностика).
         */
        buint32 getKnownEntityCount() const { return static_cast<buint32>(this->knownEntities.size()); }

        /**
         * Сброс состояния (переподключение к серверу).
         */
        void reset();

    private:
        // wire-id → локальный ComponentType (0xFF — нет соответствия)
        buint8 wireToLocal[maxWireTypes];
        buint32 wireTypeCount;

        // Своя схема (из mirror-сцены — build в acceptWelcome)
        ReplicationSchema schema;

        // Известные зеркальные сущности (линейный поиск — MVP-лимиты
        // малы; контейнер через blib-аллокатор, см. AGENTS.md)
        blib::memory::Allocator containerAllocator;
        std::vector<EntityID, blib::memory::StdAllocatorAdapter<EntityID>> knownEntities;

        bool isKnown(EntityID id) const;
        void rememberEntity(EntityID id);
        void forgetEntity(EntityID id);

        /**
         * Применить компоненты сущности (full/delta) к mirror-сцене.
         * Сущность обязана уже существовать (создаётся вызывающим).
         */
        bool applyEntityComponents(_In Scene& scene, EntityID entityId,
            _In const DecodedReplicationEntity& entity);
    };

} // namespace beng
