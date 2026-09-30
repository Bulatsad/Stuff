#include <beng/core/replicationClientState.h>

#include <beng/core/scene.h>

#include <blib/core/console/console.h>

#include <cstring>

namespace beng
{
    ReplicationClientState::ReplicationClientState()
        : wireTypeCount(0)
        , knownEntities(blib::memory::StdAllocatorAdapter<EntityID>(&this->containerAllocator))
    {
        for (buint32 w = 0; w < maxWireTypes; ++w)
        {
            this->wireToLocal[w] = invalidComponentType;
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
        outTickRate = welcome.tickRate;
        outPlayerEntity = welcome.playerEntityId;
        return true;
    }

    bool ReplicationClientState::applyEntityComponents(_In Scene& scene, EntityID entityId,
        _In const DecodedReplicationEntity& entity)
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

            // Применение полей через рефлексию (тип и kind сверяются)
            for (buint32 f = 0; f < component.fieldCount; ++f)
            {
                const ReplicationFieldPatch& patch = component.fields[f];
                const IComponentField* field = this->schema.getReplicatedField(localType, patch.fieldIndex);
                if (field == nullptr || field->getKind() != patch.value.kind)
                {
                    return false;
                }
                field->setValue(*target, patch.value);
            }
        }
        return true;
    }

    bool ReplicationClientState::applySnapshot(_In Scene& scene, _In const DecodedReplicationSnapshot& snapshot)
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
            }

            if (!this->applyEntityComponents(scene, entityId, entity))
            {
                __blib_log_error("ReplicationClientState: failed to apply entity %llu (tick %u)",
                    static_cast<unsigned long long>(entityId), snapshot.tickNumber);
                return false;
            }
        }
        return true;
    }

} // namespace beng
