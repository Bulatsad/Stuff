#pragma once

#include <string>

#include <beng/config.h>
#include <beng/core/icomponent.h>

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
        // Стабильное имя типа — идентичность типа в таблице типов Scene
        // (регистрация, резолв в шаблонных методах, save/load)
        static constexpr const char* componentTypeName = "beng.BlobShadow";

        // Не прятать 1-аргументную точку входа строгого сравнения
        using blib::core::IStrongComparable::strongCompare;

        BlobShadowComponent(EntityID target, _In const std::string& boneName, float groundOffset);

        /**
         * Конструктор по умолчанию: target = invalidEntity, пустое имя
         * кости, нулевое смещение. Нужен verifyRoundTrip (ISaveLoadable::verify).
         */
        BlobShadowComponent();

        ~BlobShadowComponent() override = default;

        BlobShadowComponent(const BlobShadowComponent&) = delete;
        BlobShadowComponent& operator=(const BlobShadowComponent&) = delete;

        EntityID getTargetEntity() const;
        const std::string& getBoneName() const;
        float getGroundOffset() const;

        bool isBoneMissingLogged() const;
        void setBoneMissingLogged();

        // ========== ISaveLoadable: сериализация и сравнение ==========
        //
        // Формат save: JSON-объект с полями компонента (targetEntity,
        // boneName, groundOffset, boneMissingLogged, isActive). Контекста
        // нет — round-trip верифицируется standalone (verify() = true).

        /**
         * Сохранить состояние компонента в поток (JSON-объект).
         */
        blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override;

        /**
         * Загрузить состояние компонента из потока (JSON-объект).
         */
        blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override;

        /**
         * Строгое (бит-в-бит) сравнение: базовые поля + все данные.
         */
        bool strongCompare(_In const blib::core::IStrongComparable& other,
            _In blib::core::CompareSession& session) const __blib_override;

        /**
         * Round-trip валидация (verifyRoundTrip).
         */
        bool verify() const __blib_override;
    };

} // namespace beng
