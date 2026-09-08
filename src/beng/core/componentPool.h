#pragma once

#include <beng/config.h>
#include <beng/core/component.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/allocators/poolAllocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>
#include <blib/core/console/console.h>

#include <vector>
#include <unordered_map>
#include <utility>

namespace beng
{
    /**
     * ComponentPool<T> - пул компонентов одного типа.
     * 
     * Назначение:
     * - Эффективное хранение компонентов одного типа
     * - Быстрый доступ по EntityID через sparse set
     * - Cache-friendly итерация через плотный массив
     * - Компоненты аллоцируются через blib::memory::PoolAllocator
     * - Служебные контейнеры (dense/sparse) — через GlobalAllocator
     *   (StdAllocatorAdapter), без дефолтного ::operator new
     * 
     * Архитектура (Sparse Set):
     * 
     *   sparse: EntityID → dense index
     *   ┌─────┬─────┬─────┬─────┐
     *   │  3  │ INV │  0  │  1  │  sparse[entityId] = dense index
     *   └─────┴─────┴─────┴─────┘
     *      ↓           ↓     ↓
     *   dense: array of {Component*, EntityID}
     *   ┌──────────────┬──────────────┬──────────────┐
     *   │ Comp*, ID=2  │ Comp*, ID=3  │ Comp*, ID=0  │  плотный массив
     *   └──────────────┴──────────────┴──────────────┘
     * 
     * Преимущества:
     * - O(1) create/get/destroy
     * - Компоненты лежат плотно в памяти → cache-friendly итерация
     * - Указатель на компонент стабилен, пока компонент не удалён
     *   (destroy другого компонента не двигает T, только Entry)
     * 
     * Использование:
     *   ComponentPool<TransformComponent> pool(128); // 128 компонентов на chunk
     *   
     *   T* comp = pool.create(entityId, ...args);
     *   T* comp = pool.get(entityId);  // nullptr если нет
     *   pool.destroy(entityId);
     * 
     * Ограничения:
     * - Не thread-safe (требуется внешняя синхронизация)
     * - Некопируем и неперемещаем: контейнеры держат указатель на
     *   собственный containerAllocator (StdAllocatorAdapter), переезд
     *   объекта оставил бы висячие ссылки
     */
    template<typename T>
    class ComponentPool
    {
        static_assert(std::is_base_of<IComponent, T>::value,
            "ComponentPool<T> requires T derived from beng::IComponent");

    public:
        /**
         * Конструктор пула компонентов.
         * 
         * @param chunkSize Количество компонентов в одном chunk PoolAllocator
         */
        explicit ComponentPool(buint32 chunkSize = defaultComponentPoolChunkSize)
            : allocator(sizeof(T), chunkSize)
        {
            dense.reserve(chunkSize);
        }

        ~ComponentPool()
        {
            // Удалить все компоненты (вызвать деструкторы)
            for (auto& entry : dense)
            {
                if (entry.component != nullptr)
                {
                    entry.component->~T();
                    allocator.deallocate(entry.component, sizeof(T));
                }
            }
        }

        // Запрет копирования и перемещения (см. комментарий к классу)
        ComponentPool(const ComponentPool&) = delete;
        ComponentPool& operator=(const ComponentPool&) = delete;
        ComponentPool(ComponentPool&&) = delete;
        ComponentPool& operator=(ComponentPool&&) = delete;

        /**
         * Создать компонент для Entity.
         * 
         * @param entityId ID Entity
         * @param args Аргументы конструктора компонента
         * @return Указатель на созданный компонент или nullptr при ошибке
         * 
         * Проверки:
         * - Если компонент уже существует → возвращает существующий (warning в лог)
         * 
         * Сразу после конструирования компоненту выставляется
         * ownerId = entityId (IComponent::setOwnerId).
         */
        template<typename... Args>
        T* create(EntityID entityId, Args&&... args)
        {
            // Проверка что компонент ещё не существует
            auto it = sparse.find(entityId);
            if (__blib_unlikely(it != sparse.end()))
            {
                __blib_log_warning("Component already exists for entity %llu, returning existing",
                    static_cast<unsigned long long>(entityId));
                return dense[it->second].component;
            }

            // Выделить память через PoolAllocator
            void* mem = allocator.allocate(sizeof(T));
            if (__blib_unlikely(mem == nullptr))
            {
                __blib_log_error("Failed to allocate component for entity %llu",
                    static_cast<unsigned long long>(entityId));
                return nullptr;
            }

            // Placement new — вызвать конструктор
            T* component = new (mem) T(std::forward<Args>(args)...);

            // Привязать компонент к владельцу
            component->setOwnerId(entityId);

            // Добавить в dense array
            buint32 denseIndex = static_cast<buint32>(dense.size());
            dense.push_back({ component, entityId });

            // Добавить в sparse map
            sparse[entityId] = denseIndex;

            return component;
        }

