#include <beng/systems/transformSystem.h>
#include <beng/core/scene.h>
#include <beng/components/transform.h>

namespace beng
{
    void TransformSystem::update(_In Scene& scene, float deltaTime)
    {
        // Получить пул Transform компонентов
        ComponentPool<TransformComponent>& pool = scene.getComponentPool<TransformComponent>();

        // Итерация по всем активным Transform компонентам.
        // Порядок в dense не важен: при пересчёте компонент сам
        // рекурсивно тянет мировую матрицу родителя.
        // Мёртвых Entity в пуле нет — destroyEntity чистит компоненты.
        for (auto it = pool.begin(); it != pool.end(); ++it)
        {
            // Вызвать getWorldMatrix() чтобы обновить кеш (если dirty)
            it->getWorldMatrix();
        }
    }

} // namespace beng
