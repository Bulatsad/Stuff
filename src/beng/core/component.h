#pragma once

#include <beng/config.h>
#include <blib/utilmacro.h>
#include <blib/core/console/console.h>

#include <typeinfo>
#include <type_traits>
#include <cstdlib>

namespace beng
{
    /**
     * IComponent - базовый интерфейс для всех компонентов.
     * 
     * Назначение:
     * - Минимальный базовый класс для компонентов
     * - Виртуальный деструктор для корректного удаления через пул
     * - Хранит EntityID владельца (нужен компонентам с иерархией,
     *   например TransformComponent для поддержки children-списков)
     * 
     * Использование:
     *   class MyComponent : public IComponent {
     *       // ... данные компонента
     *   };
     */
    class __beng_api IComponent
    {
    public:
        /**
         * Конструктор - компонент без владельца (до добавления в Entity).
         */
        IComponent()
            : ownerId(invalidEntity)
        {
        }

        virtual ~IComponent() = default;

        /**
         * Получить EntityID владельца компонента.
         * 
         * @return EntityID владельца или invalidEntity, если не установлен
         * 
         * Выставляется автоматически в ComponentPool::create().
         */
        EntityID getOwnerId() const { return ownerId; }

        /**
         * INTERNAL: Установить EntityID владельца.
         * Вызывается из ComponentPool::create() сразу после конструирования.
         * Не вызывать вручную — нарушит инвариант "ownerId == сущности из sparse".
         */
        void setOwnerId(EntityID id) { ownerId = id; }

    private:
        // EntityID владельца компонента (invalidEntity пока не привязан)
        EntityID ownerId;
    };

    /**
     * ComponentTypeRegistry - генератор уникальных ID для типов компонентов.
     * 
     * Назначение:
     * - Автоматическая регистрация типов компонентов через template метод
     * - Проверка повторной регистрации (fatal error)
     * - Проверка превышения лимита типов (fatal error)
     * 
     * Использование:
     *   ComponentType id = ComponentTypeRegistry::getTypeId<TransformComponent>();
     * 
     * Ограничения:
     * - НЕ thread-safe: регистрация и чтение должны происходить в одном
     *   потоке (main thread) до запуска игрового цикла. Static-local
     *   инициализация ID каждого типа thread-safe, но счётчик nextTypeId
     *   и флаги регистрации — нет (гонка между разными T из разных потоков).
     * - ID выдаются последовательно и не переиспользуются.
     * - Максимум maxComponentTypes (componentMaskBits) типов компонентов.
     */
    class __beng_api ComponentTypeRegistry
    {
    public:
        /**
         * Получить уникальный ID для типа компонента.
         * 
         * @tparam T Тип компонента (должен наследоваться от IComponent)
         * @return Уникальный ComponentType ID
         * 
         * При первом вызове для типа T:
         * - Регистрирует новый тип
         * - Проверяет лимит и дубликаты
         * - Возвращает новый ID
         * 
         * При повторных вызовах:
         * - Возвращает закешированный ID (static local)
         */
        template<typename T>
        static ComponentType getTypeId()
        {
            static_assert(std::is_base_of<IComponent, T>::value,
                "Component type T must inherit from beng::IComponent");

            static ComponentType typeId = registerType<T>();
            return typeId;
        }

        /**
         * Проверить зарегистрирован ли тип компонента.
         * 
         * @param typeId ID типа компонента
         * @return true если тип зарегистрирован
         */
        static bool isRegistered(ComponentType typeId);

        /**
         * Получить количество зарегистрированных типов.
         * 
         * @return Количество зарегистрированных типов компонентов
         */
        static buint8 getRegisteredCount();

    private:
        // Счётчик для генерации ID (инкрементируется при каждой регистрации)
        static ComponentType nextTypeId;

        // Флаги регистрации (индекс = ComponentType ID)
        static bool typeRegistered[maxComponentTypes];

        /**
         * Внутренний метод регистрации типа (вызывается один раз для каждого T).
         * 
         * @tparam T Тип компонента
         * @return Новый уникальный ID
         * 
         * Проверки:
         * - Превышение лимита maxComponentTypes → fatal error
         * - Повторная регистрация → fatal error (не должно происходить из-за static local)
         */
        template<typename T>
        static ComponentType registerType()
        {
            // Проверка лимита типов
            if (__blib_unlikely(nextTypeId >= maxComponentTypes))
            {
                __blib_fatal("Component type limit exceeded (max %d types)",
                    static_cast<int>(maxComponentTypes));
            }

            ComponentType id = nextTypeId++;

            // Проверка повторной регистрации (паранойя, не должно случиться)
            if (__blib_unlikely(typeRegistered[id]))
            {
                __blib_fatal("Component type already registered: %d", static_cast<int>(id));
            }

            typeRegistered[id] = true;

            __blib_log_debug("Registered component type %d: %s",
                static_cast<int>(id), typeid(T).name());

            return id;
        }
    };

} // namespace beng
