#include <blib/test/src/test.h>

#include <beng/core/componentReflection.h>
#include <beng/core/scene.h>
#include <beng/components/transform.h>

#include <vector>

// Тестовый компонент с рефлексией (одно float-поле): проверка
// интеграции registerComponentType → дескриптор в сцене
struct ReflectionTestComponent : public beng::IComponent
{
    static constexpr const char* componentTypeName = "test.Reflection";

    using blib::core::IStrongComparable::strongCompare;

    ReflectionTestComponent()
        : speed(1.5f)
    {
    }

    explicit ReflectionTestComponent(float v)
        : speed(v)
    {
    }

    bool strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const __blib_override
    {
        if (!session.enter(this, &other))
        {
            return true;
        }
        const ReflectionTestComponent& o = static_cast<const ReflectionTestComponent&>(other);
        return getOwnerId() == o.getOwnerId() && isActive == o.isActive && speed == o.speed;
    }

    // Не сериализуем (save/load Unsupported) — round-trip неприменим
    bool verify() const __blib_override
    {
        return false;
    }

    float getSpeed() const { return speed; }
    void setSpeed(float v) { speed = v; }

    static const beng::ComponentTypeDescriptor& componentReflection();

private:
    float speed;
};

namespace
{
    // Поля рефлексии тестового компонента (геттер/сеттер — лямбды
    // без захвата, паттерн FunctionField)
    const beng::FunctionField s_speedField(
        "speed", beng::FieldValue::Kind::Float,
        [](_In const beng::IComponent& component, _Out beng::FieldValue& outValue)
        {
            outValue = beng::FieldValue::fromFloat(
                static_cast<const ReflectionTestComponent&>(component).getSpeed());
        },
        [](_In beng::IComponent& component, _In const beng::FieldValue& value)
        {
            static_cast<ReflectionTestComponent&>(component).setSpeed(value.floatValue);
        });

    const beng::IComponentField* const s_testFields[] = { &s_speedField };

    const beng::ComponentTypeDescriptor s_testDescriptor(
        ReflectionTestComponent::componentTypeName, s_testFields, 1);
}

const beng::ComponentTypeDescriptor& ReflectionTestComponent::componentReflection()
{
    return s_testDescriptor;
}

BLIB_TEST_CASE("reflection: HasComponentReflection trait")
{
    static_assert(beng::HasComponentReflection<beng::TransformComponent>::value,
        "TransformComponent must provide componentReflection()");
    static_assert(beng::HasComponentReflection<ReflectionTestComponent>::value,
        "ReflectionTestComponent must provide componentReflection()");

    BLIB_TEST_CHECK(true);
}

BLIB_TEST_CASE("reflection: TransformComponent descriptor fields")
{
    const beng::ComponentTypeDescriptor& descriptor =
        beng::TransformComponent::componentReflection();

    // Сравнение по СОДЕРЖИМОМУ: литералы разных модулей (exe/DLL в
    // shared-сборке) имеют разные адреса
    BLIB_TEST_CHECK(std::strcmp(descriptor.getTypeName(), beng::TransformComponent::componentTypeName) == 0);
    BLIB_TEST_CHECK(descriptor.getFieldCount() == 3);

    const beng::IComponentField* positionField = descriptor.getField(0);
    const beng::IComponentField* scaleField = descriptor.getField(1);
    const beng::IComponentField* parentField = descriptor.getField(2);
    BLIB_TEST_CHECK(positionField != nullptr);
    BLIB_TEST_CHECK(scaleField != nullptr);
    BLIB_TEST_CHECK(parentField != nullptr);
    BLIB_TEST_CHECK(positionField->getName() == std::string("position"));
    BLIB_TEST_CHECK(scaleField->getName() == std::string("scale"));
    BLIB_TEST_CHECK(parentField->getName() == std::string("parent"));
    BLIB_TEST_CHECK(positionField->getKind() == beng::FieldValue::Kind::Vector3);
    BLIB_TEST_CHECK(scaleField->getKind() == beng::FieldValue::Kind::Vector3);
    BLIB_TEST_CHECK(parentField->getKind() == beng::FieldValue::Kind::Entity);

    // За границей — nullptr
    BLIB_TEST_CHECK(descriptor.getField(3) == nullptr);
}

