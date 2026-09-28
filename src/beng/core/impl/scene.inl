#pragma once

// Template реализация методов Scene (включается из scene.h)

#include <blib/system/memory/globalAllocator.h>
#include <blib/core/console/console.h>

#include <type_traits>

namespace beng
{
    template<typename T>
    bool Scene::hasComponent(EntityID entityId) const
    {
        ComponentType typeId = getTypeId<T>();
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

        ComponentType typeId = getTypeId<T>();

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
                static_cast<int>(getTypeId<T>()),
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
                static_cast<int>(getTypeId<T>()),
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

        // Не-fatal резолв: тип может быть не зарегистрирован в сцене
        ComponentType typeId = findTypeId<T>();
        if (typeId == invalidComponentType)
        {
            return nullptr;
        }

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

        // Не-fatal резолв: тип может быть не зарегистрирован в сцене
        ComponentType typeId = findTypeId<T>();
        if (typeId == invalidComponentType)
        {
            return nullptr;
        }

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
                    static_cast<int>(getTypeId<T>()));
            }
            return *component;
        }

        // Компонента нет — создать новый
        return addComponent<T>(entityId, std::forward<Args>(args)...);
    }

    template<typename T>
    void Scene::removeComponent(EntityID entityId)
    {
        // ИНВАРИАНТ: TransformComponent с сущности снять нельзя —
        // сущность без Transform существовать не может (создаётся
        // автоматически в createEntity, см. scene.cpp)
        if constexpr (std::is_same<T, TransformComponent>::value)
        {
            __blib_fatal("TransformComponent cannot be removed from Entity %llu "
                "(scene invariant: every entity must have a Transform)",
                static_cast<unsigned long long>(entityId));
        }

        // Проверить существует ли Entity
        if (entityLookup.find(entityId) == entityLookup.end())
        {
            __blib_log_warning("Cannot remove component from non-existent Entity %llu",
                static_cast<unsigned long long>(entityId));
            return;
        }

        ComponentType typeId = getTypeId<T>();

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
        static_assert(std::is_base_of<IComponent, T>::value,
            "Component type T must inherit from beng::IComponent");
        static_assert(HasComponentTypeName<T>::value,
            "Component must declare 'static constexpr const char* componentTypeName' "
            "(stable type name used for registration)");

        const char* name = T::componentTypeName;

        // Проверка коллизии имени через словарь типов сцены.
        // Ключ строится с аллокатором сцены: StdAllocatorAdapter без
        // дефолтного конструктора, неявная конверсия невозможна.
        ComponentTypeNameString key(name, blib::memory::StdAllocatorAdapter<char>(&containerAllocator));
        if (__blib_unlikely(typeIdByName.count(key) != 0))
        {
            __blib_fatal("Component type '%s' already registered in Scene "
                "(guard with isRegisteredComponentType<T>() before registering)",
                name);
        }

        // Проверка лимита типов на сцену (ширина ComponentMask)
        if (__blib_unlikely(typeNames.size() >= maxComponentTypes))
        {
            __blib_fatal("Component type limit exceeded for Scene (max %d types)",
                static_cast<int>(maxComponentTypes));
        }

        // Локальный ID типа: индекс в таблицах сцены, бит в ComponentMask
        ComponentType typeId = static_cast<ComponentType>(typeNames.size());

        // Выделить память для ComponentPool<T> через GlobalAllocator
        void* mem = blib::memory::GlobalAllocator::instance().allocate(sizeof(ComponentPool<T>));
        if (__blib_unlikely(mem == nullptr))
        {
            __blib_fatal("Failed to allocate ComponentPool for type '%s'", name);
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

        // Фабрика пула: Scene::load создаёт пул типа по требованию
        // (тип пришёл из файла), Scene::verify — копирует реестр.
        // Лямбды не захватывают состояние — их можно копировать между
        // сценами (copyComponentTypeRegistryFrom)
        componentPoolFactories[typeId] = [](buint32 poolChunkSize) -> void* {
            void* poolMem = blib::memory::GlobalAllocator::instance().allocate(sizeof(ComponentPool<T>));
            if (poolMem == nullptr)
            {
                return nullptr;
            }
            return new (poolMem) ComponentPool<T>(poolChunkSize);
        };
        componentPoolChunkSizes[typeId] = chunkSize;

        // Создание компонента (default-ctor + ownerId) — Scene::load.
        // Только для default-конструируемых типов: без default-ctor
        // компонент невозможно восстановить из файла (creator = nullptr,
        // Scene::load вернёт InvalidData для такого типа)
        if constexpr (std::is_default_constructible<T>::value)
        {
            componentPoolCreators[typeId] = [](void* ptr, EntityID entityId) -> IComponent* {
                return static_cast<ComponentPool<T>*>(ptr)->create(entityId);
            };
        }
        else
        {
            componentPoolCreators[typeId] = nullptr;
        }

        // Получение компонента — Scene::save/strongCompare/onLoaded
        componentPoolGetters[typeId] = [](void* ptr, EntityID entityId) -> IComponent* {
            return static_cast<ComponentPool<T>*>(ptr)->get(entityId);
        };

        // Записать имя в таблицу типов (горячий путь резолва) и в
        // словарь (проверка регистрации + резолв имён в Scene::load)
        typeNames.push_back(name);
        typeIdByName.emplace(std::move(key), typeId);

        __blib_log_info("Registered component type '%s' in Scene (typeId %d, chunk size: %u)",
            name, static_cast<int>(typeId), static_cast<unsigned int>(chunkSize));
    }

    template<typename T>
    bool Scene::isRegisteredComponentType() const
    {
        static_assert(HasComponentTypeName<T>::value,
            "Component must declare 'static constexpr const char* componentTypeName' "
            "(stable type name used for registration)");

        return findTypeId<T>() != invalidComponentType;
    }

    template<typename T>
    ComponentType Scene::findTypeId() const
    {
        static_assert(HasComponentTypeName<T>::value,
            "Component must declare 'static constexpr const char* componentTypeName' "
            "(stable type name used for registration)");

        const char* name = T::componentTypeName;
        const ComponentType count = static_cast<ComponentType>(typeNames.size());
        for (ComponentType id = 0; id < count; ++id)
        {
            if (std::strcmp(typeNames[id], name) == 0)
            {
                return id;
            }
        }
        return invalidComponentType;
    }

    template<typename T>
    ComponentType Scene::getTypeId() const
    {
        const ComponentType typeId = findTypeId<T>();
        if (__blib_unlikely(typeId == invalidComponentType))
        {
            __blib_fatal("Component type '%s' not registered in Scene "
                "(call scene.registerComponentType<T>() first)",
                T::componentTypeName);
        }
        return typeId;
    }

    template<typename T>
    ComponentPool<T>& Scene::getComponentPool()
    {
        ComponentType typeId = getTypeId<T>();

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
        ComponentType typeId = getTypeId<T>();

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
        // Не-fatal резолв: nullptr если тип не зарегистрирован в сцене
        // (RenderSystem и другие системы с опциональными компонентами)
        ComponentType typeId = findTypeId<T>();
        if (typeId == invalidComponentType)
        {
            return nullptr;
        }
        return static_cast<ComponentPool<T>*>(componentPools[typeId]);
    }

} // namespace beng
