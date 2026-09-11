#include <beng/client/systems/renderSystem.h>

#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>

namespace beng
{
    RenderSystem::RenderSystem()
        : renderTarget(nullptr)
    {
    }

    void RenderSystem::setRenderTarget(_In blib::graphics::IRenderTarget* target)
    {
        this->renderTarget = target;
    }

    void RenderSystem::update(_In beng::Scene& scene, float deltaTime)
    {
        if (__blib_unlikely(!this->renderTarget))
        {
            return;
        }

        beng::ComponentPool<SkinnedMeshComponent>& meshPool = scene.getComponentPool<SkinnedMeshComponent>();

        for (buint32 i = 0; i < meshPool.size(); ++i)
        {
            SkinnedMeshComponent* meshComp = meshPool.getByIndex(i);
            if (__blib_unlikely(!meshComp->getModel()))
            {
                continue;
            }

            blib::graphics::SkinModel* model = meshComp->getModel();

            // Мировая трансформация сущности (если есть Transform):
            // ECS-позиция/вращение/масштаб — единственный источник
            // размещения модели в мире
            beng::TransformComponent* transform =
                scene.tryGetComponent<beng::TransformComponent>(meshPool.getEntityId(i));
            if (transform)
            {
                model->setTransform(transform->getWorldMatrix());
            }

            this->renderTarget->draw(*model);
        }
    }

} // namespace beng
