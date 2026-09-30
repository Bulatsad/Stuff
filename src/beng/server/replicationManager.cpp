#include <beng/server/replicationManager.h>

#include <beng/core/scene.h>

#include <blib/core/console/console.h>
#include <blib/system/memory/globalAllocator.h>

namespace beng
{
    namespace server
    {
        namespace
        {
            /**
             * Точное сравнение значений полей (сервер сравнивает сцену
             * со СВОИМ зеркалом — оба значения из одного процесса,
             * поэтому сравнение точное, без эпсилонов).
             */
            bool fieldValuesEqual(_In const FieldValue& a, _In const FieldValue& b)
            {
                if (a.kind != b.kind)
                {
                    return false;
                }
                switch (a.kind)
                {
                    case FieldValue::Kind::Float:
                        return a.floatValue == b.floatValue;
                    case FieldValue::Kind::Int:
                        return a.intValue == b.intValue;
                    case FieldValue::Kind::Bool:
                        return a.boolValue == b.boolValue;
                    case FieldValue::Kind::Vector3:
                        return a.vector3Value.x == b.vector3Value.x
                            && a.vector3Value.y == b.vector3Value.y
                            && a.vector3Value.z == b.vector3Value.z;
                    case FieldValue::Kind::Entity:
                        return a.entityValue == b.entityValue;
                    default:
                        return false;
                }
            }
        }

        ReplicationManager::ReplicationManager()
            : schema(nullptr)
            , mirrors(nullptr)
        {
            // Зеркала клиентов — в куче через GlobalAllocator (суммарный
            // размер структур не умещается на стеке; проектное правило:
            // динамическая память — только через GlobalAllocator)
            this->mirrors = static_cast<ClientMirror*>(
                blib::memory::GlobalAllocator::instance().allocate(
                    maxClients * sizeof(ClientMirror)));
            for (buint32 i = 0; i < maxClients; ++i)
            {
                this->mirrors[i].active = false;
                this->mirrors[i].entityCount = 0;
                this->mirrors[i].pendingDestroyCount = 0;
            }
        }

        ReplicationManager::~ReplicationManager()
        {
            if (this->mirrors != nullptr)
            {
                blib::memory::GlobalAllocator::instance().deallocate(
                    this->mirrors, maxClients * sizeof(ClientMirror));
                this->mirrors = nullptr;
            }
        }

        void ReplicationManager::initialize(_In const ReplicationSchema& schema)
        {
            this->schema = &schema;
        }

        void ReplicationManager::onClientConnected(buint32 clientId)
        {
            if (clientId >= maxClients)
            {
                return;
            }
            ClientMirror& mirror = this->mirrors[clientId];
            mirror.active = true;
            mirror.entityCount = 0;
            mirror.pendingDestroyCount = 0;
        }

        void ReplicationManager::onClientDisconnected(buint32 clientId)
        {
            if (clientId >= maxClients)
            {
                return;
            }
            ClientMirror& mirror = this->mirrors[clientId];
            mirror.active = false;
            mirror.entityCount = 0;
            mirror.pendingDestroyCount = 0;
        }

        buint32 ReplicationManager::findMirrorEntity(_In const ClientMirror& mirror, EntityID id) const
        {
            // Линейный поиск: MVP-лимиты зеркал малы
            for (buint32 i = 0; i < mirror.entityCount; ++i)
            {
                if (mirror.entities[i].entityId == id)
                {
                    return i;
                }
            }
            return maxMirrorEntities;
        }

