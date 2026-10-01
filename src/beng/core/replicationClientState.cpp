#include <beng/core/replicationClientState.h>

#include <beng/core/scene.h>

#include <blib/core/console/console.h>
#include <blib/system/memory/globalAllocator.h>

#include <cstring>

namespace beng
{
    ReplicationClientState::ReplicationClientState()
        : wireTypeCount(0)
        , knownEntities(blib::memory::StdAllocatorAdapter<EntityID>(&this->containerAllocator))
        , serverTickRate(0)
        , slotPool(nullptr)
        , freeSlotHead(interpolationSlotPoolSize)
        , spawnEventCount(0)
        , destroyEventCount(0)
    {
        for (buint32 w = 0; w < maxWireTypes; ++w)
        {
            this->wireToLocal[w] = invalidComponentType;
        }

        // Пул состояний интерполяции — в куче (GlobalAllocator):
        // ~interpolationSlotPoolSize слотов не умещаются на стеке.
        // Свободные слоты связываются списком при releaseAllSlots()
        this->slotPool = static_cast<InterpolationFieldState*>(
            blib::memory::GlobalAllocator::instance().allocate(
                sizeof(InterpolationFieldState) * interpolationSlotPoolSize));
        this->releaseAllSlots();
    }

    ReplicationClientState::~ReplicationClientState()
    {
        if (this->slotPool != nullptr)
        {
            blib::memory::GlobalAllocator::instance().deallocate(
                this->slotPool, sizeof(InterpolationFieldState) * interpolationSlotPoolSize);
            this->slotPool = nullptr;
        }
    }

    void ReplicationClientState::reset()
    {
        this->wireTypeCount = 0;
        for (buint32 w = 0; w < maxWireTypes; ++w)
        {
            this->wireToLocal[w] = invalidComponentType;
        }
        this->knownEntities.clear();
        this->serverTickRate = 0;
        this->spawnEventCount = 0;
        this->destroyEventCount = 0;
        this->releaseAllSlots();
    }

    void ReplicationClientState::releaseAllSlots()
    {
        // Перестроить свободный список пула целиком (слоты не требуют
        // очистки полей — валидность задаёт entityId/freeSlotHead)
        for (buint32 i = 0; i < interpolationSlotPoolSize; ++i)
        {
            this->slotPool[i].entityId = invalidEntity;
            this->slotPool[i].nextFree = i + 1;
        }
        this->slotPool[interpolationSlotPoolSize - 1].nextFree = interpolationSlotPoolSize;
        this->freeSlotHead = 0;
    }

    bool ReplicationClientState::isKnown(EntityID id) const
    {
        // Линейный поиск: MVP-лимиты снапшотов малы (см. шапку)
        for (buint32 i = 0; i < this->knownEntities.size(); ++i)
        {
            if (this->knownEntities[i] == id)
            {
                return true;
            }
        }
        return false;
    }

    void ReplicationClientState::rememberEntity(EntityID id)
    {
        if (!this->isKnown(id))
        {
            this->knownEntities.push_back(id);
        }
    }

    void ReplicationClientState::forgetEntity(EntityID id)
    {
        for (buint32 i = 0; i < this->knownEntities.size(); ++i)
        {
            if (this->knownEntities[i] == id)
            {
                this->knownEntities.erase(this->knownEntities.begin() + i);
                return;
            }
        }
    }

    buint32 ReplicationClientState::takeSpawnEvents(_Out_opt EntityID* out, buint32 capacity)
    {
        const buint32 count = this->spawnEventCount < capacity ? this->spawnEventCount : capacity;
        for (buint32 i = 0; i < count; ++i)
        {
            out[i] = this->spawnEvents[i];
        }
        // Остаток сдвигаем в начало (игра может сливать частями)
        for (buint32 i = count; i < this->spawnEventCount; ++i)
        {
            this->spawnEvents[i - count] = this->spawnEvents[i];
        }
        this->spawnEventCount -= count;
        return count;
    }

