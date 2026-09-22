#pragma once

#include <beng/config.h>
#include <beng/core/icomponent.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/allocators/poolAllocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>
#include <blib/core/console/console.h>

#include <vector>
#include <unordered_map>
#include <utility>
#include <iterator>
#include <type_traits>

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
     * 
     * Итерация:
     * - begin()/end() (+ const/cbegin/cend) — итерация по АКТИВНЫМ
     *   компонентам (IComponent::isActive == true); неактивные
     *   пропускаются (флаг читается вживую на каждом шаге)
     * - Порядок: dense-массив (сплошной скан Entry, как у getByIndex)
     * - operator* → T& (const T& для const-пула); EntityID владельца —
     *   через метод getEntityId() итератора
     * - Инвалидация: create() (push_back в dense) и destroy()
     *   (swap-and-pop) — стандартная контейнерная семантика,
     *   end() пересчитывать заново; инкремент на end() — UB
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
         * IteratorBase<PoolT> - forward-итератор по активным компонентам пула.
         * 
         * PoolT = ComponentPool<T>        → Iterator (возвращает T&)
         * PoolT = const ComponentPool<T>  → ConstIterator (возвращает const T&)
         * 
         * Семантика:
         * - Итерирует ТОЛЬКО активные компоненты (IComponent::isActive == true),
         *   неактивные пропускаются (проверка флага O(1) на шаг)
         * - Порядок: dense-массив, как у индексных циклов getByIndex
         * - Пустой пул или пул без активных компонентов: begin() == end()
         * - operator* → T&; EntityID владельца — метод getEntityId()
         * 
         * Инвалидация (как у контейнеров):
         * - create() (push_back в dense) и destroy() (swap-and-pop)
         *   инвалидируют живые итераторы; end() пересчитывать заново
         * - Деструктор пула оставляет итераторы висячими
         * - Инкремент на end() - UB (стандартная конвенция итераторов)
         * 
         * isActive читается «вживую» на каждом шаге: переключение флага
         * во время итерации немедленно влияет на обход (не кешируется).
         */
        template<typename PoolT>
        class IteratorBase
        {
        public:
            // std-совместимые typedef'ы (по образцу PoolAllocator::IteratorBase).
            // value_type = T: итератор выдаёт сам компонент (ссылку), а не
            // указатель; владелец доступен через getEntityId().
            typedef std::forward_iterator_tag iterator_category;
            typedef T value_type;
            typedef std::ptrdiff_t difference_type;
            typedef std::conditional_t<std::is_const_v<PoolT>, const T*, T*> pointer;
            typedef std::conditional_t<std::is_const_v<PoolT>, const T&, T&> reference;

            enum class IteratorOrigin {
                Begin,
                End
            };

            IteratorBase(_In PoolT* a_pool, IteratorOrigin itOrigin)
                : pool(a_pool)
                , index(0)
            {
                if (itOrigin == IteratorOrigin::Begin)
                {
                    // Нормализация: первый компонент может быть неактивным —
                    // сразу продвигаемся к первому активному (пул без
                    // активных компонентов даёт end()).
                    this->skipInactive();
                }
                else if (itOrigin == IteratorOrigin::End)
                {
                    this->index = static_cast<buint32>(this->pool->dense.size());
                }
            }

            reference operator*() const
            {
                return static_cast<reference>(*this->pool->dense[this->index].component);
            }

            pointer operator->() const
            {
                return this->pool->dense[this->index].component;
            }

            /**
             * EntityID владельца текущего компонента.
             * Аналог pool.getEntityId(index) для индексных циклов.
             */
            EntityID getEntityId() const
            {
                return this->pool->dense[this->index].entityId;
            }

            IteratorBase& operator++()
            {
                ++this->index;
                this->skipInactive();
                return *this;
            }

            IteratorBase operator++(int)
            {
                IteratorBase tmp(*this);
                ++(*this);
                return tmp;
            }

            bool operator==(const IteratorBase& rhs) const
            {
                return (this->pool == rhs.pool) && (this->index == rhs.index);
            }

            bool operator!=(const IteratorBase& rhs) const
            {
                return !(*this == rhs);
            }

        private:
            PoolT* pool = nullptr;
            buint32 index = 0;

            // Позиция end: index указывает за последний элемент dense.
            bool isAtEnd() const
            {
                return this->index == static_cast<buint32>(this->pool->dense.size());
            }

            // Пропуск неактивных компонентов. Проверка isAtEnd() ИДЁТ ДО
            // чтения dense[index]: ++index мог увести за границу массива.
            void skipInactive()
            {
                while (!this->isAtEnd() && !this->pool->dense[this->index].component->isActive)
                {
                    ++this->index;
                }
            }
        };

        typedef IteratorBase<ComponentPool> Iterator;
        typedef IteratorBase<const ComponentPool> ConstIterator;

        Iterator begin() noexcept { return Iterator(this, Iterator::IteratorOrigin::Begin); }
        Iterator end() noexcept { return Iterator(this, Iterator::IteratorOrigin::End); }

        ConstIterator begin() const noexcept { return ConstIterator(this, ConstIterator::IteratorOrigin::Begin); }
        ConstIterator end() const noexcept { return ConstIterator(this, ConstIterator::IteratorOrigin::End); }

        ConstIterator cbegin() const noexcept { return ConstIterator(this, ConstIterator::IteratorOrigin::Begin); }
        ConstIterator cend() const noexcept { return ConstIterator(this, ConstIterator::IteratorOrigin::End); }

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
         * Для систем предпочтительнее итераторы begin()/end():
         * они дают и компонент, и владельца (it.getEntityId())
         * без ручной индексации. Индексный доступ остаётся
         * для тестов и диагностики.
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
         * 
         * Для систем предпочтительнее итераторы begin()/end()
         * (см. getEntityId). Неактивные компоненты (isActive == false)
         * этим методом возвращаются как есть — итераторы их пропускают.
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
