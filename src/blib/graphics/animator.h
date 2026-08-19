#pragma once

#include <blib/config.h>

#include <assimp/scene.h>

#include <blib/graphics/animationclip.h>

#include <string>
#include <vector>

namespace blib
{
    namespace graphics
    {
        class __blib_api Animator
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

            bool loadFromAssimp(const aiScene* paiscene);
            bool selectAnimation(const std::string& animationName);
            bool play();
            bool pause();
            bool update(float deltaTimeMs);
        };
    }
}
