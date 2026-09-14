#pragma once

#include <string>

#include <beng/config.h>
#include <beng/core/component.h>

namespace beng
{
    /**
     * BlobShadowComponent — привязка blob-тени к анимируемой модели.
     *
     * Назначение:
     * - Вешается на сущность-тень (у которой есть TransformComponent
     *   и MeshRenderComponent со слоем Shadow);
     * - BlobShadowSystem каждый кадр читает позицию кости цели
     *   (root-motion анимации) и двигает сущность-тень под неё,
     *   проецируя смещение вдоль направления света на землю.
     *
     * Использование:
     *   scene.addComponent<BlobShadowComponent>(shadowEntity, dancerEntity, "Hips", 0.5f);
     */
    class __beng_api BlobShadowComponent : public beng::IComponent
    {
    private:
        EntityID targetEntity;
        std::string boneName;
        float groundOffset;

        // INTERNAL: выставляется системой при первом неудачном
        // поиске кости — защита от спама warning'ов каждый кадр
        bool boneMissingLogged;

    public:
        BlobShadowComponent(EntityID target, _In const std::string& boneName, float groundOffset);
        ~BlobShadowComponent() override = default;

        BlobShadowComponent(const BlobShadowComponent&) = delete;
        BlobShadowComponent& operator=(const BlobShadowComponent&) = delete;

        EntityID getTargetEntity() const;
        const std::string& getBoneName() const;
        float getGroundOffset() const;

        bool isBoneMissingLogged() const;
        void setBoneMissingLogged();
    };

} // namespace beng
