#include <beng/client/components/blobShadowComponent.h>

#include <blib/core/json/json.h>
#include <blib/core/verifyHelper.h>

namespace beng
{
    namespace
    {
        // Ключи JSON-объекта компонента (формат save/load)
        constexpr const char* keyTargetEntity = "targetEntity";
        constexpr const char* keyBoneName = "boneName";
        constexpr const char* keyGroundOffset = "groundOffset";
        constexpr const char* keyBoneMissingLogged = "boneMissingLogged";
        constexpr const char* keyIsActive = "isActive";
    }

    BlobShadowComponent::BlobShadowComponent(EntityID target, _In const std::string& bone, float groundOffset)
        : targetEntity(target)
        , boneName(bone)
        , groundOffset(groundOffset)
        , boneMissingLogged(false)
    {
    }

    BlobShadowComponent::BlobShadowComponent()
        : BlobShadowComponent(invalidEntity, std::string(), 0.0f)
    {
    }

    EntityID BlobShadowComponent::getTargetEntity() const
    {
        return this->targetEntity;
    }

    const std::string& BlobShadowComponent::getBoneName() const
    {
        return this->boneName;
    }

    float BlobShadowComponent::getGroundOffset() const
    {
        return this->groundOffset;
    }

    bool BlobShadowComponent::isBoneMissingLogged() const
    {
        return this->boneMissingLogged;
    }

    void BlobShadowComponent::setBoneMissingLogged()
    {
        this->boneMissingLogged = true;
    }

    blib::core::SaveStatus BlobShadowComponent::save(_In blib::core::IOutputStream& os) const
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

        doc.set(keyTargetEntity, blib::core::json::JsonValue(targetEntity));
        doc.set(keyBoneName, blib::core::json::JsonValue(boneName.c_str()));
        doc.set(keyGroundOffset, blib::core::json::JsonValue(groundOffset));
        doc.set(keyBoneMissingLogged, blib::core::json::JsonValue(boneMissingLogged));
        doc.set(keyIsActive, blib::core::json::JsonValue(isActive));

        if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "BlobShadowComponent: failed to write JSON to stream");
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus BlobShadowComponent::load(_In blib::core::IInputStream& is)
    {
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "BlobShadowComponent: failed to parse JSON from stream");
        }

        // Валидация всех полей до применения (при ошибке состояние не меняется)
        if (__blib_unlikely(!doc.isObject()) ||
            __blib_unlikely(!doc.has(keyTargetEntity) || !doc.get(keyTargetEntity).isNumber()) ||
            __blib_unlikely(!doc.has(keyBoneName) || !doc.get(keyBoneName).isString()) ||
            __blib_unlikely(!doc.has(keyGroundOffset) || !doc.get(keyGroundOffset).isNumber()) ||
            __blib_unlikely(!doc.has(keyBoneMissingLogged) || !doc.get(keyBoneMissingLogged).isBool()) ||
            __blib_unlikely(!doc.has(keyIsActive) || !doc.get(keyIsActive).isBool()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "BlobShadowComponent: missing or malformed field");
        }

        targetEntity = static_cast<EntityID>(doc.get(keyTargetEntity).asBuint64());
        boneName = doc.get(keyBoneName).asString().c_str();
        groundOffset = doc.get(keyGroundOffset).asBfloat();
        boneMissingLogged = doc.get(keyBoneMissingLogged).asBool();
        isActive = doc.get(keyIsActive).asBool();

        return blib::core::LoadStatus::None;
    }

    bool BlobShadowComponent::strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const
    {
        // Защита от циклов
        if (!session.enter(this, &other))
        {
            return true;
        }

        const BlobShadowComponent& o = static_cast<const BlobShadowComponent&>(other);

        // Базовые поля + собственные данные (бит-в-бит)
        return getOwnerId() == o.getOwnerId() &&
            isActive == o.isActive &&
            targetEntity == o.targetEntity &&
            boneName == o.boneName &&
            groundOffset == o.groundOffset &&
            boneMissingLogged == o.boneMissingLogged;
    }

    bool BlobShadowComponent::verify() const
    {
        // Round-trip без RTTI (см. blib::core::verifyRoundTrip)
        return blib::core::verifyRoundTrip(*this);
    }

} // namespace beng
