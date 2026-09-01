#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <blib/blibint.h>
#include <blib/inline.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/globalAllocator.h>

namespace blib
{
namespace core
{
    /**
     * IAnyIteratorImpl - type-erased реализация итератора для AnyIterator.
     * 
     * Назначение:
     * - Скрывает конкретный тип итератора за виртуальным интерфейсом
     * - Позволяет AnyIterator<T> оборачивать любой std-совместимый forward итератор
     * 
     * Все методы виртуальные; конкретная реализация - AnyIteratorImplWrapper<T, Iter>.
     * 
     * @tparam T Тип элемента, на который указывает итератор (T& - результат разыменования)
     */
    template<typename T>
    class IAnyIteratorImpl
    {
    public:
        virtual ~IAnyIteratorImpl()
        {
        }

        /**
         * Продвинуть итератор на один шаг вперёд (operator++).
         */
        virtual void increment() = 0;

        /**
         * Разыменовать итератор (operator*).
         * 
         * ВАЖНО: объявлен const, так как const-итератор (как и const указатель)
         * всё равно должен выдавать T&, а не const T&.
         */
        virtual T& dereference() const = 0;

        /**
         * Сравнить с другим итератором (operator==).
         * 
         * Разнотипные итераторы (разные обёрнутые типы) всегда не равны.
         * Реализация проверяет typeid и сравнивает обёрнутые итераторы.
         */
        virtual bool equals(const IAnyIteratorImpl& other) const = 0;

        /**
         * Размер конкретной реализации в байтах (sizeof обёртки).
         * Нужен AnyIterator для принятия решения inline/сheap при копировании.
         */
        virtual size_t sizeOf() const = 0;

        /**
         * Выравнивание конкретной реализации (alignof обёртки).
         * Нужно AnyIterator для проверки допустимости inline-размещения в буфере.
         */
        virtual size_t alignmentOf() const = 0;

        /**
         * Скопировать себя в заранее подготовленный буфер dst (placement new).
         * 
         * @param dst Указатель на буфер достаточного размера (>= sizeOf())
         *            и с корректным выравниванием (>= alignmentOf())
         */
        virtual void cloneInto(_In void* dst) const = 0;
    };

    /**
     * AnyIteratorImplWrapper - конкретная type-erased реализация поверх любого итератора Iter.
     * 
     * @tparam T   Тип элемента (должен совпадать с T у AnyIterator)
     * @tparam Iter Конкретный оборачиваемый итератор (forward и выше, std-совместимый)
     */
    template<typename T, typename Iter>
    class AnyIteratorImplWrapper : public IAnyIteratorImpl<T>
    {
    public:
        /**
         * Конструктор от конкретного итератора (копируется во wrapper).
         */
        explicit AnyIteratorImplWrapper(const Iter& iter)
            : iter(iter)
        {
        }

        void increment() __blib_override
        {
            ++this->iter;
        }

        T& dereference() const __blib_override
        {
            return *this->iter;
        }

        bool equals(const IAnyIteratorImpl<T>& other) const __blib_override
        {
            // Разнотипные обёртки не равны (нельзя сравнить разные базовые типы).
            if (typeid(other) != typeid(*this))
                return false;

            // Типы совпали - сравниваем обёрнутые итераторы напрямую.
            return this->iter == static_cast<const AnyIteratorImplWrapper&>(other).iter;
        }

        size_t sizeOf() const __blib_override
        {
            return sizeof(*this);
        }

        size_t alignmentOf() const __blib_override
        {
            return alignof(AnyIteratorImplWrapper);
        }

        void cloneInto(_In void* dst) const __blib_override
        {
            // Placement new: копируем себя в подготовленный буфер.
            // Деструктор вызывается AnyIterator'ом через виртуальный деструктор.
            new (dst) AnyIteratorImplWrapper(*this);
        }

    private:
        Iter iter; // Конкретный оборачиваемый итератор
    };

