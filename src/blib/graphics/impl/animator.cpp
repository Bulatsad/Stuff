#include <blib/graphics/animator.h>

#include <blib/core/verifyHelper.h>

namespace
{
    // Ключи JSON-объекта клипа (формат сериализации)
    constexpr const char* clipKeyName = "name";
    constexpr const char* clipKeyTickPerSecond = "tickPerSecond";
    constexpr const char* clipKeyDurationTicks = "durationTicks";
    constexpr const char* clipKeyDurationMs = "durationMs";
    constexpr const char* clipKeyCycled = "cycled";
    constexpr const char* clipKeyChannels = "channels";
    constexpr const char* clipKeyBoneChains = "boneChains";

    // Ключи канала анимации
    constexpr const char* channelKeyBoneName = "boneName";
    constexpr const char* channelKeyPositionKeys = "positionKeys";
    constexpr const char* channelKeyRotationKeys = "rotationKeys";
    constexpr const char* channelKeyScaleKeys = "scaleKeys";

    // Ключи привязки клипа к костям
    constexpr const char* boneChainKeyBoneIndex = "boneIndex";
    constexpr const char* boneChainKeyElements = "elements";
    constexpr const char* elementKeyChannelIndex = "channelIndex";
    constexpr const char* elementKeyBindTransform = "bindTransform";

    // Ключи аниматора
    constexpr const char* animatorKeyClips = "clips";
    constexpr const char* animatorKeyCurrentIndex = "currentIndex";
    constexpr const char* animatorKeyTimeMs = "timeMs";
    constexpr const char* animatorKeyIsPlaying = "isPlaying";

    // Размерности сериализуемых структур
    constexpr buint32 animVector3Size = 3;
    constexpr buint32 animQuaternionSize = 4;
    constexpr buint32 animMatrixElementCount = 16;

