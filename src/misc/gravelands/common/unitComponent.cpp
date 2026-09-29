#include <gravelands/common/unitComponent.h>

#include <gravelands/common/config.h>

#include <blib/core/json/json.h>
#include <blib/core/verifyHelper.h>

namespace gravelands
{
    namespace
    {
        // Ключи JSON-объекта компонента (формат save/load)
        constexpr const char* keyMoveSpeed = "moveSpeed";
        constexpr const char* keyIsPlayer = "isPlayer";
        constexpr const char* keyMoveX = "moveX";
        constexpr const char* keyMoveZ = "moveZ";
        constexpr const char* keyIsActive = "isActive";
    }

    UnitComponent::UnitComponent()
        : moveSpeed(playerMoveSpeed)
        , isPlayer(false)
        , moveX(0)
        , moveZ(0)
    {
    }

    blib::core::SaveStatus UnitComponent::save(_In blib::core::IOutputStream& os) const
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();
        doc.set(keyMoveSpeed, blib::core::json::JsonValue(moveSpeed));
        doc.set(keyIsPlayer, blib::core::json::JsonValue(isPlayer));
        doc.set(keyMoveX, blib::core::json::JsonValue(static_cast<bint64>(moveX)));
        doc.set(keyMoveZ, blib::core::json::JsonValue(static_cast<bint64>(moveZ)));
        doc.set(keyIsActive, blib::core::json::JsonValue(isActive));

        if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "UnitComponent: failed to write JSON to stream");
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus UnitComponent::load(_In blib::core::IInputStream& is)
    {
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "UnitComponent: failed to parse JSON from stream");
        }

        // Валидация всех полей до применения (при ошибке состояние не меняется)
        bfloat loadedMoveSpeed = 0.0f;
        bool loadedIsPlayer = false;
        bint8 loadedMoveX = 0;
        bint8 loadedMoveZ = 0;
        bool loadedActive = false;
        if (__blib_unlikely(!doc.isObject()) ||
            __blib_unlikely(!doc.has(keyMoveSpeed) || !doc.get(keyMoveSpeed).isNumber()) ||
            __blib_unlikely(!doc.has(keyIsPlayer) || !doc.get(keyIsPlayer).isBool()) ||
            __blib_unlikely(!doc.has(keyMoveX) || !doc.get(keyMoveX).isNumber()) ||
            __blib_unlikely(!doc.has(keyMoveZ) || !doc.get(keyMoveZ).isNumber()) ||
            __blib_unlikely(!doc.has(keyIsActive) || !doc.get(keyIsActive).isBool()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "UnitComponent: missing or malformed field");
        }

        loadedMoveSpeed = doc.get(keyMoveSpeed).asBfloat();
        loadedIsPlayer = doc.get(keyIsPlayer).asBool();
        loadedMoveX = static_cast<bint8>(doc.get(keyMoveX).asBint64());
        loadedMoveZ = static_cast<bint8>(doc.get(keyMoveZ).asBint64());
        loadedActive = doc.get(keyIsActive).asBool();

        moveSpeed = loadedMoveSpeed;
        isPlayer = loadedIsPlayer;
        moveX = loadedMoveX;
        moveZ = loadedMoveZ;
        isActive = loadedActive;

        return blib::core::LoadStatus::None;
    }

    bool UnitComponent::strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const
    {
        if (!session.enter(this, &other))
        {
            return true;
        }

        const UnitComponent& o = static_cast<const UnitComponent&>(other);

        // Базовые поля + собственные данные (бит-в-бит)
        return strongCompareBase(o) &&
            moveSpeed == o.moveSpeed &&
            isPlayer == o.isPlayer &&
            moveX == o.moveX &&
            moveZ == o.moveZ;
    }

    bool UnitComponent::verify() const
    {
        // Round-trip без RTTI (см. blib::core::verifyRoundTrip)
        return blib::core::verifyRoundTrip(*this);
    }

    // ========== Рефлексия (Inspector/эдитор) ==========
    //
    // Статические поля-дескрипторы: геттеры/сеттеры — лямбды без
    // захвата через публичные API компонента. Дескриптор и поля
    // живут всё время процесса; сцена хранит указатель на дескриптор
    // (см. Scene::registerComponentType).

    namespace
    {
        constexpr const char* reflectionFieldMoveSpeed = "moveSpeed";
        constexpr const char* reflectionFieldIsPlayer = "isPlayer";

        const beng::FunctionField s_moveSpeedField(
            reflectionFieldMoveSpeed, beng::FieldValue::Kind::Float,
            [](_In const beng::IComponent& component, _Out beng::FieldValue& outValue)
            {
                outValue = beng::FieldValue::fromFloat(
                    static_cast<const UnitComponent&>(component).getMoveSpeed());
            },
            [](_In beng::IComponent& component, _In const beng::FieldValue& value)
            {
                static_cast<UnitComponent&>(component).setMoveSpeed(value.floatValue);
            });

        const beng::FunctionField s_isPlayerField(
            reflectionFieldIsPlayer, beng::FieldValue::Kind::Bool,
            [](_In const beng::IComponent& component, _Out beng::FieldValue& outValue)
            {
                outValue = beng::FieldValue::fromBool(
                    static_cast<const UnitComponent&>(component).getIsPlayer());
            },
            [](_In beng::IComponent& component, _In const beng::FieldValue& value)
            {
                static_cast<UnitComponent&>(component).setIsPlayer(value.boolValue);
            });

        const beng::IComponentField* const s_unitFields[] = {
            &s_moveSpeedField,
            &s_isPlayerField
        };

        constexpr buint32 s_unitFieldCount =
            static_cast<buint32>(sizeof(s_unitFields) / sizeof(s_unitFields[0]));

        const beng::ComponentTypeDescriptor s_unitReflection(
            UnitComponent::componentTypeName, s_unitFields, s_unitFieldCount);
    }

    const beng::ComponentTypeDescriptor& UnitComponent::componentReflection()
    {
        return s_unitReflection;
    }

} // namespace gravelands
