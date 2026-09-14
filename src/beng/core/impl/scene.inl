#pragma once

// Template реализация методов Scene (включается из scene.h)

#include <blib/system/memory/globalAllocator.h>
#include <blib/core/console/console.h>

namespace beng
{
    template<typename T>
    bool Scene::hasComponent(EntityID entityId) const
    {
        ComponentType typeId = ComponentTypeRegistry::getTypeId<T>();
        return getComponentBit(entityId, typeId);
    }

    template<typename T, typename... Args>
    T& Scene::addComponent(EntityID entityId, Args&&... args)
    {
        // Проверка что Entity существует
        if (__blib_unlikely(entityLookup.find(entityId) == entityLookup.end()))
        {
            __blib_fatal("Cannot add component to non-existent Entity %llu",
                static_cast<unsigned long long>(entityId));
        }

        ComponentType typeId = ComponentTypeRegistry::getTypeId<T>();

        // Проверка что компонент ещё не существует
        if (__blib_unlikely(getComponentBit(entityId, typeId)))
        {
            __blib_fatal("Component type %d already exists on Entity %llu",
                static_cast<int>(typeId), static_cast<unsigned long long>(entityId));
        }

        // Получить пул компонентов
        ComponentPool<T>& pool = getComponentPool<T>();

        // Создать компонент в пуле
        T* component = pool.create(entityId, std::forward<Args>(args)...);

        if (__blib_unlikely(component == nullptr))
        {
            __blib_fatal("Failed to create component type %d for Entity %llu",
                static_cast<int>(typeId), static_cast<unsigned long long>(entityId));
        }

        // Установить бит в маске
        setComponentBit(entityId, typeId, true);

        return *component;
    }

    template<typename T>
    T& Scene::getComponent(EntityID entityId)
    {
        T* component = tryGetComponent<T>(entityId);
        if (__blib_unlikely(component == nullptr))
        {
            __blib_fatal("Component type %d not found on Entity %llu",
                static_cast<int>(ComponentTypeRegistry::getTypeId<T>()),
                static_cast<unsigned long long>(entityId));
        }
        return *component;
    }

    template<typename T>
    const T& Scene::getComponent(EntityID entityId) const
    {
        const T* component = tryGetComponent<T>(entityId);
        if (__blib_unlikely(component == nullptr))
        {
            __blib_fatal("Component type %d not found on Entity %llu",
                static_cast<int>(ComponentTypeRegistry::getTypeId<T>()),
                static_cast<unsigned long long>(entityId));
        }
        return *component;
    }

    template<typename T>
    T* Scene::tryGetComponent(EntityID entityId)
    {
        // Проверить что Entity существует
        if (entityLookup.find(entityId) == entityLookup.end())
        {
            return nullptr;
        }

        ComponentType typeId = ComponentTypeRegistry::getTypeId<T>();

        // Проверить бит маски (дешёвая проверка до обращения к пулу)
        if (!getComponentBit(entityId, typeId))
        {
            return nullptr;
        }

        // Получить из пула
        ComponentPool<T>& pool = getComponentPool<T>();
        return pool.get(entityId);
    }

    template<typename T>
    const T* Scene::tryGetComponent(EntityID entityId) const
    {
        // Проверить что Entity существует
        if (entityLookup.find(entityId) == entityLookup.end())
        {
            return nullptr;
        }

        ComponentType typeId = ComponentTypeRegistry::getTypeId<T>();

        // Проверить бит маски (дешёвая проверка до обращения к пулу)
        if (!getComponentBit(entityId, typeId))
        {
            return nullptr;
        }

        // Получить из пула
        const ComponentPool<T>& pool = getComponentPool<T>();
        return pool.get(entityId);
    }