    /**
     * AnyIterator - type-erased forward итератор с Small Buffer Optimization.
     * 
     * Назначение:
     * - Единый интерфейс итераторов для API, где тип итератора должен быть скрыт
     * - Может оборачивать любой std-совместимый forward/bidirectional/random access
     *   итератор (указатели, std::vector::iterator, собственные итераторы и т.д.)
     * 
     * Архитектура:
     * - Внутри: inline буфер 48 байт + указатель на IAnyIteratorImpl + флаг isHeap
     * - Если обёртка влезает в буфер - хранится inline (без heap-аллокаций)
     * - Иначе выделяется через GlobalAllocator
     * 
     * Использование:
     *   std::vector<int> v = {1, 2, 3};
     *   AnyIterator<int> it(v.begin());
     *   AnyIterator<int> end(v.end());
     *   while (it != end) { process(*it); ++it; }
     * 
     * Ограничения:
     * - Только forward и выше (нет operator--, +, -)
     * - Разнотипные AnyIterator равны только если оба пустые (null)
     * - Операции над пустым (default-constructed) итератором - UB
     * - Оборачиваемый итератор должен быть copy-конструируемым
     * - Не thread-safe (как и любые итераторы)
     * 
     * @tparam T Тип элемента; разыменование возвращает T&
     *           (используйте AnyIterator<const X> для константной итерации)
     */
    template<typename T>
    class AnyIterator
    {
    public:
        // std-совместимые typedef'ы (дублируются в iterator_traits специализации ниже)
        typedef std::forward_iterator_tag iterator_category;
        typedef typename std::remove_cv<T>::type value_type;
        typedef std::ptrdiff_t difference_type;
        typedef T* pointer;
        typedef T& reference;

        /**
         * Конструктор по умолчанию - создаёт пустой (null) итератор.
         * Два пустых итератора равны между собой; разыменование пустого - UB.
         */
        AnyIterator()
            : impl(nullptr)
            , isHeap(false)
        {
        }

        /**
         * Конструктор type erasure от любого std-совместимого итератора.
         * 
         * @tparam Iter Тип оборачиваемого итератора
         * @param iter Экземпляр итератора (будет скопирован)
         * 
         * Compile-time проверки:
         * - reference == T& (результат разыменования должен соответствовать T)
         * - категория итератора - forward и выше (input/output не поддерживаются)
         * - Iter copy-конструируем
         */
        template<typename Iter>
        AnyIterator(Iter iter);

        /**
         * Деструктор - уничтожает обёртку (inline или heap).
         */
        ~AnyIterator();

        /**
         * Конструктор копирования - глубокая копия обёрнутого итератора.
         */
        AnyIterator(const AnyIterator& other);

        /**
         * Конструктор перемещения - переносит владение, other становится пустым.
         */
        AnyIterator(AnyIterator&& other) noexcept;

        /**
         * Оператор присваивания копированием.
         */
        AnyIterator& operator=(const AnyIterator& other);

        /**
         * Оператор присваивания перемещением.
         */
        AnyIterator& operator=(AnyIterator&& other) noexcept;

        /**
         * Префиксный ++ - продвинуть итератор.
         * Для пустого итератора - UB.
         */
        AnyIterator& operator++();

        /**
         * Постфиксный ++.
         */
        AnyIterator operator++(int);

        /**
         * Разыменование (operator*) - возвращает ссылку на элемент.
         * Для пустого итератора - UB.
         */
        T& operator*() const;

        /**
         * Доступ к членам (operator->).
         */
        T* operator->() const;

        /**
         * Сравнение на равенство.
         * Пустые итераторы равны друг другу; пустой не равен непустому;
         * разнотипные непустые не равны.
         */
        bool operator==(const AnyIterator& other) const;

        /**
         * Сравнение на неравенство.
         */
        bool operator!=(const AnyIterator& other) const;

    private:
        // Размер inline буфера: 48 байт достаточно для обёртки над типичным
        // итератором (vptr 8 + итератор до 32 байт), итого объект ~64 байта
        // (cache-line friendly), по аналогии с blib::memory::Allocator.
        static constexpr size_t SBO_SIZE = 48;

        // Inline буфер для маленьких обёрток
        alignas(std::max_align_t) buint8 buffer[SBO_SIZE];

        // Указатель на реализацию (в buffer или в heap)
        IAnyIteratorImpl<T>* impl;

        // true если реализация выделена в heap через GlobalAllocator
        buint8 isHeap;

        // Вспомогательные методы
        void destroyImpl();
        void copyFrom(const AnyIterator& other);
        void moveFrom(AnyIterator&& other) noexcept;
    };

    // ------------------------------------------------------------------------
    // Реализация AnyIterator
    // ------------------------------------------------------------------------

