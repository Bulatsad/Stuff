#include <beng/client/components/animatorComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/core/scene.h>

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
        constexpr const char* keyClipName = "clipName";
        constexpr const char* keyTimeMs = "timeMs";
        constexpr const char* keyPlaying = "playing";
    }
    AnimatorComponent::AnimatorComponent()
        : animator(nullptr)
        , loop(true)
        , poseDirty(false)
        , currentClipName()
        , currentTimeMs(0.0)
        , playing(false)
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

        // Snapshot плейбека с ЖИВОГО аниматора (контекст): имя текущего
        // клипа ("" если клип не выбран/не привязан), время и флаг
        // воспроизведения. Пишется прямо в поток — члены компонента
        // не меняются (const-контракт save)
        const char* clipName = "";
        double timeMs = 0.0;
        bool playingNow = false;
        if (this->animator != nullptr)
        {
            const blib::graphics::AnimationClip* clip = this->animator->getCurrentAnimation();
            if (clip != nullptr)
            {
                clipName = clip->name.c_str();
            }
            timeMs = this->animator->getCurrentTimeMs();
            playingNow = this->animator->playing();
        }

        doc.set(keyLoop, blib::core::json::JsonValue(loop));
        doc.set(keyPoseDirty, blib::core::json::JsonValue(poseDirty));
        doc.set(keyClipName, blib::core::json::JsonValue(clipName));
        doc.set(keyTimeMs, blib::core::json::JsonValue(static_cast<bdouble>(timeMs)));
        doc.set(keyPlaying, blib::core::json::JsonValue(playingNow));
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
            __blib_unlikely(!doc.has(keyIsActive) || !doc.get(keyIsActive).isBool()) ||
            __blib_unlikely(!doc.has(keyClipName) || !doc.get(keyClipName).isString()) ||
            __blib_unlikely(!doc.has(keyTimeMs) || !doc.get(keyTimeMs).isNumber()) ||
            __blib_unlikely(!doc.has(keyPlaying) || !doc.get(keyPlaying).isBool()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "AnimatorComponent: missing or malformed field");
        }

        loop = doc.get(keyLoop).asBool();
        poseDirty = doc.get(keyPoseDirty).asBool();
        isActive = doc.get(keyIsActive).asBool();
        // animator — контекст, не восстанавливается; плейбек кладём в
        // члены-«отложенное состояние» — его применит onLoaded после
        // перепривязки аниматора модели
        currentClipName.assign(doc.get(keyClipName).asString().c_str());
        currentTimeMs = doc.get(keyTimeMs).asBdouble();
        playing = doc.get(keyPlaying).asBool();

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
        if (getOwnerId() != o.getOwnerId() ||
            isActive != o.isActive ||
            loop != o.loop ||
            poseDirty != o.poseDirty ||
            ((animator == nullptr) != (o.animator == nullptr)))
        {
            return false;
        }

        // Оба аниматора привязаны — строго сравниваем ЖИВОЙ плейбек
        // (клип/время/play): round-trip сцены обязан восстановить
        // состояние один в один (см. onLoaded)
        if (animator != nullptr)
        {
            const blib::graphics::AnimationClip* clipA = animator->getCurrentAnimation();
            const blib::graphics::AnimationClip* clipB = o.animator->getCurrentAnimation();
            if ((clipA == nullptr) != (clipB == nullptr))
            {
                return false;
            }
            if (clipA != nullptr && clipA->name != clipB->name)
            {
                return false;
            }
            if (animator->getCurrentTimeMs() != o.animator->getCurrentTimeMs())
            {
                return false;
            }
            if (animator->playing() != o.animator->playing())
            {
                return false;
            }
        }

        return true;
    }

    bool AnimatorComponent::verify() const
    {
        // Round-trip без RTTI (см. blib::core::verifyRoundTrip).
        // У компонента с привязанным аниматором честно вернёт false:
        // standalone-копия аниматора не восстанавливает
        return blib::core::verifyRoundTrip(*this);
    }

    void AnimatorComponent::onLoaded(_In Scene& scene)
    {
        // Вторая фаза Scene::load: перепривязать аниматор к модели
        // SkinnedMeshComponent той же сущности (у SkinnedMeshComponent
        // onLoaded уже отработал — кеш ресурсов сцены восстановил
        // разделяемую модель; тип регистрируется после beng.SkinnedMesh)
        SkinnedMeshComponent* meshComp =
            scene.tryGetComponent<SkinnedMeshComponent>(getOwnerId());
        blib::graphics::SkinModel* model = (meshComp != nullptr) ? meshComp->getModel() : nullptr;
        this->animator = (model != nullptr) ? &model->getAnimator() : nullptr;

        // Восстановление плейбека (отложенное состояние из load()):
        // выбор клипа сбрасывает время и применяет loop → cycled
        // (selectAnimation), затем время и play/pause — состояние
        // воспроизводится один в один с сохранённым
        if (this->animator == nullptr)
        {
            return;
        }
        if (!this->currentClipName.empty())
        {
            this->selectAnimation(this->currentClipName);
        }
        this->setTime(this->currentTimeMs);
        if (this->playing)
        {
            this->play();
        }
        else
        {
            this->pause();
        }
    }

} // namespace beng