BLIB_TEST_CASE("reflection: field get/set roundtrip via IComponentField")
{
    // TransformComponent: position через дескриптор
    beng::Scene scene;
    beng::EntityID entity = scene.createEntity();
    beng::TransformComponent& transform = scene.getComponent<beng::TransformComponent>(entity);

    const beng::ComponentTypeDescriptor& descriptor =
        beng::TransformComponent::componentReflection();
    const beng::IComponentField* positionField = descriptor.getField(0);

    // Чтение: дефолт (0, 0, 0)
    beng::FieldValue value;
    positionField->getValue(transform, value);
    BLIB_TEST_CHECK(value.kind == beng::FieldValue::Kind::Vector3);
    BLIB_TEST_CHECK_CLOSE(value.vector3Value[0], 0.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(value.vector3Value[1], 0.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(value.vector3Value[2], 0.0f, 0.001f);

    // Запись: поле применяет значение компоненту
    value.vector3Value = blib::math::Vector<float, 3>(10.0f, 20.0f, 30.0f);
    positionField->setValue(transform, value);

    const blib::math::Vector<float, 3>& pos = transform.getLocalPosition();
    BLIB_TEST_CHECK_CLOSE(pos.x, 10.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(pos.y, 20.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(pos.z, 30.0f, 0.001f);

    // Roundtrip: чтение возвращает записанное
    beng::FieldValue readBack;
    positionField->getValue(transform, readBack);
    BLIB_TEST_CHECK_CLOSE(readBack.vector3Value[0], 10.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(readBack.vector3Value[1], 20.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(readBack.vector3Value[2], 30.0f, 0.001f);
}

BLIB_TEST_CASE("reflection: float field roundtrip via IComponentField")
{
    ReflectionTestComponent component(2.5f);

    const beng::ComponentTypeDescriptor& descriptor = ReflectionTestComponent::componentReflection();
    const beng::IComponentField* speedField = descriptor.getField(0);
    BLIB_TEST_CHECK(speedField != nullptr);
    BLIB_TEST_CHECK(speedField->getKind() == beng::FieldValue::Kind::Float);

    beng::FieldValue value;
    speedField->getValue(component, value);
    BLIB_TEST_CHECK_CLOSE(value.floatValue, 2.5f, 0.001f);

    value.floatValue = 7.25f;
    speedField->setValue(component, value);
    BLIB_TEST_CHECK_CLOSE(component.getSpeed(), 7.25f, 0.001f);
}

BLIB_TEST_CASE("reflection: entity (parent) field roundtrip via IComponentField")
{
    beng::Scene scene;
    beng::EntityID parent = scene.createEntity();
    beng::EntityID child = scene.createEntity();

    const beng::ComponentTypeDescriptor& descriptor =
        beng::TransformComponent::componentReflection();
    const beng::IComponentField* parentField = descriptor.getField(2);

    // Чтение: родителя нет (invalidEntity)
    beng::FieldValue value;
    parentField->getValue(scene.getComponent<beng::TransformComponent>(child), value);
    BLIB_TEST_CHECK(value.kind == beng::FieldValue::Kind::Entity);
    BLIB_TEST_CHECK(value.entityValue == beng::invalidEntity);

    // Запись: setParent (компонент в сцене — ownerScene валиден,
    // иерархия обновляется)
    value.entityValue = parent;
    parentField->setValue(scene.getComponent<beng::TransformComponent>(child), value);

    BLIB_TEST_CHECK(scene.getComponent<beng::TransformComponent>(child).getParent() == parent);
    BLIB_TEST_CHECK(scene.getComponent<beng::TransformComponent>(parent).getChildren().size() == 1);
}

BLIB_TEST_CASE("reflection: scene stores descriptors per component type")
{
    beng::Scene scene;

    // Transform — typeId 0 (инвариант сцены): дескриптор уже в сцене
    const beng::ComponentTypeDescriptor* transformDescriptor =
        scene.tryGetComponentReflection(0);
    BLIB_TEST_CHECK(transformDescriptor != nullptr);
    BLIB_TEST_CHECK(transformDescriptor->getTypeName() == std::string("beng.Transform"));

    // Незарегистрированный тип — nullptr (не fatal)
    BLIB_TEST_CHECK(scene.tryGetComponentReflection(1) == nullptr);

    // После регистрации компонента с рефлексией — дескриптор в сцене
    scene.registerComponentType<ReflectionTestComponent>();

    const beng::ComponentTypeDescriptor* testDescriptor = scene.tryGetComponentReflection(1);
    BLIB_TEST_CHECK(testDescriptor != nullptr);
    BLIB_TEST_CHECK(testDescriptor->getTypeName() == std::string("test.Reflection"));
    BLIB_TEST_CHECK(testDescriptor->getFieldCount() == 1);
}

BLIB_TEST_CASE("reflection: scene type-erased inspector API")
{
    beng::Scene scene;
    scene.registerComponentType<ReflectionTestComponent>();

    // Типы: Transform (0) + тестовый (1)
    BLIB_TEST_CHECK(scene.getComponentTypeCount() == 2);
    BLIB_TEST_CHECK(scene.getComponentTypeName(0) == std::string("beng.Transform"));
    BLIB_TEST_CHECK(scene.getComponentTypeName(1) == std::string("test.Reflection"));
    BLIB_TEST_CHECK(scene.getComponentTypeName(2) == nullptr);

    // Сущности: плотный перебор
    beng::EntityID first = scene.createEntity();
    beng::EntityID second = scene.createEntity();
    BLIB_TEST_CHECK(scene.getEntityCount() == 2);
    BLIB_TEST_CHECK(scene.getEntityId(0) == first);
    BLIB_TEST_CHECK(scene.getEntityId(1) == second);
    BLIB_TEST_CHECK(scene.getEntityId(2) == beng::invalidEntity);

    // Компоненты по локальному индексу типа (type-erased)
    beng::IComponent* transform = scene.tryGetComponent(first, 0);
    BLIB_TEST_CHECK(transform != nullptr);
    BLIB_TEST_CHECK(scene.hasComponent(first, 0));
    BLIB_TEST_CHECK(!scene.hasComponent(first, 1));
    BLIB_TEST_CHECK(scene.tryGetComponent(first, 1) == nullptr);

    // Невалидные запросы — не fatal
    BLIB_TEST_CHECK(!scene.hasComponent(beng::invalidEntity, 0));
    BLIB_TEST_CHECK(scene.tryGetComponent(beng::invalidEntity, 0) == nullptr);
    BLIB_TEST_CHECK(!scene.hasComponent(first, 63));

    // После удаления сущности — компонентов нет
    scene.destroyEntity(first);
    BLIB_TEST_CHECK(scene.tryGetComponent(first, 0) == nullptr);
    BLIB_TEST_CHECK(scene.getEntityCount() == 1);
    BLIB_TEST_CHECK(scene.getEntityId(0) == second);
}