    template<typename T>
    template<typename Iter>
    __blib_inline AnyIterator<T>::AnyIterator(Iter iter)
        : impl(nullptr)
        , isHeap(false)
    {
        typedef AnyIteratorImplWrapper<T, Iter> Impl;

        // Проверки std-совместимости оборачиваемого итератора
        static_assert(std::is_same<typename std::iterator_traits<Iter>::reference, T&>::value,
            "AnyIterator: reference type of wrapped iterator must be exactly T&");
        static_assert(std::is_convertible<typename std::iterator_traits<Iter>::iterator_category, std::forward_iterator_tag>::value,
            "AnyIterator: only forward/bidirectional/random access iterators are supported");
        static_assert(std::is_copy_constructible<Iter>::value,
            "AnyIterator: wrapped iterator must be copy constructible");

        // Inline если обёртка влезает в буфер по размеру и выравниванию,
        // иначе выделяем через GlobalAllocator.
        if (sizeof(Impl) <= SBO_SIZE && alignof(Impl) <= alignof(std::max_align_t))
        {
            new (this->buffer) Impl(iter);
            this->impl = reinterpret_cast<IAnyIteratorImpl<T>*>(this->buffer);
            this->isHeap = false;
        }
        else
        {
            void* mem = blib::memory::GlobalAllocator::instance().allocate(sizeof(Impl));
            if (!mem)
            {
                // Память не выделилась - остаёмся пустым итератором
                this->impl = nullptr;
                this->isHeap = false;
                return;
            }
            this->impl = new (mem) Impl(iter);
            this->isHeap = true;
        }
    }

    template<typename T>
    __blib_inline AnyIterator<T>::~AnyIterator()
    {
        this->destroyImpl();
    }

    template<typename T>
    __blib_inline AnyIterator<T>::AnyIterator(const AnyIterator& other)
        : impl(nullptr)
        , isHeap(false)
    {
        this->copyFrom(other);
    }

    template<typename T>
    __blib_inline AnyIterator<T>::AnyIterator(AnyIterator&& other) noexcept
        : impl(nullptr)
        , isHeap(false)
    {
        this->moveFrom(std::move(other));
    }

    template<typename T>
    __blib_inline AnyIterator<T>& AnyIterator<T>::operator=(const AnyIterator& other)
    {
        if (this != &other)
        {
            this->destroyImpl();
            this->copyFrom(other);
        }
        return *this;
    }

    template<typename T>
    __blib_inline AnyIterator<T>& AnyIterator<T>::operator=(AnyIterator&& other) noexcept
    {
        if (this != &other)
        {
            this->destroyImpl();
            this->moveFrom(std::move(other));
        }
        return *this;
    }

    template<typename T>
    __blib_inline AnyIterator<T>& AnyIterator<T>::operator++()
    {
        this->impl->increment();
        return *this;
    }

    template<typename T>
    __blib_inline AnyIterator<T> AnyIterator<T>::operator++(int)
    {
        AnyIterator tmp(*this);
        ++(*this);
        return tmp;
    }

    template<typename T>
    __blib_inline T& AnyIterator<T>::operator*() const
    {
        return this->impl->dereference();
    }

    template<typename T>
    __blib_inline T* AnyIterator<T>::operator->() const
    {
        return &this->impl->dereference();
    }

    template<typename T>
    __blib_inline bool AnyIterator<T>::operator==(const AnyIterator& other) const
    {
        // Пустые итераторы сравниваем по указателю (оба nullptr - равны)
        if (!this->impl || !other.impl)
            return this->impl == other.impl;

        return this->impl->equals(*other.impl);
    }

    template<typename T>
    __blib_inline bool AnyIterator<T>::operator!=(const AnyIterator& other) const
    {
        return !(*this == other);
    }

    template<typename T>
    __blib_inline void AnyIterator<T>::destroyImpl()
    {
        if (!this->impl)
            return;

        if (this->isHeap)
        {
            // Запоминаем размер до разрушения объекта
            size_t size = this->impl->sizeOf();

            // Виртуальный деструктор разрушает обёртку, затем освобождаем память
            this->impl->~IAnyIteratorImpl();
            blib::memory::GlobalAllocator::instance().deallocate(this->impl, size);
        }
        else
        {
            // Inline: только разрушаем объект, буфер не освобождаем
            this->impl->~IAnyIteratorImpl();
        }

        this->impl = nullptr;
        this->isHeap = false;
    }

    template<typename T>
    __blib_inline void AnyIterator<T>::copyFrom(const AnyIterator& other)
    {
        if (!other.impl)
        {
            // Копируем пустой итератор
            this->impl = nullptr;
            this->isHeap = false;
            return;
        }

        if (other.impl->sizeOf() <= SBO_SIZE && other.impl->alignmentOf() <= alignof(std::max_align_t))
        {
            // Inline-копия: обёртка сама себя размещает в наш буфер (placement new)
            other.impl->cloneInto(this->buffer);
            this->impl = reinterpret_cast<IAnyIteratorImpl<T>*>(this->buffer);
            this->isHeap = false;
        }
        else
        {
            // Heap-копия через GlobalAllocator
            void* mem = blib::memory::GlobalAllocator::instance().allocate(other.impl->sizeOf());
            if (!mem)
            {
                this->impl = nullptr;
                this->isHeap = false;
                return;
            }
            other.impl->cloneInto(mem);
            this->impl = static_cast<IAnyIteratorImpl<T>*>(mem);
            this->isHeap = true;
        }
    }