    buint32 ReplicationClientState::takeDestroyEvents(_Out_opt EntityID* out, buint32 capacity)
    {
        const buint32 count = this->destroyEventCount < capacity ? this->destroyEventCount : capacity;
        for (buint32 i = 0; i < count; ++i)
        {
            out[i] = this->destroyEvents[i];
        }
        for (buint32 i = count; i < this->destroyEventCount; ++i)
        {
            this->destroyEvents[i - count] = this->destroyEvents[i];
        }
        this->destroyEventCount -= count;
        return count;
    }

    bool ReplicationClientState::getLatestFieldSample(EntityID entityId,
        _In const char* componentTypeName, _In const char* fieldName,
        _Out FieldValue& outValue) const
    {
        // Wire-id по стабильному имени типа (схема Welcome)
        buint32 wireId = 0;
        bool wireFound = false;
        for (buint32 w = 0; w < this->wireTypeCount; ++w)
        {
            const ReplicationSchema::WireType* wire = this->schema.getWireType(w);
            if (wire != nullptr && std::strcmp(wire->name, componentTypeName) == 0)
            {
                wireId = w;
                wireFound = true;
                break;
            }
        }
        if (!wireFound)
        {
            return false; // тип не реплицируется (или Welcome не принят)
        }

        const ComponentType localType = this->wireToLocal[wireId];
        if (localType == invalidComponentType)
        {
            return false;
        }

        // Индекс реплицируемого поля по имени (дескриптор рефлексии;
        // индексы совпадают с fieldIndex слотов интерполяции)
        buint8 fieldIndex = 0;
        bool fieldFound = false;
        const buint32 fieldCount = this->schema.getReplicatedFieldCount(localType);
        for (buint32 f = 0; f < fieldCount; ++f)
        {
            const IComponentField* field = this->schema.getReplicatedField(localType, f);
            if (field != nullptr && std::strcmp(field->getName(), fieldName) == 0)
            {
                fieldIndex = static_cast<buint8>(f);
                fieldFound = true;
                break;
            }
        }
        if (!fieldFound)
        {
            return false;
        }

        // Новейший сэмпл кольца (последний по времени приёма)
        const InterpolationFieldState* slot = this->findSlot(
            entityId, static_cast<buint8>(wireId), fieldIndex);
        if (slot == nullptr || slot->count == 0)
        {
            return false; // зеркало неизвестно или снапшот ещё не пришёл
        }

        outValue = slot->samples[(slot->start + slot->count - 1) % maxInterpolationSamples].value;
        return true;
    }

    bool ReplicationClientState::acceptWelcome(_In const Scene& scene,
        _In const DecodedReplicationWelcome& welcome,
        _Out buint32& outTickRate, _Out EntityID& outPlayerEntity)
    {
        // Своя схема — из mirror-сцены (типы игры зарегистрированы)
        this->schema.build(scene);

        if (welcome.typeCount > maxWireTypes)
        {
            return false;
        }

        for (buint32 w = 0; w < welcome.typeCount; ++w)
        {
            const DecodedReplicationWelcome::TypeEntry& entry = welcome.types[w];

            // Локальный тип по стабильному имени (перебор реестра сцены)
            ComponentType localType = invalidComponentType;
            const buint32 localTypeCount = scene.getComponentTypeCount();
            for (buint32 t = 0; t < localTypeCount; ++t)
            {
                if (std::strcmp(scene.getComponentTypeName(static_cast<ComponentType>(t)), entry.name) == 0)
                {
                    localType = static_cast<ComponentType>(t);
                    break;
                }
            }

            if (localType == invalidComponentType)
            {
                // Типа нет в клиентской сборке — версии игры расходятся
                __blib_log_error("ReplicationClientState: welcome references unknown type '%s'",
                    entry.name);
                return false;
            }

            // Хеш набора реплицируемых полей локального типа (из своей схемы)
            buint32 localHash = 0;
            bool hashFound = false;
            const buint32 wireCount = this->schema.getWireTypeCount();
            for (buint32 lw = 0; lw < wireCount; ++lw)
            {
                const ReplicationSchema::WireType* wire = this->schema.getWireType(lw);
                if (wire != nullptr && std::strcmp(wire->name, entry.name) == 0)
                {
                    localHash = wire->schemaHash;
                    hashFound = true;
                    break;
                }
            }

            if (!hashFound || localHash != entry.schemaHash)
            {
                // Набор реплицируемых полей разошёлся — молчаливый
                // рассинхрон протокола недопустим
                __blib_log_error("ReplicationClientState: schema mismatch for type '%s' (local %u, peer %u)",
                    entry.name, localHash, entry.schemaHash);
                return false;
            }

            this->wireToLocal[w] = localType;
        }

        this->wireTypeCount = welcome.typeCount;
        this->serverTickRate = welcome.tickRate;
        outTickRate = welcome.tickRate;
        outPlayerEntity = welcome.playerEntityId;
        return true;
    }

