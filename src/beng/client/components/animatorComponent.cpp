#include <beng/client/components/animatorComponent.h>

namespace beng
{
    AnimatorComponent::AnimatorComponent()
        : animator(nullptr)
        , loop(true)
        , poseDirty(false)
    {
    }

    void AnimatorComponent::setAnimator(_In blib::graphics::Animator* anim)
    {
        this->animator = anim;
    }

    blib::graphics::Animator* AnimatorComponent::getAnimator()
    {
        return this->animator;
    }

    const blib::graphics::Animator* AnimatorComponent::getAnimator() const
    {
        return this->animator;
    }

    bool AnimatorComponent::selectAnimation(_In const std::string& name)
    {
        if (__blib_unlikely(!this->animator))
        {
            return false;
        }

        // Animator::selectAnimation сбрасывает время в 0 и логирует
        // предупреждение, если клип не найден
        if (__blib_unlikely(!this->animator->selectAnimation(name)))
        {
            return false;
        }

        // Применяем текущий флаг зацикливания к новому клипу
        this->animator->setCycled(this->loop);

        // Поза устарела: даже на паузе нужно переложить скелет
        // (bind-поза → первый кадр выбранного клипа)
        this->poseDirty = true;
        return true;
    }

    bool AnimatorComponent::play()
    {
        if (__blib_unlikely(!this->animator))
        {
            return false;
        }

        return this->animator->play();
    }

    bool AnimatorComponent::pause()
    {
        if (__blib_unlikely(!this->animator))
        {
            return false;
        }

        return this->animator->pause();
    }

    bool AnimatorComponent::isPlaying() const
    {
        return this->animator && this->animator->playing();
    }

    void AnimatorComponent::setLoop(bool looped)
    {
        this->loop = looped;
        if (this->animator)
        {
            this->animator->setCycled(looped);
        }
    }

    bool AnimatorComponent::isLooping() const
    {
        return this->loop;
    }

    void AnimatorComponent::setTime(double timeMs)
    {
        if (!this->animator)
        {
            return;
        }

        this->animator->setCurrentTime(timeMs);
        this->poseDirty = true;
    }

    double AnimatorComponent::getCurrentTimeMs() const
    {
        return this->animator ? this->animator->getCurrentTimeMs() : 0.0;
    }

    double AnimatorComponent::getDurationMs() const
    {
        if (!this->animator)
        {
            return 0.0;
        }

        const blib::graphics::AnimationClip* clip = this->animator->getCurrentAnimation();
        return clip ? clip->durationMs : 0.0;
    }

    const std::vector<blib::graphics::AnimationClip>& AnimatorComponent::getAnimations() const
    {
        // Пустой список для непривязанного компонента (static — живёт
        // всю жизнь процесса, ссылка валидна всегда)
        static const std::vector<blib::graphics::AnimationClip> emptyAnimations;

        return this->animator ? this->animator->getAnimations() : emptyAnimations;
    }

    bool AnimatorComponent::isPoseDirty() const
    {
        return this->poseDirty;
    }

    void AnimatorComponent::clearPoseDirty()
    {
        this->poseDirty = false;
    }

} // namespace beng