    template<typename T>
    __blib_inline void AnyIterator<T>::moveFrom(AnyIterator&& other) noexcept
    {
        if (other.isHeap)
        {
            // Heap: просто забираем указатель, объект физически не двигается
            this->impl = other.impl;
            this->isHeap = true;
        }
        else if (other.impl)
        {
            // Inline: переносим объект через copy-конструктор (cloneInto),
            // затем разрушаем исходный объект виртуальным деструктором.
            //
            // ВАЖНО: НЕ memcpy! Оборачиваемый итератор может быть stateful
            // (например, debug STL iterator с регистрацией в списке итераторов
            // контейнера через _Myproxy). Байтовая копия создала бы двойника,
            // деструктор которого ломает реестр контейнера
            // ("ITERATOR LIST CORRUPTED!"). cloneInto использует copy ctor,
            // который корректно регистрирует новый итератор, а деструктор
            // исходника корректно снимает его с учёта.
            other.impl->cloneInto(this->buffer);
            this->impl = reinterpret_cast<IAnyIteratorImpl<T>*>(this->buffer);
            this->isHeap = false;

            // Разрушаем объект в буфере исходника (буфер не освобождаем)
            other.impl->~IAnyIteratorImpl();
        }
        else
        {
            this->impl = nullptr;
            this->isHeap = false;
        }

        // Исходник больше не владеет объектом
        other.impl = nullptr;
        other.isHeap = false;
    }

    /**
     * Range - лёгкая обёртка пары итераторов [first, last) для range-based for
     * и передачи диапазонов в алгоритмы единым объектом.
     * 
     * Назначение:
     * - Хранение и передача "начала и конца" как единой сущности
     * - Работает с любыми std-совместимыми итераторами
     * 
     * Использование:
     *   std::vector<int> v = {1, 2, 3};
     *   Range<std::vector<int>::iterator> r(v.begin(), v.end());
     *   for (int& x : r) { ... }
     * 
     * @tparam Iter Тип итератора (копируемый, std-совместимый)
     */
    template<typename Iter>
    class Range
    {
    public:
        /**
         * Конструктор от пары итераторов [first, last).
         */
        Range(_In Iter first, _In Iter last)
            : first_(first)
            , last_(last)
        {
        }

        /**
         * Начало диапазона (включительно).
         */
        Iter begin() const
        {
            return this->first_;
        }

        /**
         * Конец диапазона (исключительно).
         */
        Iter end() const
        {
            return this->last_;
        }

        /**
         * Проверить, пуст ли диапазон.
         */
        bool empty() const
        {
            return this->first_ == this->last_;
        }

        /**
         * Количество элементов.
         * 
         * Сложность: O(1) для random access, O(n) для forward/bidirectional
         * (использует std::distance).
         */
        size_t size() const
        {
            return static_cast<size_t>(std::distance(this->first_, this->last_));
        }

    private:
        Iter first_; // Начало диапазона
        Iter last_;  // Конец диапазона (past-the-end)
    };

    /**
     * makeRange - создать Range по контейнеру (или C-массиву).
     * 
     * Использование:
     *   auto r = makeRange(v);        // v - любой контейнер с begin()/end()
     *   int arr[10];
     *   auto r2 = makeRange(arr);     // C-массив
     * 
     * @param container Контейнер (lvalue reference; rvalue не принимается,
     *                  т.к. диапазон по временному объекту даст висячие итераторы)
     * @return Range с итераторами container.begin()/end()
     */
    template<typename Container>
    __blib_inline auto makeRange(Container& container) -> Range<decltype(std::begin(container))>
    {
        return Range<decltype(std::begin(container))>(std::begin(container), std::end(container));
    }

} // namespace core
} // namespace blib

// ----------------------------------------------------------------------------
// std::iterator_traits для AnyIterator - делает его полноценным
// std-совместимым forward итератором (работает в std-алгоритмах).
// ----------------------------------------------------------------------------
namespace std
{
    template<typename T>
    struct iterator_traits<blib::core::AnyIterator<T>>
    {
        typedef std::forward_iterator_tag iterator_category;
        typedef typename std::remove_cv<T>::type value_type;
        typedef std::ptrdiff_t difference_type;
        typedef T* pointer;
        typedef T& reference;
    };
} // namespace std
