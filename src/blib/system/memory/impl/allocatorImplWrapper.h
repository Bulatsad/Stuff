#pragma once

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/system/memory/allocatorTraits.h>

namespace blib
{
namespace memory
{
namespace impl
{
    /**
     * IAllocatorImpl - внутренний интерфейс для type erasure.
     * 
     * INTERNAL USE ONLY - Do not use directly!
     * 
     * Назначение:
     * - Виртуальный интерфейс для полиморфного хранения разных аллокаторов
     * - Скрыт от пользователей (не экспортируется в публичный API)
     * - Обёртки для конкретных аллокаторов наследуются от этого интерфейса
     * 
     * Методы:
     * - allocate/deallocate: основной API
     * - share: для копирования Allocator (shared ownership)
     * - deepCopy: для clone() (независимая копия)
     */
    class IAllocatorImpl
    {
    public:
        virtual ~IAllocatorImpl() = default;

        /**
         * Выделить блок памяти.
         * @param size Размер в байтах
         * @return Указатель на блок или nullptr при ошибке
         */
        virtual void* allocate(size_t size) = 0;

        /**
         * Освободить блок памяти.
         * @param ptr Указатель на блок
         * @param size Размер в байтах
         */
        virtual void deallocate(void* ptr, size_t size) = 0;

        /**
         * Создать shared копию (для обычного копирования Allocator).
         * 
         * Для stateless: просто новый объект того же типа
         * Для stateful: может разделять состояние (TODO: ref-counting)
         * 
         * @return Новый экземпляр IAllocatorImpl
         */
        virtual IAllocatorImpl* share() const = 0;

        /**
         * Создать независимую глубокую копию (для clone()).
         * 
         * Для stateless: эквивалентно share()
         * Для stateful: полное копирование внутреннего состояния
         * 
         * @return Новый независимый экземпляр IAllocatorImpl
         */
        virtual IAllocatorImpl* deepCopy() const = 0;

        /**
         * Размер конкретного объекта-реализации в байтах.
         * 
         * Нужен Allocator::destroyImpl() для освобождения heap-объектов
         * через GlobalAllocator::deallocate (который требует размер).
         * Должен вызываться ДО виртуального деструктора.
         * 
         * @return sizeof(конкретной обёртки)
         */
        virtual size_t implSize() const = 0;
    };

    /**
     * AllocatorImplWrapper - шаблонная обёртка для конкретных аллокаторов.
     * 
     * INTERNAL USE ONLY - Do not use directly!
     * 
     * @tparam AllocatorType Конкретный тип аллокатора
     * @tparam IsStateless Stateless или stateful (из AllocatorTraits)
     * 
     * Специализации:
     * - IsStateless = true: не хранит состояние, создаёт новые объекты на лету
     * - IsStateless = false: хранит экземпляр аллокатора, копирует при share/deepCopy
     */
    template<typename AllocatorType, bool IsStateless>
    class AllocatorImplWrapper;

    // ============================================================================
    // Специализация для STATELESS аллокаторов
    // ============================================================================
    
    /**
     * AllocatorImplWrapper<AllocatorType, true> - для stateless аллокаторов.
     * 
     * Характеристики:
     * - Не хранит состояние аллокатора
     * - Создаёт временный объект AllocatorType при каждом вызове
     * - Оптимизация: компилятор может заинлайнить и убрать временный объект
     * - Размер: только vtable pointer (обычно 8 байт)
     */
    template<typename AllocatorType>
    class AllocatorImplWrapper<AllocatorType, true> : public IAllocatorImpl
    {
    public:
        /**
         * Конструктор по умолчанию.
         * Stateless аллокатор не требует инициализации.
         */
        AllocatorImplWrapper()
        {
            // Stateless - не нужно хранить состояние
        }

        /**
         * Выделить блок памяти.
         * Создаёт временный объект AllocatorType и вызывает его allocate().
         * 
         * @param size Размер блока в байтах
         * @return Указатель на блок или nullptr при ошибке
         */
        void* allocate(size_t size) __blib_override
        {
            // Создаём временный объект и вызываем метод
            // Оптимизация: компилятор может заинлайнить и убрать временный объект
            AllocatorType allocator;
            return allocator.allocate(size);
        }

        /**
         * Освободить блок памяти.
         * Создаёт временный объект AllocatorType и вызывает его deallocate().
         * 
         * @param ptr Указатель на блок
         * @param size Размер блока в байтах
         */
        void deallocate(void* ptr, size_t size) __blib_override
        {
            AllocatorType allocator;
            allocator.deallocate(ptr, size);
        }

