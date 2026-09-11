#pragma once

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <beng/client/components/animatorComponent.h>

namespace beng
{
    namespace editor
    {
        /**
         * AnimationPanel — таблица анимаций + контролы плейбека.
         *
         * Назначение:
         * - Список клипов модели: клик по строке выбирает клип
         *   и запускает воспроизведение;
         * - Play/Pause, галочка зацикливания;
         * - Слайдер времени (скраб): работает и на паузе
         *   (поза переложится AnimationSystem).
         *
         * Данные: не владеет компонентом — указатель выставляется
         * вызывающим при загрузке модели (nullptr — панель пустая).
         */
        class __beng_api AnimationPanel : public beng::editor::IPanel
        {
        private:
            AnimatorComponent* animComp;

        public:
            AnimationPanel();

            /**
             * Привязать компонент анимации (nullptr — нет модели).
             */
            void setAnimatorComponent(_In_opt AnimatorComponent* comp);

            void draw() override;
            const char* getName() const override { return "Animations"; }
        };

    } // namespace editor
} // namespace beng