    ReplicationClientState::InterpolationFieldState* ReplicationClientState::findSlot(
        EntityID entityId, buint8 wireTypeId, buint8 fieldIndex)
    {
        // Линейный перебор пула: зеркала MVP малы (единицы сущностей);
        // размер пула — протокольные капы, см. шапку
        for (buint32 i = 0; i < interpolationSlotPoolSize; ++i)
        {
            InterpolationFieldState& slot = this->slotPool[i];
            if (slot.entityId == entityId && slot.wireTypeId == wireTypeId &&
                slot.fieldIndex == fieldIndex)
            {
                return &slot;
            }
        }
        return nullptr;
    }

    const ReplicationClientState::InterpolationFieldState* ReplicationClientState::findSlot(
        EntityID entityId, buint8 wireTypeId, buint8 fieldIndex) const
    {
        for (buint32 i = 0; i < interpolationSlotPoolSize; ++i)
        {
            const InterpolationFieldState& slot = this->slotPool[i];
            if (slot.entityId == entityId && slot.wireTypeId == wireTypeId &&
                slot.fieldIndex == fieldIndex)
            {
                return &slot;
            }
        }
        return nullptr;
    }

    ReplicationClientState::InterpolationFieldState* ReplicationClientState::allocSlot(
        EntityID entityId, buint8 wireTypeId, buint8 fieldIndex)
    {
        if (this->freeSlotHead >= interpolationSlotPoolSize)
        {
            // Пул исчерпан (мир больше протокольных капов) — поле
            // применяется немедленно без интерполяции
            static bool warned = false;
            if (!warned)
            {
                __blib_log_warning("ReplicationClientState: interpolation slot pool exhausted");
                warned = true;
            }
            return nullptr;
        }

        const buint32 index = this->freeSlotHead;
        InterpolationFieldState& slot = this->slotPool[index];
        this->freeSlotHead = slot.nextFree;

        slot.entityId = entityId;
        slot.wireTypeId = wireTypeId;
        slot.fieldIndex = fieldIndex;
        slot.start = 0;
        slot.count = 0;
        slot.nextFree = 0;
        return &slot;
    }

    void ReplicationClientState::clearTypeSlots(EntityID entityId, buint8 wireTypeId)
    {
        // Full-ресинк компонента: кольца его полей переписываются —
        // старые сэмплы забываются, слоты остаются (переиспользуются)
        for (buint32 i = 0; i < interpolationSlotPoolSize; ++i)
        {
            InterpolationFieldState& slot = this->slotPool[i];
            if (slot.entityId == entityId && slot.wireTypeId == wireTypeId)
            {
                slot.start = 0;
                slot.count = 0;
            }
        }
    }

    void ReplicationClientState::freeEntitySlots(EntityID entityId)
    {
        for (buint32 i = 0; i < interpolationSlotPoolSize; ++i)
        {
            InterpolationFieldState& slot = this->slotPool[i];
            if (slot.entityId == entityId)
            {
                slot.entityId = invalidEntity;
                slot.nextFree = this->freeSlotHead;
                this->freeSlotHead = i;
            }
        }
    }

