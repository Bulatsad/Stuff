#include <beng/core/scene.h>

#include <blib/system/memory/globalAllocator.h>

namespace beng
{
    Scene::Scene()
        : nextEntityId(1) // 0 зарезервирован для invalidEntity
        , systemsDirty(false)
    {
        entities.reserve(defaultEntityReserveSize);
        entityMasks.reserve(defaultEntityReserveSize);
        __blib_log_info("Scene created");
    }

    Scene::~Scene()
    {
        __blib_log_info("Scene destroying (%u entities, %u component types, %u systems)",
            static_cast<unsigned int>(entities.size()),
            static_cast<unsigned int>(ComponentTypeRegistry::getRegisteredCount()),
            static_cast<unsigned int>(systems.size()));

        clear();

        __blib_log_info("Scene destroyed");
    }

    EntityID Scene::createEntity()
    {
        // Защита от переполнения (через buint64Max не доживёт ни одна игра,
        // но дешевле проверить, чем получить UB от 0-переполнения)
        if (__blib_unlikely(nextEntityId == buint64Max))
        {
            __blib_fatal("Entity ID overflow in Scene");
        }

        EntityID id = nextEntityId++;

        // Добавить в плотные массивы (ID + пустая маска)
        entities.push_back(id);
        entityMasks.push_back(0);

        // Добавить в sparse lookup
        entityLookup[id] = static_cast<buint32>(entities.size() - 1);

        __blib_log_debug("Created Entity %llu (index %u)",
            static_cast<unsigned long long>(id),
            static_cast<unsigned int>(entities.size() - 1));

        return id;
    }

    void Scene::destroyEntity(EntityID id)
    {
        auto it = entityLookup.find(id);
        if (it == entityLookup.end())
        {
            __blib_log_warning("Entity %llu not found, cannot destroy",
                static_cast<unsigned long long>(id));
            return;
        }

        buint32 index = it->second;

        __blib_log_debug("Destroying Entity %llu (index %u)",
            static_cast<unsigned long long>(id), static_cast<unsigned int>(index));

        // Удалить все компоненты Entity из пулов.
        // Проходим по битам маски; для каждого установленного бита
        // вызываем type-erased destroyer конкретного пула.
        ComponentMask mask = entityMasks[index];
        for (ComponentType typeId = 0; mask != 0 && typeId < componentMaskBits; ++typeId)
        {
            ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
            if ((mask & bit) != 0)
            {
                void* poolPtr = componentPools[typeId];
                void (*destroyer)(void*, EntityID) = componentPoolDestroyers[typeId];
                if (poolPtr != nullptr && destroyer != nullptr)
                {
                    destroyer(poolPtr, id);
                }
                else
                {
                    // Маска утверждает что компонент есть, а пула нет —
                    // рассогласование состояния (не должно происходить)
                    __blib_log_warning("Entity %llu: component type %d has no pool destroyer",
                        static_cast<unsigned long long>(id), static_cast<int>(typeId));
                }
                mask &= ~bit;
            }
        }

        // Swap and pop из entities/entityMasks (для сохранения плотности)
        buint32 lastIndex = static_cast<buint32>(entities.size() - 1);
        if (index != lastIndex)
        {
            // Переместить последнюю Entity на место удалённой
            entities[index] = entities[lastIndex];
            entityMasks[index] = entityMasks[lastIndex];

            // Обновить lookup для перемещённой Entity.
            // ВАЖНО: operator[] может рехешировать map и инвалидировать
            // итераторы, поэтому удаление ниже — по ключу, а не по it.
            entityLookup[entities[index]] = index;
        }

        // Удалить последние элементы плотных массивов
        entities.pop_back();
        entityMasks.pop_back();

        // Удалить из lookup (по ключу — безопасно после возможного рехеша)
        entityLookup.erase(id);
    }

    void Scene::setComponentBit(EntityID entityId, ComponentType typeId, bool value)
    {
        if (__blib_unlikely(typeId >= componentMaskBits))
        {
            __blib_fatal("Component type %d exceeds mask width (%d bits)",
                static_cast<int>(typeId), static_cast<int>(componentMaskBits));
        }

        auto it = entityLookup.find(entityId);
        if (__blib_unlikely(it == entityLookup.end()))
        {
            __blib_fatal("Cannot update component mask of non-existent Entity %llu",
                static_cast<unsigned long long>(entityId));
        }

        ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
        if (value)
        {
            entityMasks[it->second] |= bit;
        }
        else
        {
            entityMasks[it->second] &= ~bit;
        }
    }

    bool Scene::getComponentBit(EntityID entityId, ComponentType typeId) const
    {
        if (typeId >= componentMaskBits)
        {
            return false;
        }

        auto it = entityLookup.find(entityId);
        if (it == entityLookup.end())
        {
            return false;
        }

        ComponentMask bit = static_cast<ComponentMask>(1) << typeId;
        return (entityMasks[it->second] & bit) != 0;
    }

    void Scene::addSystem(_In ISystem* system)
    {
        if (system == nullptr)
        {
            __blib_log_warning("Cannot add null system to Scene");
            return;
        }

        // Проверка что система ещё не добавлена
        for (ISystem* existing : systems)
        {
            if (existing == system)
            {
                __blib_log_warning("System %s already added to Scene", system->getName());
                return;
            }
        }

        systems.push_back(system);
        systemsDirty = true; // требуется пересортировка

        __blib_log_info("Added system: %s (priority %d)",
            system->getName(), static_cast<int>(system->getPriority()));
    }

    void Scene::removeSystem(_In ISystem* system)
    {
        if (system == nullptr)
        {
            __blib_log_warning("Cannot remove null system from Scene");
            return;
        }

        auto it = std::find(systems.begin(), systems.end(), system);
        if (it == systems.end())
        {
            __blib_log_warning("System %s not found in Scene", system->getName());
            return;
        }

        systems.erase(it);

        __blib_log_info("Removed system: %s", system->getName());
    }

    void Scene::update(float deltaTime)
    {
        // Пересортировать системы если нужно
        if (systemsDirty)
        {
            sortSystems();
            systemsDirty = false;
        }

        // Вызвать update для каждой системы
        for (ISystem* system : systems)
        {
            system->update(*this, deltaTime);
        }
    }

    void Scene::sortSystems()
    {
        std::sort(systems.begin(), systems.end(), [](ISystem* a, ISystem* b) {
            return a->getPriority() < b->getPriority();
        });

        __blib_log_debug("Systems sorted by priority:");
        for (ISystem* system : systems)
        {
            __blib_log_debug("  - %s (priority %d)",
                system->getName(), static_cast<int>(system->getPriority()));
        }
    }

    void Scene::clear()
    {
        // Удалить Entity (их компоненты удаляются ниже вместе с пулами,
        // но на случай ручного вызова clear в будущем — зачищаем всё)
        entities.clear();
        entityMasks.clear();
        entityLookup.clear();

        // Удалить все component pools (через type-erased deleter функции)
        for (ComponentType typeId = 0; typeId < maxComponentTypes; ++typeId)
        {
            void* poolPtr = componentPools[typeId];
            void (*deleter)(void*) = componentPoolDeleters[typeId];
            if (poolPtr != nullptr && deleter != nullptr)
            {
                deleter(poolPtr);
            }
            componentPools[typeId] = nullptr;
            componentPoolDeleters[typeId] = nullptr;
            componentPoolDestroyers[typeId] = nullptr;
        }

        // Системы не удаляем — Scene не владеет ими
        systems.clear();

        nextEntityId = 1;
        systemsDirty = false;
    }

} // namespace beng
