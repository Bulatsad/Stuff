#include <beng/client/systems/renderSystem.h>

#include <beng/client/components/meshRenderComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>

#include <Windows.h>
#include <gl/GL.h>

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

        // Статические меши — по слоям в фиксированном порядке
        // (Ground → Shadow → AlphaTested → Opaque, см. RenderLayer).
        // Пул берётся не-fatal: сцены без статических мешей
        // (вьювер) не обязаны регистрировать MeshRenderComponent
        beng::ComponentPool<MeshRenderComponent>* meshPool = scene.tryGetComponentPool<MeshRenderComponent>();
        if (meshPool != nullptr)
        {
            this->drawLayer(scene, *meshPool, RenderLayer::Ground);
            this->drawLayer(scene, *meshPool, RenderLayer::Shadow);
            this->drawLayer(scene, *meshPool, RenderLayer::AlphaTested);
            this->drawLayer(scene, *meshPool, RenderLayer::Opaque);
        }

        // Скелетные модели — поверх непрозрачного слоя (Opaque):
        // трансформация сущности — единственный источник размещения
        beng::ComponentPool<SkinnedMeshComponent>& skinnedPool = scene.getComponentPool<SkinnedMeshComponent>();

        for (buint32 i = 0; i < skinnedPool.size(); ++i)
        {
            SkinnedMeshComponent* meshComp = skinnedPool.getByIndex(i);
            if (__blib_unlikely(!meshComp->getModel()))
            {
                continue;
            }

            blib::graphics::SkinModel* model = meshComp->getModel();

            beng::TransformComponent* transform =
                scene.tryGetComponent<beng::TransformComponent>(skinnedPool.getEntityId(i));
            if (transform)
            {
                model->setTransform(transform->getWorldMatrix());
            }

            this->renderTarget->draw(*model);
        }
    }

    void RenderSystem::drawLayer(
        _In beng::Scene& scene,
        _In beng::ComponentPool<MeshRenderComponent>& meshPool,
        RenderLayer layer)
    {
        // Слой теней рисуется с альфа-блендингом и без записи глубины
        // (мягкие диски не должны спорить с землёй и друг с другом).
        // Состояние восстанавливается после слоя
        const bool blended = (layer == RenderLayer::Shadow);

        if (blended)
        {
            this->renderTarget->rc.api.ogl.__blib_glEnable(GL_BLEND);
            this->renderTarget->rc.api.ogl.__blib_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            this->renderTarget->rc.api.ogl.__blib_glDepthMask(GL_FALSE);
        }

        for (buint32 i = 0; i < meshPool.size(); ++i)
        {
            MeshRenderComponent* meshComp = meshPool.getByIndex(i);
            if (meshComp->getLayer() != layer)
            {
                continue;
            }

            // Мировая трансформация сущности (если есть Transform):
            // ECS-позиция/вращение/масштаб — единственный источник
            // размещения меша в мире
            beng::TransformComponent* transform =
                scene.tryGetComponent<beng::TransformComponent>(meshPool.getEntityId(i));
            if (transform)
            {
                meshComp->getMesh().setTransform(transform->getWorldMatrix());
            }

            this->renderTarget->draw(meshComp->getMesh());
        }

        if (blended)
        {
            this->renderTarget->rc.api.ogl.__blib_glDepthMask(GL_TRUE);
            this->renderTarget->rc.api.ogl.__blib_glDisable(GL_BLEND);
        }
    }

} // namespace beng