    // Vector3f → массив [x, y, z]
    blib::core::json::JsonValue animVector3fToArray(_In const blib::graphics::Vector3f& v)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        arr.pushBack(blib::core::json::JsonValue(v.x));
        arr.pushBack(blib::core::json::JsonValue(v.y));
        arr.pushBack(blib::core::json::JsonValue(v.z));
        return arr;
    }

    // Quaternion<double> → массив [x, y, z, w]
    blib::core::json::JsonValue animQuaternionToArray(_In const blib::math::Quaternion<double>& q)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        arr.pushBack(blib::core::json::JsonValue(q.x));
        arr.pushBack(blib::core::json::JsonValue(q.y));
        arr.pushBack(blib::core::json::JsonValue(q.z));
        arr.pushBack(blib::core::json::JsonValue(q.w));
        return arr;
    }

    // Матрица → массив 16 чисел в порядке, согласованном с
    // Matrix(std::initializer_list): data[0][0], data[1][0], ..., data[3][3]
    blib::core::json::JsonValue animMatrixToArray(_In const blib::graphics::TransformMatrix& m)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        for (buint32 j = 0; j < 4; ++j)
        {
            for (buint32 i = 0; i < 4; ++i)
            {
                arr.pushBack(blib::core::json::JsonValue(m.data[i][j]));
            }
        }
        return arr;
    }

    bool animReadVector3f(_In const blib::core::json::JsonValue& v, _Out blib::graphics::Vector3f& out)
    {
        if (!v.isArray() || v.size() != animVector3Size)
        {
            return false;
        }
        out = blib::graphics::Vector3f(v[0].asBfloat(), v[1].asBfloat(), v[2].asBfloat());
        return true;
    }

    bool animReadQuaternion(_In const blib::core::json::JsonValue& v, _Out blib::math::Quaternion<double>& out)
    {
        if (!v.isArray() || v.size() != animQuaternionSize)
        {
            return false;
        }
        out = blib::math::Quaternion<double>(
            v[3].asBdouble(), v[0].asBdouble(), v[1].asBdouble(), v[2].asBdouble());
        return true;
    }

    bool animReadMatrix(_In const blib::core::json::JsonValue& arr, _Out blib::graphics::TransformMatrix& out)
    {
        if (!arr.isArray() || arr.size() != animMatrixElementCount)
        {
            return false;
        }
        blib::graphics::TransformMatrix m;
        m.loadIdentity();
        buint32 index = 0;
        for (buint32 j = 0; j < 4; ++j)
        {
            for (buint32 i = 0; i < 4; ++i)
            {
                m.data[i][j] = arr[index].asBfloat();
                ++index;
            }
        }
        out = m;
        return true;
    }

    bool animMatricesEqual(_In const blib::graphics::TransformMatrix& a, _In const blib::graphics::TransformMatrix& b)
    {
        for (buint32 j = 0; j < 4; ++j)
        {
            for (buint32 i = 0; i < 4; ++i)
            {
                if (a.data[i][j] != b.data[i][j])
                {
                    return false;
                }
            }
        }
        return true;
    }

    // Ключи канала: пары [time, [x, y, z]]
    blib::core::json::JsonValue animVectorKeysToArray(
        _In const std::vector<std::pair<double, blib::graphics::Vector3f>>& keys)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        for (const std::pair<double, blib::graphics::Vector3f>& key : keys)
        {
            blib::core::json::JsonValue pair = blib::core::json::JsonValue::makeArray();
            pair.pushBack(blib::core::json::JsonValue(key.first));
            pair.pushBack(animVector3fToArray(key.second));
            arr.pushBack(pair);
        }
        return arr;
    }

    // Ключи канала: пары [time, [x, y, z, w]]
    blib::core::json::JsonValue animQuaternionKeysToArray(
        _In const std::vector<std::pair<double, blib::math::Quaternion<double>>>& keys)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        for (const std::pair<double, blib::math::Quaternion<double>>& key : keys)
        {
            blib::core::json::JsonValue pair = blib::core::json::JsonValue::makeArray();
            pair.pushBack(blib::core::json::JsonValue(key.first));
            pair.pushBack(animQuaternionToArray(key.second));
            arr.pushBack(pair);
        }
        return arr;
    }

    // Чтение пар [time, [x, y, z]]
    bool animReadVectorKeys(_In const blib::core::json::JsonValue& arr,
        _Out std::vector<std::pair<double, blib::graphics::Vector3f>>& out)
    {
        out.clear();
        if (!arr.isArray())
        {
            return false;
        }
        for (buint32 i = 0; i < arr.size(); ++i)
        {
            const blib::core::json::JsonValue& pair = arr[i];
            if (!pair.isArray() || pair.size() != 2)
            {
                out.clear();
                return false;
            }
            blib::graphics::Vector3f value;
            if (!animReadVector3f(pair[1], value))
            {
                out.clear();
                return false;
            }
            out.push_back(std::make_pair(pair[0].asBdouble(), value));
        }
        return true;
    }

    // Чтение пар [time, [x, y, z, w]]
    bool animReadQuaternionKeys(_In const blib::core::json::JsonValue& arr,
        _Out std::vector<std::pair<double, blib::math::Quaternion<double>>>& out)
    {
        out.clear();
        if (!arr.isArray())
        {
            return false;
        }
        for (buint32 i = 0; i < arr.size(); ++i)
        {
            const blib::core::json::JsonValue& pair = arr[i];
            if (!pair.isArray() || pair.size() != 2)
            {
                out.clear();
                return false;
            }
            blib::math::Quaternion<double> value;
            if (!animReadQuaternion(pair[1], value))
            {
                out.clear();
                return false;
            }
            out.push_back(std::make_pair(pair[0].asBdouble(), value));
        }
        return true;
    }

    bool animVectorKeysEqual(_In const std::vector<std::pair<double, blib::graphics::Vector3f>>& a,
        _In const std::vector<std::pair<double, blib::graphics::Vector3f>>& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].first != b[i].first || a[i].second != b[i].second)
            {
                return false;
            }
        }
        return true;
    }

    bool animQuaternionKeysEqual(_In const std::vector<std::pair<double, blib::math::Quaternion<double>>>& a,
        _In const std::vector<std::pair<double, blib::math::Quaternion<double>>>& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].first != b[i].first ||
                a[i].second.x != b[i].second.x || a[i].second.y != b[i].second.y ||
                a[i].second.z != b[i].second.z || a[i].second.w != b[i].second.w)
            {
                return false;
            }
        }
        return true;
    }

    // Канал → JSON
    blib::core::json::JsonValue animChannelToJson(_In const blib::graphics::AnimationChannel& channel)
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();
        doc.set(channelKeyBoneName, blib::core::json::JsonValue(channel.boneName.c_str()));
        doc.set(channelKeyPositionKeys, animVectorKeysToArray(channel.positionKeys));
        doc.set(channelKeyRotationKeys, animQuaternionKeysToArray(channel.rotaionKeys));
        doc.set(channelKeyScaleKeys, animVectorKeysToArray(channel.scaleKeys));
        return doc;
    }

    // JSON → канал (false — неверная форма)
    bool animReadChannel(_In const blib::core::json::JsonValue& v, _Out blib::graphics::AnimationChannel& out)
    {
        if (!v.isObject() ||
            !v.has(channelKeyBoneName) || !v.get(channelKeyBoneName).isString() ||
            !v.has(channelKeyPositionKeys) || !v.has(channelKeyRotationKeys) ||
            !v.has(channelKeyScaleKeys))
        {
            return false;
        }

        blib::graphics::AnimationChannel channel;
        channel.boneName = v.get(channelKeyBoneName).asString().c_str();
        if (!animReadVectorKeys(v.get(channelKeyPositionKeys), channel.positionKeys) ||
            !animReadQuaternionKeys(v.get(channelKeyRotationKeys), channel.rotaionKeys) ||
            !animReadVectorKeys(v.get(channelKeyScaleKeys), channel.scaleKeys))
        {
            return false;
        }
        out = std::move(channel);
        return true;
    }

    bool animChannelsEqual(_In const blib::graphics::AnimationChannel& a, _In const blib::graphics::AnimationChannel& b)
    {
        return a.boneName == b.boneName &&
            animVectorKeysEqual(a.positionKeys, b.positionKeys) &&
            animQuaternionKeysEqual(a.rotaionKeys, b.rotaionKeys) &&
            animVectorKeysEqual(a.scaleKeys, b.scaleKeys);
    }

    // Привязка клипа к костям → JSON
    blib::core::json::JsonValue animBoneChainToJson(_In const blib::graphics::AnimationClip::BoneChain& boneChain)
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();
        doc.set(boneChainKeyBoneIndex, blib::core::json::JsonValue(static_cast<buint64>(boneChain.boneIndex)));

        blib::core::json::JsonValue& elements =
            doc.set(boneChainKeyElements, blib::core::json::JsonValue::makeArray());
        for (const blib::graphics::AnimationClip::BoneChainElement& element : boneChain.elements)
        {
            blib::core::json::JsonValue elementObj = blib::core::json::JsonValue::makeObject();
            elementObj.set(elementKeyChannelIndex,
                blib::core::json::JsonValue(static_cast<buint64>(element.channelIndex)));
            elementObj.set(elementKeyBindTransform, animMatrixToArray(element.bindTransform));
            elements.pushBack(elementObj);
        }
        return doc;
    }

    // JSON → привязка. channelCount — число каналов клипа: индекс канала
    // либо < channelCount (сэмпл канала), либо == channelCount (bind-only)
    bool animReadBoneChain(_In const blib::core::json::JsonValue& v,
        _In buint32 channelCount,
        _Out blib::graphics::AnimationClip::BoneChain& out)
    {
        if (!v.isObject() ||
            !v.has(boneChainKeyBoneIndex) || !v.get(boneChainKeyBoneIndex).isNumber() ||
            !v.has(boneChainKeyElements))
        {
            return false;
        }

        blib::graphics::AnimationClip::BoneChain boneChain;
        boneChain.boneIndex = static_cast<size_t>(v.get(boneChainKeyBoneIndex).asBuint64());

        const blib::core::json::JsonValue& elementsArr = v.get(boneChainKeyElements);
        if (!elementsArr.isArray())
        {
            return false;
        }
        for (buint32 i = 0; i < elementsArr.size(); ++i)
        {
            const blib::core::json::JsonValue& elementObj = elementsArr[i];
            if (!elementObj.isObject() ||
                !elementObj.has(elementKeyChannelIndex) || !elementObj.get(elementKeyChannelIndex).isNumber() ||
                !elementObj.has(elementKeyBindTransform))
            {
                return false;
            }

            const buint64 channelIndex = elementObj.get(elementKeyChannelIndex).asBuint64();
            if (channelIndex > static_cast<buint64>(channelCount))
            {
                return false;
            }

            blib::graphics::AnimationClip::BoneChainElement element;
            element.channelIndex = static_cast<size_t>(channelIndex);
            if (!animReadMatrix(elementObj.get(elementKeyBindTransform), element.bindTransform))
            {
                return false;
            }
            boneChain.elements.push_back(element);
        }

        out = std::move(boneChain);
        return true;
    }

    bool animBoneChainsEqual(_In const std::vector<blib::graphics::AnimationClip::BoneChain>& a,
        _In const std::vector<blib::graphics::AnimationClip::BoneChain>& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i)
        {
            const blib::graphics::AnimationClip::BoneChain& chainA = a[i];
            const blib::graphics::AnimationClip::BoneChain& chainB = b[i];
            if (chainA.boneIndex != chainB.boneIndex ||
                chainA.elements.size() != chainB.elements.size())
            {
                return false;
            }
            for (size_t e = 0; e < chainA.elements.size(); ++e)
            {
                if (chainA.elements[e].channelIndex != chainB.elements[e].channelIndex ||
                    !animMatricesEqual(chainA.elements[e].bindTransform, chainB.elements[e].bindTransform))
                {
                    return false;
                }
            }
        }
        return true;
    }
}