        bool ReplicationManager::fillFullPatch(_In const Scene& scene, EntityID entityId,
            _Out ReplicationEntityPatch& patch, _In_Out MirrorEntity& mirrorEntity)
        {
            patch.entityId = entityId;
            patch.flags = replicationEntityFull;
            patch.componentCount = 0;

            mirrorEntity.entityId = entityId;
            mirrorEntity.componentCount = 0;

            // Все реплицируемые компоненты сущности (порядок схемы)
            const buint32 wireTypeCount = this->schema->getWireTypeCount();
            for (buint32 w = 0; w < wireTypeCount; ++w)
            {
                const ComponentType localType = this->schema->getWireType(w)->localTypeId;
                if (!scene.hasComponent(entityId, localType))
                {
                    continue;
                }

                const IComponent* component = scene.tryGetComponent(entityId, localType);
                const buint32 fieldCount = this->schema->getReplicatedFieldCount(localType);
                if (component == nullptr || fieldCount == 0 ||
                    patch.componentCount >= maxSnapshotComponentsPerEntity)
                {
                    continue;
                }

                ReplicationComponentPatch& cp = patch.components[patch.componentCount];
                cp.localTypeId = localType;
                cp.fieldCount = static_cast<buint8>(fieldCount);
                for (buint32 f = 0; f < fieldCount; ++f)
                {
                    const IComponentField* field = this->schema->getReplicatedField(localType, f);
                    cp.fieldIndices[f] = static_cast<buint8>(f);
                    field->getValue(*component, cp.fieldValues[f]);
                }
                ++patch.componentCount;

                // Зеркало: компонент целиком
                MirrorComponent& mc = mirrorEntity.components[mirrorEntity.componentCount];
                mc.localTypeId = localType;
                mc.fieldCount = static_cast<buint8>(fieldCount);
                for (buint32 f = 0; f < fieldCount; ++f)
                {
                    mc.values[f] = cp.fieldValues[f];
                }
                ++mirrorEntity.componentCount;
            }

            return true;
        }

        bool ReplicationManager::fillDeltaPatch(_In const Scene& scene, EntityID entityId,
            _In_Out MirrorEntity& mirrorEntity, _Out ReplicationEntityPatch& patch)
        {
            patch.entityId = entityId;
            patch.flags = 0;
            patch.componentCount = 0;
            bool anyDirty = false;

            const buint32 wireTypeCount = this->schema->getWireTypeCount();
            for (buint32 w = 0; w < wireTypeCount; ++w)
            {
                const ComponentType localType = this->schema->getWireType(w)->localTypeId;
                if (!scene.hasComponent(entityId, localType))
                {
                    // Удаление компонентов не реплицируется (MVP) —
                    // см. шапку и SERVER.md
                    continue;
                }

                const IComponent* component = scene.tryGetComponent(entityId, localType);
                const buint32 fieldCount = this->schema->getReplicatedFieldCount(localType);
                if (component == nullptr || fieldCount == 0)
                {
                    continue;
                }

                // Зеркальный компонент (может отсутствовать — компонент
                // добавлен сущности после последнего снапшота)
                buint32 mirrorCompIndex = maxSnapshotComponentsPerEntity;
                for (buint32 m = 0; m < mirrorEntity.componentCount; ++m)
                {
                    if (mirrorEntity.components[m].localTypeId == localType)
                    {
                        mirrorCompIndex = m;
                        break;
                    }
                }

                ReplicationComponentPatch cp;
                cp.localTypeId = localType;
                cp.fieldCount = 0;

                if (mirrorCompIndex == maxSnapshotComponentsPerEntity)
                {
                    // Новый компонент: отправляем все его поля (в дельте —
                    // с явными индексами, формат это поддерживает)
                    for (buint32 f = 0; f < fieldCount; ++f)
                    {
                        const IComponentField* field = this->schema->getReplicatedField(localType, f);
                        cp.fieldIndices[f] = static_cast<buint8>(f);
                        field->getValue(*component, cp.fieldValues[f]);
                    }
                    cp.fieldCount = static_cast<buint8>(fieldCount);

                    // Зеркало: компонент добавляется
                    MirrorComponent& mc = mirrorEntity.components[mirrorEntity.componentCount];
                    mc.localTypeId = localType;
                    mc.fieldCount = static_cast<buint8>(fieldCount);
                    for (buint32 f = 0; f < fieldCount; ++f)
                    {
                        mc.values[f] = cp.fieldValues[f];
                    }
                    ++mirrorEntity.componentCount;
                }
                else
                {
                    // Дифф полей: отправить только изменившиеся
                    MirrorComponent& mc = mirrorEntity.components[mirrorCompIndex];
                    for (buint32 f = 0; f < fieldCount; ++f)
                    {
                        const IComponentField* field = this->schema->getReplicatedField(localType, f);
                        FieldValue current;
                        field->getValue(*component, current);

                        if (!fieldValuesEqual(current, mc.values[f]))
                        {
                            if (cp.fieldCount >= maxReplicatedFields)
                            {
                                break;
                            }
                            cp.fieldIndices[cp.fieldCount] = static_cast<buint8>(f);
                            cp.fieldValues[cp.fieldCount] = current;
                            ++cp.fieldCount;

                            // Зеркало обновляется тем, что отправлено
                            mc.values[f] = current;
                        }
                    }
                }

                if (cp.fieldCount > 0)
                {
                    if (patch.componentCount >= maxSnapshotComponentsPerEntity)
                    {
                        return false;
                    }
                    patch.components[patch.componentCount++] = cp;
                    anyDirty = true;
                }
            }

            return anyDirty;
        }

