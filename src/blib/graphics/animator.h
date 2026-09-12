#pragma once

#include <blib/config.h>

#include <assimp/scene.h>

#include <blib/graphics/animationclip.h>
#include <blib/graphics/skelet.h>

#include <string>
#include <vector>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Animator
        {
        private:
            std::vector<blib::graphics::AnimationClip> animationList;
            size_t currentAnimationIndex = 0;
            double currentTimeMs = 0.0;
            bool isPlaying = false;

        public:
            const std::vector<blib::graphics::AnimationClip>& getAnimations() const { return this->animationList; }
            const blib::graphics::AnimationClip* getCurrentAnimation() const;
            double getCurrentTimeMs() const { return this->currentTimeMs; }
            bool playing() const { return this->isPlaying; }

            // Скраб времени: принудительно выставить позицию в текущем
            // клипе (используется слайдером плейбека; позу нужно
            // переложить через update — см. SkinModel::update)
            void setCurrentTime(double timeMs) { this->currentTimeMs = timeMs; }

            // Включает/выключает зацикливание ТЕКУЩЕГО клипа
            // (при выключенном cycled время не оборачивается fmod'ом)
            void setCycled(bool cycled)
            {
                if (!(this->animationList.empty()))
                {
                    this->animationList[this->currentAnimationIndex].cycled = cycled;
                }
            }

            bool loadFromAssimp(const aiScene* paiscene);

            // Добавить клипы из внешней сцены к текущему списку, не
            // сбрасывая текущий клип/время/воспроизведение. fallbackName
            // используется для безымянных и дублирующихся клипов, а
            // также как имя единственного клипа файла (у Mixamo клипы
            // часто называются "mixamo.com" — имя файла информативнее).
            // Если задан skelet, каналы клипов привязываются к его
            // костям (см. Skelet::bindClipToSkeleton)
            bool appendFromAssimp(
                _In const aiScene* paiscene,
                _In const blib::graphics::Skelet* skelet,
                _In const std::string& fallbackName);

            bool selectAnimation(const std::string& animationName);
            bool play();
            bool pause();
            bool update(float deltaTimeMs);
        };
    }
}