blib::core::json::JsonValue blib::graphics::AnimationClip::toJson() const
{
    blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

    doc.set(clipKeyName, blib::core::json::JsonValue(this->name.c_str()));
    doc.set(clipKeyTickPerSecond, blib::core::json::JsonValue(this->tickPerSecond));
    doc.set(clipKeyDurationTicks, blib::core::json::JsonValue(this->durationTicks));
    doc.set(clipKeyDurationMs, blib::core::json::JsonValue(this->durationMs));
    doc.set(clipKeyCycled, blib::core::json::JsonValue(this->cycled));

    blib::core::json::JsonValue& channels =
        doc.set(clipKeyChannels, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::AnimationChannel& channel : this->channels)
    {
        channels.pushBack(animChannelToJson(channel));
    }

    blib::core::json::JsonValue& boneChains =
        doc.set(clipKeyBoneChains, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::AnimationClip::BoneChain& boneChain : this->boneChains)
    {
        boneChains.pushBack(animBoneChainToJson(boneChain));
    }

    return doc;
}

blib::core::LoadStatus blib::graphics::AnimationClip::fromJson(_In const blib::core::json::JsonValue& json)
{
    // Валидация формы ДО применения: при ошибке состояние не меняется
    if (!json.isObject() ||
        !json.has(clipKeyName) || !json.get(clipKeyName).isString() ||
        !json.has(clipKeyTickPerSecond) || !json.get(clipKeyTickPerSecond).isNumber() ||
        !json.has(clipKeyDurationTicks) || !json.get(clipKeyDurationTicks).isNumber() ||
        !json.has(clipKeyDurationMs) || !json.get(clipKeyDurationMs).isNumber() ||
        !json.has(clipKeyCycled) || !json.get(clipKeyCycled).isBool() ||
        !json.has(clipKeyChannels) || !json.has(clipKeyBoneChains))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Каналы
    std::vector<blib::graphics::AnimationChannel> channels;
    const blib::core::json::JsonValue& channelsArr = json.get(clipKeyChannels);
    if (!channelsArr.isArray())
    {
        return blib::core::LoadStatus::InvalidData;
    }
    for (buint32 i = 0; i < channelsArr.size(); ++i)
    {
        blib::graphics::AnimationChannel channel;
        if (!animReadChannel(channelsArr[i], channel))
        {
            return blib::core::LoadStatus::InvalidData;
        }
        channels.push_back(std::move(channel));
    }

    // Привязки к костям (индексы каналов проверяются против channels)
    std::vector<blib::graphics::AnimationClip::BoneChain> boneChains;
    const blib::core::json::JsonValue& boneChainsArr = json.get(clipKeyBoneChains);
    if (!boneChainsArr.isArray())
    {
        return blib::core::LoadStatus::InvalidData;
    }
    for (buint32 i = 0; i < boneChainsArr.size(); ++i)
    {
        blib::graphics::AnimationClip::BoneChain boneChain;
        if (!animReadBoneChain(boneChainsArr[i], static_cast<buint32>(channels.size()), boneChain))
        {
            return blib::core::LoadStatus::InvalidData;
        }
        boneChains.push_back(std::move(boneChain));
    }

    // Все данные валидны — применить
    this->name = json.get(clipKeyName).asString().c_str();
    this->tickPerSecond = json.get(clipKeyTickPerSecond).asBdouble();
    this->durationTicks = json.get(clipKeyDurationTicks).asBdouble();
    this->durationMs = json.get(clipKeyDurationMs).asBdouble();
    this->cycled = json.get(clipKeyCycled).asBool();
    this->channels = std::move(channels);
    this->boneChains = std::move(boneChains);

    return blib::core::LoadStatus::None;
}

