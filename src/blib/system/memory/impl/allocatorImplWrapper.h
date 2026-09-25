#pragma once

#include <atomic>
#include <cstdio>
#include <new>
#include <type_traits>
#include <utility>

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
        virtual void* allocate(size_t size) __blib_pure_virtual_function;

        /**
         * Освободить блок памяти.
         * @param ptr Указатель на блок
         * @param size Размер в байтах
         */
        virtual void deallocate(void* ptr, size_t size) __blib_pure_virtual_function;

        /**
         * Создать shared копию (для обычного копирования Allocator).
         * 
         * Для stateless: просто новый объект того же типа
         * Для stateful: разделяет состояние через ref-counting
         * (первый вызов переводит уникального владельца в shared-режим)
         * 
         * ВАЖНО: метод НЕ const — промоция мутирует исходную обёртку
         * (аллокатор переезжает из inline-хранилища в контрольный блок).
         * Вызов из const-контекста Allocator легален: impl указывает
         * на не-const объект.
         * 
         * @return Новый экземпляр IAllocatorImpl или nullptr при отказе аллокации
         */
        virtual IAllocatorImpl* share() __blib_pure_virtual_function;

        /**
         * Создать независимую глубокую копию (для clone()).
         * 
         * Для stateless: эквивалентно share()
         * Для stateful: полное копирование внутреннего состояния
         * 
         * @return Новый независимый экземпляр IAllocatorImpl
         */
        virtual IAllocatorImpl* deepCopy() const __blib_pure_virtual_function;

        /**
         * Размер конкретного объекта-реализации в байтах.
         * 
         * Нужен Allocator::destroyImpl() для освобождения heap-объектов
         * через GlobalAllocator::deallocate (который требует размер).
         * Должен вызываться ДО виртуального деструктора.
         * 
         * @return sizeof(конкретной обёртки)
         */
        virtual size_t implSize() const __blib_pure_virtual_function;
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
        IAllocatorImpl* share() __blib_override
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
         * Реализует аллокацию сам (не зовёт share()): share() не const
         * (для stateful промоция мутирует источник), а deepCopy() const -
         * у stateless мутации нет, но сигнатуры различаются.
         * 
         * @return Новый экземпляр AllocatorImplWrapper
         */
        IAllocatorImpl* deepCopy() const __blib_override
        {
            // Для stateless deepCopy эквивалентен share - новый идентичный объект
            void* memory = GlobalAllocator::instance().allocate(sizeof(AllocatorImplWrapper));
            if (!memory)
            {
                return nullptr;
            }
            return new (memory) AllocatorImplWrapper();
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
     * - Хранит экземпляр AllocatorType ИЛИ ссылку на разделяемый
     *   контрольный блок (ref-counted shared ownership)
     * - Размер: sizeof(vtable ptr) + max(sizeof(AllocatorType), 8) + bool
     * 
     * Два режима хранения:
     * 1. Уникальный владелец (isShared == false): аллокатор живёт inline
     *    в union-хранилище. Маленькие аллокаторы целиком умещаются в SBO
     *    Allocator'а - ноль heap-аллокаций (инвариант SBO сохранён).
     * 2. Shared-режим (isShared == true): аллокатор живёт в контрольном
     *    блоке SharedState в heap; обёртка хранит указатель на него.
     *    При первом share() аллокатор ПЕРЕЕЗЖАЕТ из inline-хранилища
     *    в контрольный блок (промоция) - копирование возможно даже для
     *    move-only аллокаторов, т.к. копирования состояния не происходит.
     * 
     * Ref-counting:
     * - refCount: std::atomic<buint32> - копирование/уничтожение Allocator
     *   потокобезопасны (сам аллокатор - нет, его thread-safety не меняется).
     * - Последний владелец разрушает аллокатор и возвращает контрольный
     *   блок GlobalAllocator'у (точный размер известен статически).
     * 
     * deepCopy() (независимая копия, clone()):
     * - Требует copy-constructible AllocatorType: копируется состояние
     *   (из inline-хранилища или из контрольного блока - в зависимости
     *   от режима).
     * - Move-only типы (PoolAllocatorImpl): nullptr + stderr - честное
     *   «невозможно», clone() даёт «мёртвый» аллокатор (задокументировано).
     */
    template<typename AllocatorType>
    class AllocatorImplWrapper<AllocatorType, false> : public IAllocatorImpl
    {
    public:
        /**
         * Контрольный блок разделяемого состояния.
         * Владеет инстансом аллокатора после промоции; живёт в heap
         * (GlobalAllocator), освобождается последним владельцем.
         */
        struct SharedState
        {
            // Счётчик владельцев. Атомарный: share()/деструктор могут
            // вызываться из разных потоков.
            std::atomic<buint32> refCount;

            // Разделяемый инстанс аллокатора.
            AllocatorType allocator;

            explicit SharedState(AllocatorType&& a)
                : refCount(1)
                , allocator(std::move(a))
            {
            }
        };

        /**
         * Конструктор от аллокатора (перемещение) - уникальный владелец.
         * Аллокатор placement-конструируется в inline union-хранилище
         * (память выделяет SBO Allocator'а, здесь - только конструкция).
         * 
         * @param alloc Экземпляр аллокатора (будет перемещён)
         */
        explicit AllocatorImplWrapper(AllocatorType&& alloc)
            : isShared(false)
        {
            // Placement new разрешён - память не выделяет, только конструирует.
            new (&storage.allocator) AllocatorType(std::move(alloc));
        }

        /**
         * Деструктор. Управляет временем жизни в зависимости от режима:
         * - уникальный: разрушает inline-аллокатор;
         * - shared: декремент счётчика; последний владелец разрушает
         *   аллокатор и возвращает контрольный блок GlobalAllocator'у.
         */
        ~AllocatorImplWrapper() __blib_override
        {
            if (isShared)
            {
                // fetch_sub == 1 => мы были последним владельцем.
                if (storage.sharedState->refCount.fetch_sub(1) == 1)
                {
                    storage.sharedState->allocator.~AllocatorType();
                    GlobalAllocator::instance().deallocate(
                        storage.sharedState, sizeof(SharedState));
                }
            }
            else
            {
                storage.allocator.~AllocatorType();
            }
        }

        // Обёртка живёт в SBO (inline или heap-указатель) и перемещается
        // только bytewise (Allocator move) или через placement new (share/
        // deepCopy). C++-копирование/перемещение удалены: union-режим
        // нельзя корректно продублировать (double-free контрольного блока,
        // двойной деструктор inline-аллокатора).
        AllocatorImplWrapper(const AllocatorImplWrapper&) = delete;
        AllocatorImplWrapper& operator=(const AllocatorImplWrapper&) = delete;
        AllocatorImplWrapper(AllocatorImplWrapper&&) = delete;
        AllocatorImplWrapper& operator=(AllocatorImplWrapper&&) = delete;

        /**
         * Выделить блок памяти.
         * Переадресует вызов разделяемому (или inline) аллокатору.
         * 
         * @param size Размер блока в байтах
         * @return Указатель на блок или nullptr при ошибке
         */
        void* allocate(size_t size) __blib_override
        {
            if (isShared)
            {
                return storage.sharedState->allocator.allocate(size);
            }
            return storage.allocator.allocate(size);
        }

        /**
         * Освободить блок памяти.
         * Переадресует вызов разделяемому (или inline) аллокатору.
         * 
         * @param ptr Указатель на блок
         * @param size Размер блока в байтах
         */
        void deallocate(void* ptr, size_t size) __blib_override
        {
            if (isShared)
            {
                storage.sharedState->allocator.deallocate(ptr, size);
            }
            else
            {
                storage.allocator.deallocate(ptr, size);
            }
        }

        /**
         * Создать shared копию (ref-counting).
         * 
         * - Уникальный владелец: промоция - аллокатор переезжает из
         *   inline-хранилища в heap-контрольный блок SharedState,
         *   обёртка становится shared-владельцем.
         * - Shared-режим: просто refCount++ и новая обёртка в heap.
         * 
         * Работает для ЛЮБОГО AllocatorType (в т.ч. move-only):
         * состояние не копируется, а разделяется.
         * 
         * Порядок аллокаций: сначала память под новую обёртку, затем
         * (при промоции) под контрольный блок - при отказе второй
         * первая возвращается, источник не затронут.
         * 
         * ВАЖНО: метод не const - промоция мутирует обёртку-источник
         * (union-режим и inline-хранилище). Copy-конструктор Allocator
         * вызывает его через shareImpl() const: impl указывает на
         * не-const объект, что легально.
         * 
         * @return Новый экземпляр AllocatorImplWrapper в heap,
         *         или nullptr при отказе аллокации
         */
        IAllocatorImpl* share() __blib_override
        {
            // 1. Память под новую обёртку (освобождается в destroyImpl,
            //    путь 3, через implSize()).
            void* wrapperMem = GlobalAllocator::instance().allocate(
                sizeof(AllocatorImplWrapper));
            if (!wrapperMem)
            {
                return nullptr;
            }

            // 2. Промоция уникального владельца: аллокатор переезжает
            //    в heap-контрольный блок.
            if (!isShared)
            {
                void* stateMem = GlobalAllocator::instance().allocate(sizeof(SharedState));
                if (!stateMem)
                {
                    // Откат: источник не затронут, память обёртки возвращаем.
                    GlobalAllocator::instance().deallocate(
                        wrapperMem, sizeof(AllocatorImplWrapper));
                    return nullptr;
                }

                // Placement new: перемещение аллокатора в контрольный блок.
                SharedState* state = new (stateMem) SharedState(std::move(storage.allocator));

                // Moved-from инстанс в inline-хранилище разрушаем и
                // переключаем union на указатель.
                storage.allocator.~AllocatorType();
                storage.sharedState = state;
                isShared = true;
            }

            // 3. Учитываем новую копию (в т.ч. нашу).
            storage.sharedState->refCount.fetch_add(1);

            // 4. Новая shared-обёртка поверх уже выделенной памяти.
            return new (wrapperMem) AllocatorImplWrapper(storage.sharedState);
        }

        /**
         * Создать глубокую независимую копию (для clone()).
         * 
         * Требует copy-constructible AllocatorType: новый уникальный
         * владелец с копией состояния (из inline-хранилища или из
         * контрольного блока - в зависимости от режима).
         * 
         * Move-only аллокаторы (PoolAllocatorImpl): независимая копия
         * невыразима - диагностика в stderr (blib-system не имеет Console)
         * и nullptr; clone() такого аллокатора даёт «мёртвый» аллокатор.
         * 
         * @return Новый экземпляр AllocatorImplWrapper с независимым
         *         состоянием, или nullptr (OOM / move-only тип)
         */
        IAllocatorImpl* deepCopy() const __blib_override
        {
            if constexpr (std::is_copy_constructible<AllocatorType>::value)
            {
                void* mem = GlobalAllocator::instance().allocate(
                    sizeof(AllocatorImplWrapper));
                if (!mem)
                {
                    return nullptr;
                }

                const AllocatorType& source = isShared
                    ? storage.sharedState->allocator
                    : storage.allocator;

                // Временная копия состояния перемещается в inline-хранилище
                // новой обёртки-уникального владельца.
                return new (mem) AllocatorImplWrapper(AllocatorType(source));
            }
            else
            {
                // blib-system не имеет Console - диагностика напрямую в stderr
                fprintf(stderr,
                    "Allocator: deepCopy impossible for move-only stateful allocator\n");
                return nullptr;
            }
        }

        size_t implSize() const __blib_override
        {
            // Размер для освобождения heap-объекта через GlobalAllocator
            return sizeof(AllocatorImplWrapper);
        }

    private:
        /**
         * Конструктор shared-обёртки (только из share()).
         * Union содержит указатель на существующий контрольный блок.
         */
        explicit AllocatorImplWrapper(SharedState* state)
            : isShared(true)
        {
            storage.sharedState = state;
        }

        /**
         * Union-хранилище: inline-аллокатор (уникальный владелец) или
         * указатель на контрольный блок (shared-режим). Время жизни
         * членов управляет обёртка вручную (ctor/dtor/share) - у union
         * пустые ctor/dtor.
         */
        union Storage
        {
            AllocatorType allocator;  // Уникальный владелец: инстанс inline
            SharedState* sharedState; // Shared-режим: контрольный блок

            Storage()
            {
            }
            ~Storage()
            {
            }
        };

        Storage storage;  // Активный член определяется isShared
        bool isShared;    // false - inline-аллокатор, true - shared-режим
    };

} // namespace impl

// Импортируем IAllocatorImpl и AllocatorImplWrapper в namespace blib::memory
// для обратной совместимости с существующим кодом
using impl::IAllocatorImpl;
using impl::AllocatorImplWrapper;

} // namespace memory
} // namespace blib
