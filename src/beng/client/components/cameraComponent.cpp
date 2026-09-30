#include <beng/client/components/cameraComponent.h>

#include <beng/core/scene.h>

#include <blib/core/json/json.h>
#include <blib/core/verifyHelper.h>

namespace beng
{
    namespace
    {
        // Ключи JSON-объекта компонента (формат save/load)
        constexpr const char* keyFovDegrees = "fovDegrees";
        constexpr const char* keyNearDistance = "nearDistance";
        constexpr const char* keyFarDistance = "farDistance";
        constexpr const char* keyPixelWidth = "pixelWidth";
        constexpr const char* keyPixelHeight = "pixelHeight";
        constexpr const char* keyIsActive = "isActive";

        // Дефолты компонента (Unity-подобные; кадр — 16:9)
        constexpr bfloat defaultFovDegrees = 60.0f;
        constexpr bfloat defaultNearDistance = 0.1f;
        constexpr bfloat defaultFarDistance = 1000.0f;
        constexpr buint32 defaultPixelWidth = 1280;
        constexpr buint32 defaultPixelHeight = 720;

        // Монотонный счётчик активаций (процесс): штампы сравниваются
        // CameraSystem'ом. Сценарий — однопоточный главный цикл
        // эдитора/клиента: потокобезопасность не требуется
        buint64 s_activationCounter = 0;
    }

    CameraComponent::CameraComponent()
        : fovDegrees(defaultFovDegrees)
        , nearDistance(defaultNearDistance)
        , farDistance(defaultFarDistance)
        , pixelWidth(defaultPixelWidth)
        , pixelHeight(defaultPixelHeight)
        , activationStamp(++s_activationCounter)
    {
        // IComponent-база: isActive = true (камера активна по умолчанию)
    }

    void CameraComponent::setActive(bool active)
    {
        if (active && !isActive)
        {
            // Свежий штамп: «последняя включённая побеждает» —
            // CameraSystem погасит остальные активные камеры сцены
            activationStamp = ++s_activationCounter;
        }
        isActive = active;
    }

    void CameraComponent::onLoaded(_In Scene& scene)
    {
        // Контекст: компонент камеры сцену не использует — вызов нужен
        // только для штампа активации при загрузке файла. Активные
        // камеры получают штампы в порядке файла: при нескольких
        // активных побеждает последняя (консистентно с setActive)
        (void)scene;
        if (isActive)
        {
            activationStamp = ++s_activationCounter;
        }
    }

    blib::core::SaveStatus CameraComponent::save(_In blib::core::IOutputStream& os) const
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

        doc.set(keyFovDegrees, blib::core::json::JsonValue(fovDegrees));
        doc.set(keyNearDistance, blib::core::json::JsonValue(nearDistance));
        doc.set(keyFarDistance, blib::core::json::JsonValue(farDistance));
        doc.set(keyPixelWidth, blib::core::json::JsonValue(pixelWidth));
        doc.set(keyPixelHeight, blib::core::json::JsonValue(pixelHeight));
        doc.set(keyIsActive, blib::core::json::JsonValue(isActive));