        /**
         * Получить компонент Entity.
         * 
         * @param entityId ID Entity
         * @return Указатель на компонент или nullptr если не существует
         */
        T* get(EntityID entityId)
        {
            auto it = sparse.find(entityId);
            if (it == sparse.end())
            {
                return nullptr;
            }
            return dense[it->second].component;
        }

        const T* get(EntityID entityId) const
        {
            auto it = sparse.find(entityId);
            if (it == sparse.end())
            {
                return nullptr;
            }
            return dense[it->second].component;
        }

        /**
         * Удалить компонент Entity.
         * 
         * @param entityId ID Entity
         * 
         * Алгоритм (swap and pop):
         * 1. Найти индекс в dense через sparse
         * 2. Вызвать деструктор и освободить память
         * 3. Swap с последним элементом в dense
         * 4. Pop последнего элемента
         * 5. Обновить sparse для swap'нутого элемента
         * 
         * Поведение:
         * - Если компонент не существует → no-op (не ошибка)
         */
        void destroy(EntityID entityId)
        {
            auto it = sparse.find(entityId);
            if (it == sparse.end())
            {
                return; // компонент не существует — no-op
            }

            buint32 denseIndex = it->second;
            T* component = dense[denseIndex].component;

            // Вызвать деструктор
            component->~T();

            // Освободить память через PoolAllocator
            allocator.deallocate(component, sizeof(T));

            // Swap and pop из dense (для сохранения плотности)
            buint32 lastIndex = static_cast<buint32>(dense.size() - 1);
            if (denseIndex != lastIndex)
            {
                // Переместить последний элемент на место удалённого
                dense[denseIndex] = dense[lastIndex];

                // Обновить sparse для перемещённого Entity
                sparse[dense[denseIndex].entityId] = denseIndex;
            }

            // Удалить последний элемент
            dense.pop_back();

            // Удалить из sparse
            sparse.erase(it);
        }

        /**
         * Получить количество компонентов в пуле.
         * 
         * @return Количество активных компонентов
         */
        buint32 size() const
        {
            return static_cast<buint32>(dense.size());
        }

        /**
         * Проверить пуст ли пул.
         * 
         * @return true если нет компонентов
         */
        bool empty() const
        {
            return dense.empty();
        }

        /**
         * Получить EntityID по индексу в dense array.
         * 
         * @param index Индекс в dense array
         * @return EntityID компонента
         * 
         * Использование (для систем):
         *   for (buint32 i = 0; i < pool.size(); ++i) {
         *       EntityID id = pool.getEntityId(i);
         *       T* component = pool.getByIndex(i);
         *   }
         */
        EntityID getEntityId(buint32 index) const
        {
            return dense[index].entityId;
        }

        /**
         * Получить компонент по индексу в dense array.
         * 
         * @param index Индекс в dense array
         * @return Указатель на компонент
         */
        T* getByIndex(buint32 index)
        {
            return dense[index].component;
        }

        const T* getByIndex(buint32 index) const
        {
            return dense[index].component;
        }

    private:
        // Entry в dense array — компонент + EntityID владельца
        struct Entry
        {
            T* component;       // Указатель на компонент
            EntityID entityId;  // ID Entity владельца
        };

        // Адаптер STL-контейнеров к blib-аллокатору (см. StdAllocatorAdapter)
        template<typename U>
        using ContainerAllocator = blib::memory::StdAllocatorAdapter<U>;

        // Аллокатор служебных контейнеров (dense/sparse).
        // Объявлен ПЕРЕД контейнерами: они хранят указатель на него.
        // По умолчанию — DefaultAllocator (прокси к GlobalAllocator).
        blib::memory::Allocator containerAllocator;

        // PoolAllocator для выделения памяти под компоненты
        blib::memory::PoolAllocator allocator;

        // Плотный массив компонентов (для cache-friendly итерации)
        std::vector<Entry, ContainerAllocator<Entry>> dense{
            ContainerAllocator<Entry>(&containerAllocator) };

        // Разреженный map для быстрого доступа EntityID → dense index
        std::unordered_map<EntityID, buint32,
            std::hash<EntityID>, std::equal_to<EntityID>,
            ContainerAllocator<std::pair<const EntityID, buint32>>> sparse{
                ContainerAllocator<std::pair<const EntityID, buint32>>(&containerAllocator) };
    };

} // namespace beng
