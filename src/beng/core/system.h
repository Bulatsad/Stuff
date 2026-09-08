#pragma once

#include <beng/config.h>
#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace beng
{
    // Forward declaration
    class Scene;

    /**
     * ISystem - базовый интерфейс для всех систем ECS.
     * 
     * Назначение:
     * - Системы обрабатывают Entity с определённым набором компонентов
     * - Вызываются каждый кадр через Scene::update(deltaTime)
     * - Упорядочены по приоритету (меньше = раньше выполняется)
     * 
     * Философия:
     * - Системы — stateless логика обработки компонентов
     * - Вся обработка в методе update()
     * - Scene предоставляет доступ к Entity и component pools
     * 
     * Примеры систем:
     * - PhysicsSystem: обновляет позиции по velocity
     * - CollisionSystem: проверяет столкновения
     * - TransformSystem: пересчитывает мировые координаты иерархии
     * - RenderSystem: отрисовывает Entity с RenderableComponent
     * 
     * Использование:
     *   class MySystem : public ISystem {
     *   public:
     *       void update(Scene& scene, float deltaTime) override {
     *           // Получить пул компонентов
     *           auto& pool = scene.getComponentPool<MyComponent>();
     *           
     *           // Итерация по всем компонентам
     *           for (buint32 i = 0; i < pool.size(); ++i) {
     *               MyComponent* comp = pool.getByIndex(i);
     *               EntityID id = pool.getEntityId(i);
     *               // ... обработка
     *           }
     *       }
     *       
     *       const char* getName() const override { return "MySystem"; }
     *   };
     * 
     * Ограничения:
     * - Не thread-safe (все системы выполняются последовательно)
     * - Не владеют данными (все данные в компонентах)
     */
    class __beng_api ISystem
    {
    public:
        virtual ~ISystem() = default;

        /**
         * Главный метод обновления системы.
         * 
         * @param scene Ссылка на Scene (доступ к Entity и component pools)
         * @param deltaTime Время с предыдущего кадра (в секундах)
         * 
         * Вызывается каждый кадр из Scene::update().
         * Системы выполняются в порядке приоритета.
         */
        virtual void update(_In Scene& scene, float deltaTime) = 0;

        /**
         * Получить приоритет выполнения системы.
         * 
         * @return Приоритет (меньше = раньше выполняется)
         * 
         * Системы с меньшим приоритетом выполняются первыми.
         * По умолчанию = 0 (средний приоритет).
         * 
         * Примеры:
         * - TransformSystem: -100 (до всех, т.к. многие зависят от позиций)
         * - PhysicsSystem: -50
         * - CollisionSystem: 0
         * - RenderSystem: 100 (после всех, рендерит финальное состояние)
         */
        virtual bint32 getPriority() const { return 0; }

        /**
         * Получить название системы (для debug/logging).
         * 
         * @return Название системы
         */
        virtual const char* getName() const = 0;
    };

} // namespace beng