blib::core::SaveStatus blib::graphics::AnimationClip::save(_In blib::core::IOutputStream& os) const
{
    const blib::core::json::JsonValue doc = this->toJson();
    if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
    {
        return blib::core::SaveStatus::WriteFailed;
    }
    return blib::core::SaveStatus::None;
}

blib::core::LoadStatus blib::graphics::AnimationClip::load(_In blib::core::IInputStream& is)
{
    blib::core::json::JsonParser parser;
    blib::core::json::JsonValue doc;
    if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
    {
        return blib::core::LoadStatus::InvalidData;
    }
    return this->fromJson(doc);
}

bool blib::graphics::AnimationClip::strongCompare(_In const blib::core::IStrongComparable& other,
    _In blib::core::CompareSession& session) const
{
    if (!session.enter(this, &other))
    {
        return true;
    }

    const blib::graphics::AnimationClip& o = static_cast<const blib::graphics::AnimationClip&>(other);

    if (this->name != o.name ||
        this->tickPerSecond != o.tickPerSecond ||
        this->durationTicks != o.durationTicks ||
        this->durationMs != o.durationMs ||
        this->cycled != o.cycled ||
        this->channels.size() != o.channels.size())
    {
        return false;
    }

    for (size_t i = 0; i < this->channels.size(); ++i)
    {
        if (!animChannelsEqual(this->channels[i], o.channels[i]))
        {
            return false;
        }
    }

    return animBoneChainsEqual(this->boneChains, o.boneChains);
}