        /**
         * Создать shared копию.
         * Для stateless просто создаём новый идентичный объект.
         * 
         * Память выделяется через GlobalAllocator (ПРАВИЛО: ::new запрещён,
         * все аллокации только через GlobalAllocator).
         * 
         * @return Новый экземпляр AllocatorImplWrapper в heap, или nullptr при отказе аллокации
         */
        IAllocatorImpl* share() const __blib_override
        {
            // Stateless - просто создаём новый идентичный объект
            void* memory = GlobalAllocator::instance().allocate(sizeof(AllocatorImplWrapper));
            if (!memory)
            {
                // Аллокация не удалась - share невозможен
                return nullptr;
            }

            // Placement new разрешён (память не выделяет)
            return new (memory) AllocatorImplWrapper();
        }

        /**
         * Создать глубокую копию.
         * Для stateless эквивалентно share() (нет состояния для копирования).
         * 
         * @return Новый экземпляр AllocatorImplWrapper
         */
        IAllocatorImpl* deepCopy() const __blib_override
        {
            // Для stateless deepCopy эквивалентен share
            return share();
        }

        size_t implSize() const __blib_override
        {
            // Размер для освобождения heap-копий через GlobalAllocator
            return sizeof(AllocatorImplWrapper);
        }
    };

    // ============================================================================
    // Специализация для STATEFUL аллокаторов
    // ============================================================================
    
    /**
     * AllocatorImplWrapper<AllocatorType, false> - для stateful аллокаторов.
     * 
     * Характеристики:
     * - Хранит экземпляр AllocatorType
     * - Прямая переадресация вызовов к хранимому аллокатору
     * - Размер: sizeof(AllocatorType) + vtable pointer
     */
    template<typename AllocatorType>
    class AllocatorImplWrapper<AllocatorType, false> : public IAllocatorImpl
    {
    public:
        /**
         * Конструктор от аллокатора (перемещение).
         * 
         * @param alloc Экземпляр аллокатора (будет перемещён)
         */
        explicit AllocatorImplWrapper(AllocatorType&& alloc)
            : allocator(std::move(alloc))
        {
        }

        /**
         * Выделить блок памяти.
         * Переадресует вызов к хранимому аллокатору.
         * 
         * @param size Размер блока в байтах
         * @return Указатель на блок или nullptr при ошибке
         */
        void* allocate(size_t size) __blib_override
        {
            return allocator.allocate(size);
        }

        /**
         * Освободить блок памяти.
         * Переадресует вызов к хранимому аллокатору.
         * 
         * @param ptr Указатель на блок
         * @param size Размер блока в байтах
         */
        void deallocate(void* ptr, size_t size) __blib_override
        {
            allocator.deallocate(ptr, size);
        }

        /**
         * Создать shared копию.
         * 
         * ВНИМАНИЕ: Текущая реализация НЕ поддерживает настоящий ref-counting.
         * Для move-only аллокаторов (без copy constructor) эта операция невозможна.
         * 
         * TODO: Реализовать ref-counting с shared_ptr для shared ownership.
         * 
         * @return Новый экземпляр AllocatorImplWrapper с копией аллокатора,
         *         или nullptr если аллокатор не copyable
         */
        IAllocatorImpl* share() const __blib_override
        {
            // HACK: Пока просто возвращаем nullptr для move-only аллокаторов
            // В будущем нужен ref-counting
            // 
            // Проблема: std::is_copy_constructible нельзя использовать здесь,
            // т.к. нужна compile-time проверка для if constexpr
            //
            // Временное решение: используем nullptr для индикации что share() не поддерживается
            return nullptr;
        }

        /**
         * Создать глубокую независимую копию.
         * 
         * Для move-only аллокаторов эта операция невозможна.
         * 
         * @return nullptr (копирование не поддерживается для stateful move-only аллокаторов)
         */
        IAllocatorImpl* deepCopy() const __blib_override
        {
            // Полная независимая копия невозможна для move-only аллокаторов
            return nullptr;
        }

        size_t implSize() const __blib_override
        {
            // Размер для освобождения heap-объекта через GlobalAllocator
            return sizeof(AllocatorImplWrapper);
        }

    private:
        AllocatorType allocator; // Хранимый экземпляр аллокатора
    };

} // namespace impl

// Импортируем IAllocatorImpl и AllocatorImplWrapper в namespace blib::memory
// для обратной совместимости с существующим кодом
using impl::IAllocatorImpl;
using impl::AllocatorImplWrapper;

} // namespace memory
} // namespace blib