        bool ReplicationManager::buildSnapshot(buint32 clientId, _In const Scene& scene, buint32 tickNumber,
            bool forceFull, _Out ReplicationEntityPatch* outEntities, _Out buint32& outEntityCount)
        {
            (void)tickNumber;
            if (clientId >= maxClients || this->schema == nullptr)
            {
                return false;
            }

            ClientMirror& mirror = this->mirrors[clientId];
            if (!mirror.active)
            {
                return false;
            }

            buint32 patchCount = 0;

            // ===== 1. Удалённые на сервере сущности (в зеркале ещё есть) =====
            // Признак существования — Transform (инвариант сцены: он у всех)
            const ComponentType transformType = scene.getTransformTypeId();
            for (buint32 i = 0; i < mirror.entityCount; ++i)
            {
                const EntityID id = mirror.entities[i].entityId;
                if (!scene.hasComponent(id, transformType))
                {
                    // Сущность уничтожена: destroy-патч (если влезает)
                    if (mirror.pendingDestroyCount < maxMirrorEntities)
                    {
                        mirror.pendingDestroys[mirror.pendingDestroyCount++] = id;
                    }

                    // Зеркало: swap-and-pop
                    mirror.entities[i] = mirror.entities[mirror.entityCount - 1];
                    --mirror.entityCount;
                    --i;
                }
            }

            // ===== 2. Destroy-патчи =====
            for (buint32 d = 0; d < mirror.pendingDestroyCount; ++d)
            {
                if (patchCount >= maxSnapshotEntities)
                {
                    return false;
                }
                ReplicationEntityPatch& patch = outEntities[patchCount++];
                patch.entityId = mirror.pendingDestroys[d];
                patch.flags = replicationEntityDestroy;
                patch.componentCount = 0;
            }
            mirror.pendingDestroyCount = 0;

            // ===== 3. Патчи сущностей сцены =====
            if (scene.getEntityCount() > maxSnapshotEntities || scene.getEntityCount() > maxMirrorEntities)
            {
                return false;
            }

            for (buint32 dense = 0; dense < scene.getEntityCount(); ++dense)
            {
                if (patchCount >= maxSnapshotEntities)
                {
                    return false;
                }

                const EntityID id = scene.getEntityId(dense);
                const buint32 mirrorIndex = this->findMirrorEntity(mirror, id);

                if (mirrorIndex == maxMirrorEntities)
                {
                    // Неизвестная сущность → spawn (полный патч)
                    if (mirror.entityCount >= maxMirrorEntities)
                    {
                        return false;
                    }

                    MirrorEntity& mirrorEntity = mirror.entities[mirror.entityCount];
                    if (!this->fillFullPatch(scene, id, outEntities[patchCount], mirrorEntity))
                    {
                        return false;
                    }
                    ++mirror.entityCount;
                    ++patchCount;
                }
                else if (forceFull)
                {
                    // Принудительный ресинк (дроп очереди сети)
                    if (!this->fillFullPatch(scene, id, outEntities[patchCount], mirror.entities[mirrorIndex]))
                    {
                        return false;
                    }
                    ++patchCount;
                }
                else
                {
                    // Дельта: пустой патч (ничего не изменилось) не шлём
                    ReplicationEntityPatch patch;
                    if (this->fillDeltaPatch(scene, id, mirror.entities[mirrorIndex], patch))
                    {
                        outEntities[patchCount++] = patch;
                    }
                }
            }

            outEntityCount = patchCount;
            return true;
        }

    } // namespace server
} // namespace beng