bool blib::graphics::AnimationClip::verify() const
{
    // Round-trip без RTTI (см. blib::core::verifyRoundTrip)
    return blib::core::verifyRoundTrip(*this);
}

blib::core::json::JsonValue blib::graphics::Animator::toJson() const
{
    blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

    blib::core::json::JsonValue& clips =
        doc.set(animatorKeyClips, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::AnimationClip& clip : this->animationList)
    {
        clips.pushBack(clip.toJson());
    }

    doc.set(animatorKeyCurrentIndex, blib::core::json::JsonValue(static_cast<buint64>(this->currentAnimationIndex)));
    doc.set(animatorKeyTimeMs, blib::core::json::JsonValue(this->currentTimeMs));
    doc.set(animatorKeyIsPlaying, blib::core::json::JsonValue(this->isPlaying));

    return doc;
}

blib::core::LoadStatus blib::graphics::Animator::fromJson(_In const blib::core::json::JsonValue& json)
{
    // Валидация формы ДО применения: при ошибке состояние не меняется
    if (!json.isObject() ||
        !json.has(animatorKeyClips) || !json.get(animatorKeyClips).isArray() ||
        !json.has(animatorKeyCurrentIndex) || !json.get(animatorKeyCurrentIndex).isNumber() ||
        !json.has(animatorKeyTimeMs) || !json.get(animatorKeyTimeMs).isNumber() ||
        !json.has(animatorKeyIsPlaying) || !json.get(animatorKeyIsPlaying).isBool())
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Клипы
    std::vector<blib::graphics::AnimationClip> clips;
    const blib::core::json::JsonValue& clipsArr = json.get(animatorKeyClips);
    for (buint32 i = 0; i < clipsArr.size(); ++i)
    {
        blib::graphics::AnimationClip clip;
        if (__blib_unlikely(clip.fromJson(clipsArr[i]) != blib::core::LoadStatus::None))
        {
            return blib::core::LoadStatus::InvalidData;
        }
        clips.push_back(std::move(clip));
    }

    // Индекс текущего клипа обязан попадать в список (или указывать
    // «за концом» у пустого списка — как после выгрузки клипов)
    const buint64 currentIndex = json.get(animatorKeyCurrentIndex).asBuint64();
    if (__blib_unlikely(currentIndex > static_cast<buint64>(clips.size())))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Все данные валидны — применить
    this->animationList = std::move(clips);
    this->currentAnimationIndex = static_cast<size_t>(currentIndex);
    this->currentTimeMs = json.get(animatorKeyTimeMs).asBdouble();
    this->isPlaying = json.get(animatorKeyIsPlaying).asBool();

    return blib::core::LoadStatus::None;
}

