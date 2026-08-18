#pragma once

#include <blib/config.h>

#include <assimp/scene.h>

#include <blib/graphics/animationclip.h>

#include <time.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Animator
        {
        private:
            std::vector<blib::graphics::AnimationClip> animationList;

        public:
            const std::vector<blib::graphics::AnimationClip>& getAnimations() const { return this->animationList; }
            bool play(clock_t deltaTime);
            bool selectAnimation(const std::string& animationName);
            bool loadFromAssimp(const aiScene* paiscene);
        };
    }
}
