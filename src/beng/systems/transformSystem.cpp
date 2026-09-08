#include <beng/systems/transformSystem.h>
#include <beng/core/scene.h>
#include <beng/components/transform.h>

namespace beng
{
    void TransformSystem::update(_In Scene& scene, float deltaTime)
    {
        // Получить пул Transform компонентов
        ComponentPool<TransformComponent>& pool = scene.getComponentPool<TransformComponent>();

        // Итерация по всем Transform компонентам.
        // Порядок в dense не важен: при пересчёте компонент сам
        // рекурсивно тянет мировую матрицу родителя.
        // Мёртвых Entity в пуле нет — destroyEntity чистит компоненты.
        for (buint32 i = 0; i < pool.size(); ++i)
        {
            TransformComponent* transform = pool.getByIndex(i);
            if (transform != nullptr)
            {
                // Вызвать getWorldMatrix() чтобы обновить кеш (если dirty)
                transform->getWorldMatrix();
            }
        }
    }

} // namespace beng
