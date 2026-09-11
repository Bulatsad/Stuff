#pragma once

#include <string>
#include <vector>

#include <beng/config.h>
#include <beng/core/component.h>

#include <blib/graphics/animator.h>

namespace beng
{
    /**
     * AnimatorComponent — компонент управления анимацией сущности.
     *
     * Назначение:
     * - Обёртка над blib::graphics::Animator (живёт внутри SkinModel,
     *   компонент им НЕ владеет);
     * - Добавляет состояние зацикливания и флаг «поза устарела»
     *   (нужен для скраба времени на паузе);
     * - Продвижение времени и применение позы делает AnimationSystem.
     *
     * Использование (типичный поток в вьювере):
     *   animComp.setAnimator(&meshComp.getModel()->getAnimator());
     *   animComp.selectAnimation("walk"); animComp.play();
     *   animComp.setLoop(false);
     */
    class __beng_api AnimatorComponent : public beng::IComponent
    {
    private:
        blib::graphics::Animator* animator;
        bool loop;
        bool poseDirty;

    public:
        AnimatorComponent();

        /**
         * Привязать аниматор модели (вызывается после загрузки модели).
         * Компонент не владеет аниматором: тот живёт внутри SkinModel
         * и умирает вместе с ней.
         */
        void setAnimator(_In blib::graphics::Animator* anim);
        blib::graphics::Animator* getAnimator();
        const blib::graphics::Animator* getAnimator() const;

        /**
         * Выбрать клип по имени и начать с нулевого времени.
         * @return false, если аниматор не привязан или клип не найден
         */
        bool selectAnimation(_In const std::string& name);

        /**
         * Запустить/остановить воспроизведение.
         */
        bool play();
        bool pause();
        bool isPlaying() const;

        /**
         * Зацикливание: пишется в cycled ТЕКУЩЕГО клипа.
         * При loop=false и достижении конца AnimationSystem ставит
         * компонент на паузу (поза остаётся на последнем кадре).
         */
        void setLoop(bool looped);
        bool isLooping() const;

        /**
         * Скраб: принудительно выставить время в текущем клипе.
         * Поза переложится AnimationSystem даже на паузе.
         */
        void setTime(double timeMs);
        double getCurrentTimeMs() const;
        double getDurationMs() const;

        /**
         * Сквозной доступ к списку клипов (для UI-таблицы).
         */
        const std::vector<blib::graphics::AnimationClip>& getAnimations() const;

        /**
         * INTERNAL: флаг «поза устарела» (для AnimationSystem).
         * Ставится при выборе клипа/скрабе на паузе, снимается
         * системой после пересчёта позы.
         */
        bool isPoseDirty() const;
        void clearPoseDirty();
    };

} // namespace beng
