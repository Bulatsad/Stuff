#include <beng/client/components/blobShadowComponent.h>

namespace beng
{
    BlobShadowComponent::BlobShadowComponent(EntityID target, _In const std::string& bone, float groundOffset)
        : targetEntity(target)
        , boneName(bone)
        , groundOffset(groundOffset)
        , boneMissingLogged(false)
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

} // namespace beng
