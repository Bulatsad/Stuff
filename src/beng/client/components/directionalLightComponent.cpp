#include <beng/client/components/directionalLightComponent.h>

#include <blib/core/json/json.h>
#include <blib/core/verifyHelper.h>

namespace beng
{
    namespace
    {
        // Ключи JSON-объекта компонента (формат save/load)
        constexpr const char* keyDirection = "direction";
        constexpr const char* keyColor = "color";
        constexpr const char* keyIntensity = "intensity";
        constexpr const char* keyIsActive = "isActive";

        // Размерность Vector3 в JSON (массив из трёх чисел)
        constexpr buint32 vector3Size = 3;

        // Дефолты как у RenderContext (мягкий тёплый «солнечный» свет —
        // см. blib/graphics/impl/rendercontext.cpp)
        constexpr bfloat defaultDirectionX = 0.4f;
        constexpr bfloat defaultDirectionY = -0.8f;
        constexpr bfloat defaultDirectionZ = 0.3f;
        constexpr bfloat defaultColorR = 1.0f;
        constexpr bfloat defaultColorG = 0.95f;
        constexpr bfloat defaultColorB = 0.85f;
        constexpr bfloat defaultIntensity = 0.9f;

        // Запись Vector3 в JSON-объект (массив [x, y, z])
        void setVector3(_In blib::core::json::JsonValue& doc, _In const char* key,
            _In const blib::math::Vector<float, 3>& value)
        {
            blib::core::json::JsonValue& arr = doc.set(key, blib::core::json::JsonValue::makeArray());
            arr.pushBack(blib::core::json::JsonValue(value.x));
            arr.pushBack(blib::core::json::JsonValue(value.y));
            arr.pushBack(blib::core::json::JsonValue(value.z));
        }

        // Чтение Vector3 из JSON-объекта (массив ровно из трёх чисел)
        bool readVector3(_In const blib::core::json::JsonValue& doc, _In const char* key,
            _Out blib::math::Vector<float, 3>& out)
        {
            if (!doc.has(key))
            {
                return false;
            }
            const blib::core::json::JsonValue& arr = doc.get(key);
            if (!arr.isArray() || arr.size() != vector3Size)
            {
                return false;
            }
            out = blib::math::Vector<float, 3>(
                arr[0].asBfloat(), arr[1].asBfloat(), arr[2].asBfloat());
            return true;
        }
    }

    DirectionalLightComponent::DirectionalLightComponent()
        : direction(defaultDirectionX, defaultDirectionY, defaultDirectionZ)
        , color(defaultColorR, defaultColorG, defaultColorB)
        , intensity(defaultIntensity)
    {
    }

    blib::core::SaveStatus DirectionalLightComponent::save(_In blib::core::IOutputStream& os) const
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

        setVector3(doc, keyDirection, direction);
        setVector3(doc, keyColor, color);
        doc.set(keyIntensity, blib::core::json::JsonValue(intensity));
        doc.set(keyIsActive, blib::core::json::JsonValue(isActive));

        if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "DirectionalLightComponent: failed to write JSON to stream");
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus DirectionalLightComponent::load(_In blib::core::IInputStream& is)
    {
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "DirectionalLightComponent: failed to parse JSON from stream");
        }

        // Валидация всех полей до применения (при ошибке состояние не меняется)
        blib::math::Vector<float, 3> loadedDirection;
        blib::math::Vector<float, 3> loadedColor;
        bfloat loadedIntensity;
        bool loadedActive;
        if (__blib_unlikely(!doc.isObject()) ||
            __blib_unlikely(!readVector3(doc, keyDirection, loadedDirection)) ||
            __blib_unlikely(!readVector3(doc, keyColor, loadedColor)) ||
            __blib_unlikely(!doc.has(keyIntensity) || !doc.get(keyIntensity).isNumber()) ||
            __blib_unlikely(!doc.has(keyIsActive) || !doc.get(keyIsActive).isBool()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "DirectionalLightComponent: missing or malformed field");
        }
        loadedIntensity = doc.get(keyIntensity).asBfloat();
        loadedActive = doc.get(keyIsActive).asBool();

        direction = loadedDirection;
        color = loadedColor;
        intensity = loadedIntensity;
        isActive = loadedActive;

        return blib::core::LoadStatus::None;
    }

    bool DirectionalLightComponent::strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const
    {
        // Защита от циклов
        if (!session.enter(this, &other))
        {
            return true;
        }

        const DirectionalLightComponent& o = static_cast<const DirectionalLightComponent&>(other);

        // Базовые поля + собственные данные (бит-в-бит)
        return strongCompareBase(o) &&
            direction == o.direction &&
            color == o.color &&
            intensity == o.intensity;
    }

    bool DirectionalLightComponent::verify() const
    {
        // Round-trip без RTTI (см. blib::core::verifyRoundTrip)
        return blib::core::verifyRoundTrip(*this);
    }

} // namespace beng
