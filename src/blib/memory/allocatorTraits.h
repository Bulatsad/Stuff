#pragma once

namespace blib
{
namespace memory
{
    /**
     * AllocatorTraits - traits для определения свойств аллокаторов.
     * 
     * Назначение:
     * - Определение stateless vs stateful аллокаторов в compile-time
     * - Оптимизация для stateless аллокаторов (не нужно хранить состояние)
     * 
     * Использование:
     * - По умолчанию все аллокаторы считаются stateful (isStateless = false)
     * - Для stateless аллокаторов нужно создать специализацию с isStateless = true
     * 
     * Пример специализации:
     *   template<>
     *   struct AllocatorTraits<MallocAllocator> {
     *       static constexpr bool isStateless = true;
     *   };
     * 
     * Stateless аллокаторы:
     * - Не имеют внутреннего состояния
     * - Все экземпляры взаимозаменяемы
     * - Примеры: MallocAllocator, DefaultAllocator (прокси к GlobalAllocator)
     * - Оптимизация: можно не хранить объект, только vtable указатель
     * 
     * Stateful аллокаторы:
     * - Имеют внутреннее состояние (пулы памяти, арены и т.д.)
     * - Экземпляры не взаимозаменяемы
     * - Примеры: PoolAllocator, ArenaAllocator, StackAllocator
     * - Требуют копирования/shared ownership при копировании Allocator
     * 
     * @tparam AllocatorType Тип аллокатора для проверки
     */
    template<typename AllocatorType>
    struct AllocatorTraits
    {
        /**
         * По умолчанию считаем все аллокаторы stateful.
         * Это безопасный вариант - лишнее копирование лучше чем UB.
         */
        static constexpr bool isStateless = false;
    };

    // Примеры специализаций будут добавлены в конкретных реализациях аллокаторов
    // Например в defaultAllocator.h:
    //
    // class DefaultAllocator { ... };
    //
    // template<>
    // struct AllocatorTraits<DefaultAllocator> {
    //     static constexpr bool isStateless = true;
    // };

} // namespace memory
} // namespace blib
