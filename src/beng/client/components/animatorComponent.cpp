#include <beng/client/components/animatorComponent.h>

#include <blib/core/json/json.h>
#include <blib/core/verifyHelper.h>

namespace beng
{
    namespace
    {
        // Ключи JSON-объекта компонента (формат save/load)
        constexpr const char* keyLoop = "loop";
        constexpr const char* keyPoseDirty = "poseDirty";
        constexpr const char* keyIsActive = "isActive";
    }
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

    blib::core::SaveStatus AnimatorComponent::save(_In blib::core::IOutputStream& os) const
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

        doc.set(keyLoop, blib::core::json::JsonValue(loop));
        doc.set(keyPoseDirty, blib::core::json::JsonValue(poseDirty));
        doc.set(keyIsActive, blib::core::json::JsonValue(isActive));

        if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "AnimatorComponent: failed to write JSON to stream");
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus AnimatorComponent::load(_In blib::core::IInputStream& is)
    {
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "AnimatorComponent: failed to parse JSON from stream");
        }

        // Валидация всех полей до применения (при ошибке состояние не меняется)
        if (__blib_unlikely(!doc.isObject()) ||
            __blib_unlikely(!doc.has(keyLoop) || !doc.get(keyLoop).isBool()) ||
            __blib_unlikely(!doc.has(keyPoseDirty) || !doc.get(keyPoseDirty).isBool()) ||
            __blib_unlikely(!doc.has(keyIsActive) || !doc.get(keyIsActive).isBool()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "AnimatorComponent: missing or malformed field");
        }

        loop = doc.get(keyLoop).asBool();
        poseDirty = doc.get(keyPoseDirty).asBool();
        isActive = doc.get(keyIsActive).asBool();
        // animator — контекст, не восстанавливается

        return blib::core::LoadStatus::None;
    }

    bool AnimatorComponent::strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const
    {
        // Защита от циклов
        if (!session.enter(this, &other))
        {
            return true;
        }

        const AnimatorComponent& o = static_cast<const AnimatorComponent&>(other);

        // Базовые поля + собственные данные. animator — контекст:
        // сравнение по null-состоянию (привязан / не привязан)
        return getOwnerId() == o.getOwnerId() &&
            isActive == o.isActive &&
            loop == o.loop &&
            poseDirty == o.poseDirty &&
            ((animator == nullptr) == (o.animator == nullptr));
    }

    bool AnimatorComponent::verify() const
    {
        // Round-trip без RTTI (см. blib::core::verifyRoundTrip).
        // У компонента с привязанным аниматором честно вернёт false:
        // standalone-копия аниматора не восстанавливает
        return blib::core::verifyRoundTrip(*this);
    }

} // namespace beng
