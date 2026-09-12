#include <blib/graphics/animator.h>

#include <blib/core/console/console.h>

#include <cmath>
#include <utility>

namespace
{
    // Разделитель числового суффикса для дублирующихся имён клипов
    constexpr const char* clipNameSuffixSeparator = " #";

    // Уникальное имя клипа: если baseName уже занят существующими или
    // добавляемыми клипами, добавляется суффикс " #N" (N = 2, 3, ...)
    std::string makeUniqueClipName(
        _In const std::vector<blib::graphics::AnimationClip>& existing,
        _In const std::vector<blib::graphics::AnimationClip>& pending,
        size_t skipIndex,
        _In const std::string& baseName)
    {
        std::string candidate = baseName;
        for (size_t suffix = 2; ; ++suffix)
        {
            bool duplicate = false;
            for (const blib::graphics::AnimationClip& clip : existing)
            {
                if (clip.name == candidate)
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
            {
                for (size_t i = 0; i < pending.size(); ++i)
                {
                    if (i != skipIndex && pending[i].name == candidate)
                    {
                        duplicate = true;
                        break;
                    }
                }
            }
            if (!duplicate)
            {
                return candidate;
            }

            candidate = baseName + clipNameSuffixSeparator + std::to_string(suffix);
        }
    }
}

bool blib::graphics::Animator::loadFromAssimp(const aiScene* paiscene)
{
    this->animationList.resize(paiscene->mNumAnimations);

    for (size_t i = 0; i < this->animationList.size(); ++i)
    {
        if(!(this->animationList[i].loadFromAssimp(paiscene->mAnimations[i])))
        {
            __blib_log_error("failed to load animation #%zu from scene", i);
            return false;
        }
    }

    this->currentAnimationIndex = 0;
    this->currentTimeMs = 0.0;
    this->isPlaying = false;

    return true;
}

bool blib::graphics::Animator::appendFromAssimp(
    _In const aiScene* paiscene,
    _In const blib::graphics::Skelet* skelet,
    _In const std::string& fallbackName)
{
    if (__blib_unlikely(paiscene->mNumAnimations == 0))
    {
        __blib_log_error("animation scene has no animations");
        return false;
    }

    // Загрузка во временный список: при ошибке текущий не меняется
    std::vector<blib::graphics::AnimationClip> newClips(paiscene->mNumAnimations);
    for (size_t i = 0; i < newClips.size(); ++i)
    {
        if (!(newClips[i].loadFromAssimp(paiscene->mAnimations[i])))
        {
            __blib_log_error("failed to load animation #%zu from scene", i);
            return false;
        }
    }

    // Единственный клип файла именуем по файлу (fallbackName) — так
    // Mixamo-анимации не остаются все с именем "mixamo.com"
    const bool renameSingleClip = (newClips.size() == 1) && !(fallbackName.empty());
    for (size_t i = 0; i < newClips.size(); ++i)
    {
        if (renameSingleClip || newClips[i].name.empty())
        {
            newClips[i].name = fallbackName;
        }

        newClips[i].name = makeUniqueClipName(this->animationList, newClips, i, newClips[i].name);
    }

    // Привязка каналов к костям по дереву файла анимации: его
    // FBX-декомпозиция может отличаться от файла модели
    if (skelet)
    {
        for (blib::graphics::AnimationClip& clip : newClips)
        {
            skelet->bindClipToSkeleton(clip, paiscene);
        }
    }

    for (blib::graphics::AnimationClip& clip : newClips)
    {
        this->animationList.push_back(std::move(clip));
    }

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

    // Анимации с таким именем нет — предупреждаем вызывающего через консоль
    __blib_log_warning("animation '%s' not found in animator", animationName.c_str());
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
