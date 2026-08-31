#pragma once

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>
#include <blib/memory/sbo.h>

// Включаем определение AllocatorImplWrapper перед использованием
#include <blib/memory/impl/allocatorImplWrapper.h>

namespace blib
{
namespace memory
{
    /**
     * Allocator - основной type-erased аллокатор с константным размером.
     * 
     * Назначение:
     * - Единообразный API для управления памятью без шаблонной параметризации
     * - Type erasure - может инкапсулировать любой аллокатор (stateless/stateful)
     * - Константный размер (64 байта) позволяет использовать в не-шаблонных классах
     * - Small Buffer Optimization - маленькие аллокаторы хранятся inline без heap allocation
     * 
     * Архитектура:
     * ┌─────────────────────────────────────┐
     * │ SBO<56> storage                     │ ← inline буфер для маленьких impl
     * ├─────────────────────────────────────┤
     * │ IAllocatorImpl* impl (8 bytes)      │ ← указатель на реализацию
     * └─────────────────────────────────────┘
     * Итого: 64 байта (cache-line friendly)
     * 
     * Использование:
     *   // Дефолтный аллокатор
     *   Allocator alloc;
     *   void* ptr = alloc.allocate(100);
     *   alloc.deallocate(ptr, 100);
     * 
     *   // Кастомный аллокатор
     *   MyCustomAllocator custom;
     *   Allocator alloc2(std::move(custom));
     * 
     * Копирование:
     * - По умолчанию использует shared ownership (share())
     * - Для независимой копии используйте clone() -> deepCopy()
     * 
     * Thread-safety:
     * - Зависит от конкретной реализации аллокатора
     * - GlobalAllocator и DefaultAllocator - thread-safe
     * 
     * Ограничения:
     * - Виртуальные вызовы добавляют overhead (~5-10ns)
     * - Alignment инкапсулирован в конкретных реализациях
     * - deallocate() требует передачи размера (как в std::allocator)
     */
    class __blib_api Allocator
    {
    public:
        /**
         * Конструктор по умолчанию - создаёт аллокатор с DefaultAllocator.
         */
        Allocator();

        /**
         * Конструктор от конкретного аллокатора (type erasure).
         * 
         * @tparam AllocatorImpl Тип аллокатора (должен иметь методы allocate/deallocate)
         * @param alloc Экземпляр аллокатора (будет перемещён)
         * 
         * Работа:
         * - Проверяет AllocatorTraits<AllocatorImpl>::isStateless
         * - Для stateless: создаёт легковесную обёртку
         * - Для stateful: копирует объект в SBO или heap
         * 
         * Требования к AllocatorImpl:
         * - void* allocate(size_t size)
         * - void deallocate(void* ptr, size_t size)
         * - Опционально: специализация AllocatorTraits<AllocatorImpl>
         */
        template<typename AllocatorImpl>
        explicit Allocator(AllocatorImpl&& alloc);

        /**
         * Деструктор - уничтожает impl и освобождает ресурсы.
         */
        ~Allocator();

        /**
         * Конструктор копирования - использует share() для shared ownership.
         * 
         * Для stateless: создаёт новый идентичный объект (дёшево)
         * Для stateful: разделяет состояние через ref-counting (пока простое копирование)
         * 
         * TODO: Реализовать настоящий ref-counting для stateful аллокаторов
         */
        Allocator(const Allocator& other);

        /**
         * Конструктор перемещения - забирает impl у other.
         */
        Allocator(Allocator&& other) noexcept;

        /**
         * Оператор присваивания копированием.
         */
        Allocator& operator=(const Allocator& other);

        /**
         * Оператор присваивания перемещением.
         */
        Allocator& operator=(Allocator&& other) noexcept;

        /**
         * Выделить блок памяти.
         * 
         * @param size Размер блока в байтах (должен быть > 0)
         * @return Указатель на выделенный блок или nullptr при ошибке
         * 
         * Alignment:
         * - Определяется конкретной реализацией аллокатора
         * - DefaultAllocator использует системное выравнивание
         * - Для специфичного alignment используйте соответствующий аллокатор
         *   (например, AlignedAllocator<32> для 32-byte alignment)
         */
        void* allocate(size_t size);

        /**
         * Освободить ранее выделенный блок памяти.
         * 
         * @param ptr Указатель на блок (должен быть получен из allocate)
         * @param size Размер блока в байтах (должен совпадать с allocate)
         * 
         * ВАЖНО: size ОБЯЗАТЕЛЕН и должен точно совпадать с размером при аллокации.
         * Это требование std::allocator и необходимо для некоторых аллокаторов
         * (например, pool allocators выбирают пул по размеру).
         */
        void deallocate(_In void* ptr, size_t size);

        /**
         * Создать независимую глубокую копию аллокатора.
         * 
         * @return Новый Allocator с независимым состоянием
         * 
         * Отличие от копирования:
         * - Обычное копирование: shared ownership (статефул аллокаторы делят состояние)
         * - clone(): полностью независимая копия (своё состояние)
         * 
         * Для stateless: эквивалентно копированию
         * Для stateful: полное дублирование внутреннего состояния
         */
        Allocator clone() const;

    private:
        // Размер SBO буфера: 64 - sizeof(void*) = 56 байт
        static constexpr size_t SBO_SIZE = 56;

        // Small Buffer Optimization для хранения маленьких impl inline
        SBO<SBO_SIZE> storage;

        // Указатель на реализацию (в storage или в heap)
        IAllocatorImpl* impl;

        // Вспомогательные методы (реализованы в .cpp)
        void destroyImpl();
        IAllocatorImpl* shareImpl() const;
        IAllocatorImpl* cloneImpl() const;
    };

} // namespace memory
} // namespace blib

// Включаем реализацию шаблонного конструктора
#include <blib/memory/impl/allocator.inl>