        if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "CameraComponent: failed to write JSON to stream");
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus CameraComponent::load(_In blib::core::IInputStream& is)
    {
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "CameraComponent: failed to parse JSON from stream");
        }

        // Валидация всех полей до применения (при ошибке состояние
        // не меняется)
        bfloat loadedFov;
        bfloat loadedNear;
        bfloat loadedFar;
        buint32 loadedWidth;
        buint32 loadedHeight;
        bool loadedActive;
        if (__blib_unlikely(!doc.isObject()) ||
            __blib_unlikely(!doc.has(keyFovDegrees) || !doc.get(keyFovDegrees).isNumber()) ||
            __blib_unlikely(!doc.has(keyNearDistance) || !doc.get(keyNearDistance).isNumber()) ||
            __blib_unlikely(!doc.has(keyFarDistance) || !doc.get(keyFarDistance).isNumber()) ||
            __blib_unlikely(!doc.has(keyPixelWidth) || !doc.get(keyPixelWidth).isNumber()) ||
            __blib_unlikely(!doc.has(keyPixelHeight) || !doc.get(keyPixelHeight).isNumber()) ||
            __blib_unlikely(!doc.has(keyIsActive) || !doc.get(keyIsActive).isBool()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "CameraComponent: missing or malformed field");
        }
        loadedFov = doc.get(keyFovDegrees).asBfloat();
        loadedNear = doc.get(keyNearDistance).asBfloat();
        loadedFar = doc.get(keyFarDistance).asBfloat();
        // JSON-числа — bfloat: целые значения разрешения читаются
        // приведением (до миллиона пикселей — точно)
        loadedWidth = static_cast<buint32>(doc.get(keyPixelWidth).asBfloat());
        loadedHeight = static_cast<buint32>(doc.get(keyPixelHeight).asBfloat());
        loadedActive = doc.get(keyIsActive).asBool();

        // Вырожденные параметры не принимаем: нулевое разрешение
        // и неверный порядок плоскостей ломают проекцию потребителя
        if (__blib_unlikely(loadedWidth < 1 || loadedHeight < 1 ||
            loadedFov <= 0.0f || loadedNear <= 0.0f || loadedFar <= loadedNear))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "CameraComponent: invalid camera parameters");
        }

        fovDegrees = loadedFov;
        nearDistance = loadedNear;
        farDistance = loadedFar;
        pixelWidth = loadedWidth;
        pixelHeight = loadedHeight;
        isActive = loadedActive;

        return blib::core::LoadStatus::None;
    }

    bool CameraComponent::strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const
    {
        // Защита от циклов
        if (!session.enter(this, &other))
        {
            return true;
        }

        const CameraComponent& o = static_cast<const CameraComponent&>(other);

        // Базовые поля + собственные данные (бит-в-бит). Штамп —
        // контекст, не сравнивается
        return strongCompareBase(o) &&
            fovDegrees == o.fovDegrees &&
            nearDistance == o.nearDistance &&
            farDistance == o.farDistance &&
            pixelWidth == o.pixelWidth &&
            pixelHeight == o.pixelHeight;
    }

    bool CameraComponent::verify() const
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
        constexpr const char* reflectionFieldFov = "fovDegrees";
        constexpr const char* reflectionFieldNear = "nearDistance";
        constexpr const char* reflectionFieldFar = "farDistance";
        constexpr const char* reflectionFieldPixelWidth = "pixelWidth";
        constexpr const char* reflectionFieldPixelHeight = "pixelHeight";
        constexpr const char* reflectionFieldActive = "active";

        const FunctionField s_fovField(
            reflectionFieldFov, FieldValue::Kind::Float,
            [](_In const IComponent& component, _Out FieldValue& outValue)
            {
                outValue = FieldValue::fromFloat(
                    static_cast<const CameraComponent&>(component).getFovDegrees());
            },
            [](_In IComponent& component, _In const FieldValue& value)
            {
                static_cast<CameraComponent&>(component).setFovDegrees(value.floatValue);
            });

        const FunctionField s_nearField(
            reflectionFieldNear, FieldValue::Kind::Float,
            [](_In const IComponent& component, _Out FieldValue& outValue)
            {
                outValue = FieldValue::fromFloat(
                    static_cast<const CameraComponent&>(component).getNearDistance());
            },
            [](_In IComponent& component, _In const FieldValue& value)
            {
                static_cast<CameraComponent&>(component).setNearDistance(value.floatValue);
            });

        const FunctionField s_farField(
            reflectionFieldFar, FieldValue::Kind::Float,
            [](_In const IComponent& component, _Out FieldValue& outValue)
            {
                outValue = FieldValue::fromFloat(
                    static_cast<const CameraComponent&>(component).getFarDistance());
            },
            [](_In IComponent& component, _In const FieldValue& value)
            {
                static_cast<CameraComponent&>(component).setFarDistance(value.floatValue);
            });

        const FunctionField s_pixelWidthField(
            reflectionFieldPixelWidth, FieldValue::Kind::Int,
            [](_In const IComponent& component, _Out FieldValue& outValue)
            {
                outValue = FieldValue::fromInt(
                    static_cast<bint32>(static_cast<const CameraComponent&>(component).getPixelWidth()));
            },
            [](_In IComponent& component, _In const FieldValue& value)
            {
                if (value.intValue > 0)
                {
                    static_cast<CameraComponent&>(component).setPixelWidth(
                        static_cast<buint32>(value.intValue));
                }
            });

        const FunctionField s_pixelHeightField(
            reflectionFieldPixelHeight, FieldValue::Kind::Int,
            [](_In const IComponent& component, _Out FieldValue& outValue)
            {
                outValue = FieldValue::fromInt(
                    static_cast<bint32>(static_cast<const CameraComponent&>(component).getPixelHeight()));
            },
            [](_In IComponent& component, _In const FieldValue& value)
            {
                if (value.intValue > 0)
                {
                    static_cast<CameraComponent&>(component).setPixelHeight(
                        static_cast<buint32>(value.intValue));
                }
            });

        const FunctionField s_activeField(
            reflectionFieldActive, FieldValue::Kind::Bool,
            [](_In const IComponent& component, _Out FieldValue& outValue)
            {
                outValue = FieldValue::fromBool(
                    static_cast<const CameraComponent&>(component).getActive());
            },
            [](_In IComponent& component, _In const FieldValue& value)
            {
                static_cast<CameraComponent&>(component).setActive(value.boolValue);
            });

        const IComponentField* const s_cameraFields[] = {
            &s_fovField,
            &s_nearField,
            &s_farField,
            &s_pixelWidthField,
            &s_pixelHeightField,
            &s_activeField
        };

        constexpr buint32 s_cameraFieldCount =
            static_cast<buint32>(sizeof(s_cameraFields) / sizeof(s_cameraFields[0]));

        const ComponentTypeDescriptor s_cameraReflection(
            CameraComponent::componentTypeName, s_cameraFields, s_cameraFieldCount);
    }

    const ComponentTypeDescriptor& CameraComponent::componentReflection()
    {
        return s_cameraReflection;
    }

} // namespace beng