    void ReplicationClientState::pushSample(_In_Out InterpolationFieldState& slot,
        bfloat receiveTime, _In const FieldValue& value)
    {
        if (slot.count >= maxInterpolationSamples)
        {
            // Кольцо полное: старейший сэмпл вытесняется (кольцо
            // «плывёт» — start сдвигается)
            slot.start = (slot.start + 1) % maxInterpolationSamples;
            slot.count = maxInterpolationSamples - 1;
        }

        const buint32 writeIndex = (slot.start + slot.count) % maxInterpolationSamples;
        slot.samples[writeIndex].receiveTime = receiveTime;
        slot.samples[writeIndex].value = value;
        ++slot.count;
    }

    bool ReplicationClientState::applyEntityComponents(_In Scene& scene, EntityID entityId,
        _In const DecodedReplicationEntity& entity, bfloat receiveTime)
    {
        for (buint32 c = 0; c < entity.componentCount; ++c)
        {
            const DecodedReplicationComponent& component = entity.components[c];

            if (component.wireTypeId >= this->wireTypeCount)
            {
                return false;
            }

            const ComponentType localType = this->wireToLocal[component.wireTypeId];
            if (localType == invalidComponentType)
            {
                // Тип не резолвлен (welcome не принят) — протокол нарушен
                return false;
            }

            const buint32 schemaFieldCount = this->schema.getReplicatedFieldCount(localType);
            if (component.full && component.fieldCount != schemaFieldCount)
            {
                // Число полей полного состояния обязано совпадать со схемой
                return false;
            }

            // Компонент гарантируем (Transform уже создан сценой)
            if (!scene.hasComponent(entityId, localType) &&
                !scene.addComponent(entityId, localType))
            {
                return false;
            }

            IComponent* target = scene.tryGetComponent(entityId, localType);
            if (target == nullptr)
            {
                return false;
            }

            // Full-ресинк: кольца интерполяции типа переписываются
            if (component.full)
            {
                this->clearTypeSlots(entityId, component.wireTypeId);
            }

            // Применение полей через рефлексию (тип и kind сверяются).
            // Интерполируемые поля не пишутся в сцену сразу — уходят
            // в кольца сэмплов (renderMirror применяет их с задержкой)
            for (buint32 f = 0; f < component.fieldCount; ++f)
            {
                const ReplicationFieldPatch& patch = component.fields[f];
                const IComponentField* field = this->schema.getReplicatedField(localType, patch.fieldIndex);
                if (field == nullptr || field->getKind() != patch.value.kind)
                {
                    return false;
                }

                if (field->isInterpolated())
                {
                    InterpolationFieldState* slot = this->findSlot(
                        entityId, component.wireTypeId, patch.fieldIndex);
                    if (slot == nullptr)
                    {
                        slot = this->allocSlot(entityId, component.wireTypeId, patch.fieldIndex);
                    }
                    if (slot == nullptr)
                    {
                        // Пул исчерпан: деградация — применить сразу
                        field->setValue(*target, patch.value);
                        continue;
                    }
                    this->pushSample(*slot, receiveTime, patch.value);
                }
                else
                {
                    field->setValue(*target, patch.value);
                }
            }
        }
        return true;
    }

    bool ReplicationClientState::applySnapshot(_In Scene& scene,
        _In const DecodedReplicationSnapshot& snapshot, bfloat receiveTime)
    {
        for (buint32 e = 0; e < snapshot.entityCount; ++e)
        {
            const DecodedReplicationEntity& entity = snapshot.entities[e];

            if (entity.destroy)
            {
                // Сервер уничтожил сущность: зеркало знаем — удаляем,
                // не знаем — нечего удалять
                if (this->isKnown(entity.entityId))
                {
                    scene.destroyEntity(entity.entityId);
                    this->forgetEntity(entity.entityId);
                    this->freeEntitySlots(entity.entityId);

                    if (this->destroyEventCount < maxSnapshotEntities)
                    {
                        this->destroyEvents[this->destroyEventCount] = entity.entityId;
                        ++this->destroyEventCount;
                    }
                }
                continue;
            }

            EntityID entityId = entity.entityId;
            if (!this->isKnown(entityId))
            {
                // Спавн: зеркало с СЕРВЕРНЫМ id (инвариант: id монотонны)
                entityId = scene.createEntityWithId(entityId);
                if (entityId == invalidEntity)
                {
                    // Коллизия id — протокол нарушен; запись пропускаем
                    __blib_log_warning("ReplicationClientState: entity id %llu collides, spawn skipped",
                        static_cast<unsigned long long>(entity.entityId));
                    continue;
                }
                this->rememberEntity(entityId);

                if (this->spawnEventCount < maxSnapshotEntities)
                {
                    this->spawnEvents[this->spawnEventCount] = entityId;
                    ++this->spawnEventCount;
                }
            }

            if (!this->applyEntityComponents(scene, entityId, entity, receiveTime))
            {
                __blib_log_error("ReplicationClientState: failed to apply entity %llu (tick %u)",
                    static_cast<unsigned long long>(entityId), snapshot.tickNumber);
                return false;
            }
        }
        return true;
    }

