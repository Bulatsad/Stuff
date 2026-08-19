#include <blib/graphics/animator.h>

#include <cmath>

bool blib::graphics::Animator::loadFromAssimp(const aiScene* paiscene)
{
    this->animationList.resize(paiscene->mNumAnimations);

    for (size_t i = 0; i < this->animationList.size(); ++i)
    {
        if(!(this->animationList[i].loadFromAssimp(paiscene->mAnimations[i])))
        {
            // TODO : Logging
            return false;
        }
    }

    this->currentAnimationIndex = 0;
    this->currentTimeMs = 0.0;
    this->isPlaying = false;

    return true;
}

const blib::graphics::AnimationClip* blib::graphics::Animator::getCurrentAnimation() const
{
    if (this->animationList.empty())
    {
        return nullptr;
    }

    return &(this->animationList[this->currentAnimationIndex]);
}

bool blib::graphics::Animator::selectAnimation(const std::string& animationName)
{
    for (size_t i = 0; i < this->animationList.size(); ++i)
    {
        if (this->animationList[i].name == animationName)
        {
            this->currentAnimationIndex = i;
            this->currentTimeMs = 0.0;
            return true;
        }
    }

    // TODO : Logging
    return false;
}

bool blib::graphics::Animator::play()
{
    if (this->animationList.empty())
    {
        return false;
    }

    this->isPlaying = true;
    return true;
}

bool blib::graphics::Animator::pause()
{
    this->isPlaying = false;
    return true;
}

bool blib::graphics::Animator::update(float deltaTimeMs)
{
    if (!(this->isPlaying) || this->animationList.empty())
    {
        return false;
    }

    this->currentTimeMs += deltaTimeMs;

    const blib::graphics::AnimationClip& clip = this->animationList[this->currentAnimationIndex];
    if (clip.cycled && clip.durationMs > 0.0)
    {
        this->currentTimeMs = std::fmod(this->currentTimeMs, clip.durationMs);
    }

    return true;
}
