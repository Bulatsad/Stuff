#pragma once

#include <beng/config.h>
#include <blib/utilmacro.h>

#include <type_traits>

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
     * Контракт имени типа:
     * - Каждый конкретный компонент ОБЯЗАН объявить статическое имя
     *   типа: `static constexpr const char* componentTypeName = "...";`
     * - Имя — стабильная идентичность типа внутри Scene: по нему тип
     *   регистрируется (scene.registerComponentType<T>) и ищется всеми
     *   шаблонными методами сцены; в будущем — пишется в save/load
     * - Имена уникальны в рамках процесса по конвенции (префикс модуля:
     *   "beng.Transform", "gravelands.Mob"); коллизия имени при
     *   регистрации в одной Scene — fatal error
     * - Проверка наличия имени — трейт HasComponentTypeName<T> ниже
     * 
     * Использование:
     *   class MyComponent : public IComponent {
     *   public:
     *       static constexpr const char* componentTypeName = "game.MyComponent";
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
     * HasComponentTypeName<T> - трейт контракта имени типа компонента.
     * 
     * Назначение:
     * - Статическая проверка что T объявляет
     *   `static constexpr const char* componentTypeName`
     *   (иначе регистрация/поиск типа в Scene невозможны)
     * - Даёт внятный static_assert вместо нечитаемых ошибок
     *   инстанцирования шаблона
     * 
     * Значение: std::true_type если T::componentTypeName существует
     * и конвертируется в const char*; иначе std::false_type.
     * 
     * SFINAE (void_t) гарантирует отсутствие hard error при проверке
     * типа без члена — false_type вместо ошибки компиляции.
     */
    template<typename T, typename = void>
    struct HasComponentTypeName : std::false_type
    {
    };

    template<typename T>
    struct HasComponentTypeName<T, std::void_t<decltype(T::componentTypeName)>>
        : std::is_convertible<decltype(T::componentTypeName), const char*>
    {
    };

} // namespace beng
