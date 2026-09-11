#include <beng/client/systems/animationSystem.h>

#include <beng/client/components/animatorComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>

#include <blib/graphics/skinmodel.h>

namespace beng
{
    namespace
    {
        // Аниматор blib работает в миллисекундах, ECS — в секундах
        constexpr float secondsToMilliseconds = 1000.0f;
    }

    void AnimationSystem::update(_In beng::Scene& scene, float deltaTime)
    {
        // Пул обязан быть зарегистрирован вызывающим (как и в
        // TransformSystem: getComponentPool без регистрации — fatal)
        beng::ComponentPool<AnimatorComponent>& animPool = scene.getComponentPool<AnimatorComponent>();

        for (buint32 i = 0; i < animPool.size(); ++i)
        {
            AnimatorComponent* animComp = animPool.getByIndex(i);
            if (__blib_unlikely(!animComp->getAnimator()))
            {
                continue;
            }

            // Модель нужна, чтобы переложить позу скелета после
            // продвижения времени
            SkinnedMeshComponent* meshComp =
                scene.tryGetComponent<SkinnedMeshComponent>(animPool.getEntityId(i));
            if (__blib_unlikely(!meshComp || !meshComp->getModel()))
            {
                continue;
            }

            blib::graphics::SkinModel* model = meshComp->getModel();

            if (animComp->isPlaying())
            {
                // SkinModel::update двигает время и применяет клип
                // (при cycled=true время оборачивается fmod'ом)
                model->update(deltaTime * secondsToMilliseconds);

                // Нециклическая анимация доиграла — пауза. Поза уже
                // на последнем кадре: AnimationChannel::findBorders
                // клампит время за пределами ключей
                if (!animComp->isLooping())
                {
                    const blib::graphics::AnimationClip* clip = animComp->getAnimator()->getCurrentAnimation();
                    if (clip && clip->durationMs > 0.0 &&
                        animComp->getAnimator()->getCurrentTimeMs() >= clip->durationMs)
                    {
                        animComp->pause();
                    }
                }
            }
            else if (animComp->isPoseDirty())
            {
                // Пауза, но время/клип менялись (скраб): переложить
                // позу без продвижения времени — SkinModel::update(0)
                // применяет клип на текущем времени
                model->update(0.0f);
                animComp->clearPoseDirty();
            }
        }
    }

} // namespace beng
