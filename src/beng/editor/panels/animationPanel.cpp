#include <beng/editor/panels/animationPanel.h>

#include <imgui/imgui.h>

#include <blib/graphics/animator.h>

namespace beng
{
    namespace editor
    {
        namespace
        {
            constexpr const char* panelTitle = "Animations";
            constexpr const char* noModelMessage = "No model loaded";
            constexpr const char* unnamedAnimationLabel = "(unnamed animation)";
            constexpr const char* playButtonLabel = "Play";
            constexpr const char* pauseButtonLabel = "Pause";
            constexpr const char* loopCheckboxLabel = "Loop";
            constexpr const char* timeSliderLabel = "Time";
            constexpr const char* timeSliderFormat = "%.0f ms";
            constexpr const char* timeTextFormat = "Time: %.1f / %.1f ms";
        }

        AnimationPanel::AnimationPanel()
            : animComp(nullptr)
        {
        }

        void AnimationPanel::setAnimatorComponent(_In_opt AnimatorComponent* comp)
        {
            this->animComp = comp;
        }

        void AnimationPanel::draw()
        {
            ImGui::Begin(panelTitle);

            if (__blib_unlikely(!this->animComp))
            {
                ImGui::TextDisabled(noModelMessage);
                ImGui::End();
                return;
            }

            // Таблица клипов: клик по строке выбирает и запускает
            // анимацию (Selectable сам подсвечивает текущую)
            const std::vector<blib::graphics::AnimationClip>& animations = this->animComp->getAnimations();
            const blib::graphics::AnimationClip* currentClip =
                this->animComp->getAnimator() ? this->animComp->getAnimator()->getCurrentAnimation() : nullptr;

            for (const blib::graphics::AnimationClip& animation : animations)
            {
                const bool isSelected = (currentClip == &animation);
                const char* displayName = animation.name.empty() ? unnamedAnimationLabel : animation.name.c_str();
                if (ImGui::Selectable(displayName, isSelected))
                {
                    this->animComp->selectAnimation(animation.name);
                    this->animComp->play();
                }
            }

            ImGui::Separator();

            // Play/Pause
            const bool isPlaying = this->animComp->isPlaying();
            if (ImGui::Button(isPlaying ? pauseButtonLabel : playButtonLabel))
            {
                if (isPlaying)
                {
                    this->animComp->pause();
                }
                else
                {
                    this->animComp->play();
                }
            }

            // Зацикливание
            ImGui::SameLine();
            bool loop = this->animComp->isLooping();
            if (ImGui::Checkbox(loopCheckboxLabel, &loop))
            {
                this->animComp->setLoop(loop);
            }

            // Скраб времени: слайдер редактируется в любой момент;
            // на паузе поза переложится через poseDirty-флаг
            const double currentTime = this->animComp->getCurrentTimeMs();
            const double duration = this->animComp->getDurationMs();
            if (duration > 0.0)
            {
                float timeValue = static_cast<float>(currentTime);
                if (ImGui::SliderFloat(timeSliderLabel, &timeValue, 0.0f, static_cast<float>(duration), timeSliderFormat))
                {
                    this->animComp->setTime(static_cast<double>(timeValue));
                }
            }

            ImGui::Text(timeTextFormat, currentTime, duration);

            ImGui::End();
        }

    } // namespace editor
} // namespace beng
