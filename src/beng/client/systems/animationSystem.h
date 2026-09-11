#pragma once

#include <beng/config.h>
#include <beng/core/system.h>

namespace beng
{
    /**
     * AnimationSystem — продвижение скелетной анимации.
     *
     * Для каждой сущности с AnimatorComponent + SkinnedMeshComponent:
     * - если играет: продвигает время (в миллисекундах, как требует
     *   blib::graphics::SkinModel::update) и применяет позу;
     * - при выключенном зацикливании ставит компонент на паузу после
     *   достижения конца клипа (поза остаётся на последнем кадре);
     * - на паузе: перекладывает позу только если время/клип менялись
     *   (скраб слайдером) — без продвижения времени.
     *
     * Приоритет -50: после TransformSystem (-100), до рендера (100),
     * т.к. скелет двигается в пространстве трансформации сущности.
     */
    class __beng_api AnimationSystem : public beng::ISystem
    {
    public:
        static constexpr bint32 priority = -50;

        void update(_In beng::Scene& scene, float deltaTime) override;
        bint32 getPriority() const override { return AnimationSystem::priority; }
        const char* getName() const override { return "AnimationSystem"; }
    };

} // namespace beng