    void ReplicationClientState::renderMirror(_In Scene& scene, bfloat nowSeconds)
    {
        if (this->serverTickRate == 0)
        {
            return; // Welcome не принят — интерполировать нечего
        }

        // Время рендера: постоянный лаг в тиках (см. шапку)
        const bfloat delaySeconds =
            static_cast<bfloat>(interpolationDelayTicks) / static_cast<bfloat>(this->serverTickRate);
        const bfloat renderTime = nowSeconds - delaySeconds;

        for (buint32 i = 0; i < interpolationSlotPoolSize; ++i)
        {
            const InterpolationFieldState& slot = this->slotPool[i];
            if (slot.entityId == invalidEntity || slot.count == 0)
            {
                continue;
            }

            const ComponentType localType = this->wireToLocal[slot.wireTypeId];
            if (localType == invalidComponentType)
            {
                continue;
            }

            IComponent* component = scene.tryGetComponent(slot.entityId, localType);
            const IComponentField* field = this->schema.getReplicatedField(localType, slot.fieldIndex);
            if (component == nullptr || field == nullptr)
            {
                continue;
            }

            const InterpolationSample& oldest = slot.samples[slot.start];
            const InterpolationSample& newest =
                slot.samples[(slot.start + slot.count - 1) % maxInterpolationSamples];

            if (slot.count == 1 || renderTime <= oldest.receiveTime)
            {
                // Буфер ещё не наполнился (или время рендера раньше
                // старейшего сэмпла) — держим старейший сэмпл
                field->setValue(*component, oldest.value);
                continue;
            }
            if (renderTime >= newest.receiveTime)
            {
                // Время рендера новее новейшего сэмпла (сервер молчит
                // или отстаёт) — держим новейший, без экстраполяции
                field->setValue(*component, newest.value);
                continue;
            }

            // Пара сэмплов, охватывающая время рендера
            buint32 a = slot.start;
            buint32 b = (a + 1) % maxInterpolationSamples;
            while (slot.samples[b].receiveTime < renderTime)
            {
                a = b;
                b = (b + 1) % maxInterpolationSamples;
            }

            const InterpolationSample& prev = slot.samples[a];
            const InterpolationSample& next = slot.samples[b];

            if (next.receiveTime <= prev.receiveTime)
            {
                // Вырожденный интервал (сэмплы одного времени) —
                // применяем новейший
                field->setValue(*component, next.value);
                continue;
            }

            const bfloat alpha = (renderTime - prev.receiveTime) / (next.receiveTime - prev.receiveTime);

            // MVP: lerp только Vector3 (позиция); иной kind с флагом
            // interpolated — применяется новейший сэмпл (см. шапку)
            if (prev.value.kind == FieldValue::Kind::Vector3 &&
                next.value.kind == FieldValue::Kind::Vector3)
            {
                FieldValue lerped = FieldValue::fromVector3(
                    prev.value.vector3Value + (next.value.vector3Value - prev.value.vector3Value) * alpha);
                field->setValue(*component, lerped);
            }
            else
            {
                field->setValue(*component, next.value);
            }
        }
    }

} // namespace beng