    template<typename T, typename... Args>
    T& Scene::resolveComponent(EntityID entityId, Args&&... args)
    {
        // Компонент уже существует — вернуть его
        if (hasComponent<T>(entityId))
        {
            T* component = tryGetComponent<T>(entityId);
            if (__blib_unlikely(component == nullptr))
            {
                // Несоответствие маски и пула — fatal error
                __blib_fatal("Component mask inconsistency for Entity %llu type %d",
                    static_cast<unsigned long long>(entityId),
                    static_cast<int>(ComponentTypeRegistry::getTypeId<T>()));
            }
            return *component;
        }

        // Компонента нет — создать новый
        return addComponent<T>(entityId, std::forward<Args>(args)...);
    }

    template<typename T>
    void Scene::removeComponent(EntityID entityId)
    {
        // Проверить существует ли Entity
        if (entityLookup.find(entityId) == entityLookup.end())
        {
            __blib_log_warning("Cannot remove component from non-existent Entity %llu",
                static_cast<unsigned long long>(entityId));
            return;
        }

        ComponentType typeId = ComponentTypeRegistry::getTypeId<T>();

        // Компонента нет — no-op
        if (!getComponentBit(entityId, typeId))
        {
            return;
        }

        // Удалить компонент из пула
        ComponentPool<T>& pool = getComponentPool<T>();
        pool.destroy(entityId);

        // Сбросить бит в маске
        setComponentBit(entityId, typeId, false);
    }

    template<typename T>
    void Scene::registerComponentType(buint32 chunkSize)
    {
        ComponentType typeId = ComponentTypeRegistry::getTypeId<T>();

        // Проверка что тип ещё не зарегистрирован
        if (componentPools[typeId] != nullptr)
        {
            __blib_log_warning("Component type %d already registered in Scene",
                static_cast<int>(typeId));
            return;
        }

        // Выделить память для ComponentPool<T> через GlobalAllocator
        void* mem = blib::memory::GlobalAllocator::instance().allocate(sizeof(ComponentPool<T>));
        if (__blib_unlikely(mem == nullptr))
        {
            __blib_fatal("Failed to allocate ComponentPool for type %d", static_cast<int>(typeId));
        }

        // Placement new — создать ComponentPool<T>
        ComponentPool<T>* pool = new (mem) ComponentPool<T>(chunkSize);

        // Сохранить в массив пулов
        componentPools[typeId] = pool;

        // Сохранить deleter функцию для type-erased удаления в clear()
        componentPoolDeleters[typeId] = [](void* ptr) {
            ComponentPool<T>* p = static_cast<ComponentPool<T>*>(ptr);
            p->~ComponentPool<T>();
            blib::memory::GlobalAllocator::instance().deallocate(p, sizeof(ComponentPool<T>));
        };

        // Сохранить destroyer функцию для удаления компонента по EntityID
        // (используется в Scene::destroyEntity)
        componentPoolDestroyers[typeId] = [](void* ptr, EntityID entityId) {
            static_cast<ComponentPool<T>*>(ptr)->destroy(entityId);
        };

        __blib_log_info("Registered component pool for type %d (chunk size: %u)",
            static_cast<int>(typeId), static_cast<unsigned int>(chunkSize));
    }

    template<typename T>
    ComponentPool<T>& Scene::getComponentPool()
    {
        ComponentType typeId = ComponentTypeRegistry::getTypeId<T>();

        if (__blib_unlikely(componentPools[typeId] == nullptr))
        {
            __blib_fatal("Component type %d not registered in Scene (call registerComponentType first)",
                static_cast<int>(typeId));
        }

        return *static_cast<ComponentPool<T>*>(componentPools[typeId]);
    }

    template<typename T>
    const ComponentPool<T>& Scene::getComponentPool() const
    {
        ComponentType typeId = ComponentTypeRegistry::getTypeId<T>();

        if (__blib_unlikely(componentPools[typeId] == nullptr))
        {
            __blib_fatal("Component type %d not registered in Scene (call registerComponentType first)",
                static_cast<int>(typeId));
        }

        return *static_cast<const ComponentPool<T>*>(componentPools[typeId]);
    }

    template<typename T>
    ComponentPool<T>* Scene::tryGetComponentPool()
    {
        ComponentType typeId = ComponentTypeRegistry::getTypeId<T>();
        return static_cast<ComponentPool<T>*>(componentPools[typeId]);
    }

} // namespace beng
