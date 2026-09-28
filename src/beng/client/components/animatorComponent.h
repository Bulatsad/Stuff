#pragma once

#include <string>
#include <vector>

#include <beng/config.h>
#include <beng/core/icomponent.h>

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

        // Плейбек-состояние, восстановленное load() и применяемое к
        // аниматору в onLoaded (аниматор в load() ещё не привязан —
        // он живёт внутри модели SkinnedMeshComponent той же сущности).
        // Сериализуемые поля компонента; save() пишет их из ЖИВОГО
        // аниматора (snapshot), strongCompare сравнивает живой плейбек
        std::string currentClipName;
        double currentTimeMs;
        bool playing;

    public:
        // Стабильное имя типа — идентичность типа в таблице типов Scene
        // (регистрация, резолв в шаблонных методах, save/load)
        static constexpr const char* componentTypeName = "beng.Animator";

        // Не прятать 1-аргументную точку входа строгого сравнения
        using blib::core::IStrongComparable::strongCompare;

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

        // ========== ISaveLoadable: сериализация и сравнение ==========
        //
        // Формат save: JSON-объект {loop, poseDirty, isActive, clipName,
        // timeMs, playing}. Плейбек (клип/время/play) — сериализуемое
        // состояние компонента: save() снимает snapshot с ЖИВОГО
        // аниматора ("" при отсутствии текущего клипа), load()
        // кладёт его в члены-«отложенное состояние», onLoaded применяет
        // к перепривязанному аниматору модели. Указатель animator —
        // контекст: не сериализуется, сравнивается по null-состоянию;
        // при обоих привязанных strongCompare сравнивает ЖИВОЙ плейбек
        // (имя клипа, время, playing) бит-в-бит — round-trip сцены
        // воспроизводит состояние один в один.

        /**
         * Сохранить состояние компонента в поток (JSON-объект).
         */
        blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;

        /**
         * Загрузить состояние компонента из потока (JSON-объект).
         */
        blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;

        /**
         * Строгое сравнение: базовые поля + loop/poseDirty + animator
         * по null-состоянию; при обоих привязанных — живой плейбек
         * (клип/время/playing) бит-в-бит.
         */
        bool strongCompare(_In const blib::core::IStrongComparable& other,
            _In blib::core::CompareSession& session) const __blib_override;

        /**
         * Round-trip валидация (verifyRoundTrip).
         */
        bool verify() const __blib_override;

        /**
         * Вторая фаза загрузки сцены: перепривязать аниматор к модели
         * SkinnedMeshComponent той же сущности и восстановить плейбек
         * (клип → время → play/pause) из load()-состояния. Вызывается
         * Scene::load после onLoaded всех компонентов с меньшим
         * ComponentType — тип обязан регистрироваться ПОСЛЕ
         * beng.SkinnedMesh (см. IComponent::onLoaded).
         */
        void onLoaded(_In Scene& scene) __blib_override;
    };

} // namespace beng
