#pragma once

#include <beng/config.h>
#include <beng/core/system.h>

namespace beng
{
    /**
     * TransformSystem - система обновления мировых координат Transform компонентов.
     * 
     * Назначение:
     * - Пересчитывает мировые матрицы для всех TransformComponent
     * - Обрабатывает иерархию (parent → children)
     * - Должна выполняться первой (приоритет -100)
     * 
     * Использование:
     *   TransformSystem transformSystem;
     *   scene.addSystem(&transformSystem);
     * 
     * Приоритет:
     * - -100 (выполняется до всех остальных систем)
     * - Это важно т.к. многие системы зависят от актуальных позиций
     */
    class __beng_api TransformSystem : public ISystem
    {
    public:
        /**
         * Обновить Transform компоненты.
         * 
         * @param scene Ссылка на Scene
         * @param deltaTime Время с предыдущего кадра (не используется)
         * 
         * Алгоритм:
         * - Итерация по всем TransformComponent
         * - Вызов getWorldMatrix() для обновления кеша (если dirty)
         */
        void update(_In Scene& scene, float deltaTime) __blib_override;

        /**
         * Получить приоритет системы.
         * 
         * @return -100 (выполняется первой)
         */
        bint32 getPriority() const __blib_override { return priorityValue; }

        /**
         * Получить название системы.
         * 
         * @return "TransformSystem"
         */
        const char* getName() const __blib_override { return systemName; }

    private:
        static constexpr bint32 priorityValue = -100;
        static constexpr const char* systemName = "TransformSystem";
    };

} // namespace beng
