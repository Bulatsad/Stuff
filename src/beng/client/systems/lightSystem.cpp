#include <beng/client/systems/lightSystem.h>

#include <beng/client/components/ambientLightComponent.h>
#include <beng/client/components/directionalLightComponent.h>
#include <beng/core/scene.h>

namespace beng
{
    LightSystem::LightSystem()
        : renderTarget(nullptr)
    {
    }

    void LightSystem::setRenderTarget(_In blib::graphics::IRenderTarget* target)
    {
        this->renderTarget = target;
    }

    void LightSystem::update(_In beng::Scene& scene, float deltaTime)
    {
        (void)deltaTime; // свет статичен в течение кадра

        if (this->renderTarget == nullptr)
        {
            return;
        }

        blib::graphics::RenderContext& rc = this->renderTarget->rc;

        // Направленный свет: первый активный компонент пула. Пула или
        // компонентов нет — rc-дефолты остаются нетронутыми
        ComponentPool<DirectionalLightComponent>* directionalPool =
            scene.tryGetComponentPool<DirectionalLightComponent>();
        if (directionalPool != nullptr)
        {
            for (auto it = directionalPool->begin(); it != directionalPool->end(); ++it)
            {
                const DirectionalLightComponent& lightComp = *it;
                rc.directionalLight.direction = lightComp.getDirection();
                rc.directionalLight.color = lightComp.getColor();
                rc.directionalLight.intensity = lightComp.getIntensity();
                break;
            }
        }

        // Эмбиент: аналогично — первый активный компонент пула
        ComponentPool<AmbientLightComponent>* ambientPool =
            scene.tryGetComponentPool<AmbientLightComponent>();
        if (ambientPool != nullptr)
        {
            for (auto it = ambientPool->begin(); it != ambientPool->end(); ++it)
            {
                const AmbientLightComponent& lightComp = *it;
                rc.ambientLight.color = lightComp.getColor();
                rc.ambientLight.intensity = lightComp.getIntensity();
                break;
            }
        }
    }

} // namespace beng
