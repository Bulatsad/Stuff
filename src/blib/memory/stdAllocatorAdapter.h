#pragma once

#include <blib/memory/allocator.h>
#include <utility>
#include <cstddef>

namespace blib
{
namespace memory
{
    /**
     * StdAllocatorAdapter - адаптер для использования blib::memory::Allocator с STL контейнерами.
     * 
     * Назначение:
     * - Позволяет использовать любой blib::memory::Allocator с std::vector, std::list и т.д.
     * - Соответствует требованиям std::allocator_traits (C++11/14/17)
     * - Поддерживает rebind для типов отличных от T
     * 
     * Использование:
     *   blib::memory::Allocator myAlloc;
     *   std::vector<int, blib::memory::StdAllocatorAdapter<int>> vec(&myAlloc);
     *   vec.push_back(42);
     * 
     * Важно:
     * - Хранит УКАЗАТЕЛЬ на Allocator (не копирует его)
     * - Allocator должен жить дольше всех контейнеров использующих этот адаптер
     * - Не thread-safe сам по себе (зависит от Allocator)
     * 
     * Требования std::allocator:
     * - value_type, size_type, difference_type
     * - allocate(n), deallocate(p, n)
     * - construct(p, args...), destroy(p) - deprecated в C++17, но поддерживаются
     * - rebind для совместимости с контейнерами
     * - операторы сравнения
     * 
     * @tparam T Тип элементов для аллокации
     */
    template<typename T>
    class StdAllocatorAdapter
    {
    public:
        // ============================================================================
        // Типы требуемые std::allocator_traits
        // ============================================================================
        
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;

        /**
         * rebind - механизм для создания аллокатора другого типа.
         * Требуется для контейнеров использующих внутренние узлы (std::list, std::map и т.д.)
         * 
         * Пример: std::list<int> внутри использует аллокатор для узлов списка,
         * поэтому делает rebind<Node> от аллокатора<int>
         */
        template<typename U>
        struct rebind
        {
            using other = StdAllocatorAdapter<U>;
        };

        // ============================================================================
        // Конструкторы
        // ============================================================================

        /**
         * Конструктор от указателя на Allocator.
         * 
         * @param alloc Указатель на blib::memory::Allocator (не может быть nullptr)
         * 
         * ВАЖНО: Адаптер не владеет Allocator, только хранит указатель.
         * Пользователь должен гарантировать что Allocator живёт дольше адаптера.
         */
        explicit StdAllocatorAdapter(Allocator* alloc)
            : allocator(alloc)
        {
        }

        /**
         * Конструктор копирования от адаптера того же типа.
         */
        StdAllocatorAdapter(const StdAllocatorAdapter& other)
            : allocator(other.allocator)
        {
        }

        /**
         * Конструктор от адаптера другого типа (для rebind).
         * 
         * @tparam U Тип элементов другого адаптера
         * @param other Адаптер другого типа
         * 
         * Важно: оба адаптера будут указывать на один и тот же Allocator.
         */
        template<typename U>
        StdAllocatorAdapter(const StdAllocatorAdapter<U>& other)
            : allocator(other.allocator)
        {
        }

        /**
         * Деструктор - ничего не делает (не владеет Allocator).
         */
        ~StdAllocatorAdapter() = default;

        // ============================================================================
        // Основные методы аллокации
        // ============================================================================

        /**
         * Выделить массив из n элементов типа T.
         * 
         * @param n Количество элементов (не байт!)
         * @return Указатель на выделенный массив
         * 
         * Важно:
         * - Выделяет n * sizeof(T) байт
         * - НЕ вызывает конструкторы объектов (это делает контейнер через construct)
         * - Может бросить std::bad_alloc при ошибке (зависит от Allocator)
         */
        T* allocate(size_type n)
        {
            if (n == 0)
            {
                return nullptr;
            }

            // Вычисляем размер в байтах
            size_type bytes = n * sizeof(T);

            // Выделяем через blib::memory::Allocator
            void* ptr = allocator->allocate(bytes);

            // Приводим к правильному типу
            return static_cast<T*>(ptr);
        }

        /**
         * Освободить ранее выделенный массив.
         * 
         * @param p Указатель на массив (полученный из allocate)
         * @param n Количество элементов (должно совпадать с allocate)
         * 
         * Важно:
         * - НЕ вызывает деструкторы объектов (это делает контейнер через destroy)
         * - n должен точно совпадать с тем что был в allocate
         */
        void deallocate(T* p, size_type n)
        {
            if (!p || n == 0)
            {
                return;
            }

            // Вычисляем размер в байтах
            size_type bytes = n * sizeof(T);

            // Освобождаем через blib::memory::Allocator
            allocator->deallocate(p, bytes);
        }

        // ============================================================================
        // Методы construct/destroy (deprecated в C++17, но нужны для совместимости)
        // ============================================================================

        /**
         * Создать объект типа U по адресу p с аргументами args.
         * 
         * @tparam U Тип создаваемого объекта (может отличаться от T для rebind)
         * @tparam Args Типы аргументов конструктора
         * @param p Указатель на выделенную память
         * @param args Аргументы для конструктора U
         * 
         * Использует placement new для создания объекта в уже выделенной памяти.
         */
        template<typename U, typename... Args>
        void construct(U* p, Args&&... args)
        {
            // Placement new - создаём объект в уже выделенной памяти
            new (p) U(std::forward<Args>(args)...);
        }

        /**
         * Уничтожить объект типа U по адресу p.
         * 
         * @tparam U Тип уничтожаемого объекта
         * @param p Указатель на объект
         * 
         * Явно вызывает деструктор, но НЕ освобождает память (это делает deallocate).
         */
        template<typename U>
        void destroy(U* p)
        {
            // Явно вызываем деструктор
            p->~U();
        }

        // ============================================================================
        // Операторы сравнения
        // ============================================================================

        /**
         * Проверка на равенство двух адаптеров.
         * 
         * Два адаптера равны если они используют один и тот же Allocator.
         * Это важно для контейнеров при swap и move operations.
         * 
         * @tparam U Тип элементов другого адаптера
         * @param other Другой адаптер
         * @return true если оба адаптера используют один Allocator
         */
        template<typename U>
        bool operator==(const StdAllocatorAdapter<U>& other) const
        {
            return allocator == other.allocator;
        }

        /**
         * Проверка на неравенство двух адаптеров.
         */
        template<typename U>
        bool operator!=(const StdAllocatorAdapter<U>& other) const
        {
            return allocator != other.allocator;
        }

    private:
        // Указатель на blib::memory::Allocator (не владеющий)
        Allocator* allocator;

        // Дружественность для доступа к allocator при rebind
        template<typename U>
        friend class StdAllocatorAdapter;
    };

} // namespace memory
} // namespace blib