blib::core::SaveStatus blib::graphics::Animator::save(_In blib::core::IOutputStream& os) const
{
    const blib::core::json::JsonValue doc = this->toJson();
    if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
    {
        return blib::core::SaveStatus::WriteFailed;
    }
    return blib::core::SaveStatus::None;
}

blib::core::LoadStatus blib::graphics::Animator::load(_In blib::core::IInputStream& is)
{
    blib::core::json::JsonParser parser;
    blib::core::json::JsonValue doc;
    if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
    {
        return blib::core::LoadStatus::InvalidData;
    }
    return this->fromJson(doc);
}

bool blib::graphics::Animator::strongCompare(_In const blib::core::IStrongComparable& other,
    _In blib::core::CompareSession& session) const
{
    if (!session.enter(this, &other))
    {
        return true;
    }

    const blib::graphics::Animator& o = static_cast<const blib::graphics::Animator&>(other);

    if (this->currentAnimationIndex != o.currentAnimationIndex ||
        this->currentTimeMs != o.currentTimeMs ||
        this->isPlaying != o.isPlaying ||
        this->animationList.size() != o.animationList.size())
    {
        return false;
    }

    for (size_t i = 0; i < this->animationList.size(); ++i)
    {
        if (!this->animationList[i].strongCompare(o.animationList[i], session))
        {
            return false;
        }
    }

    return true;
}

bool blib::graphics::Animator::verify() const
{
    // Round-trip без RTTI (см. blib::core::verifyRoundTrip)
    return blib::core::verifyRoundTrip(*this);
}
