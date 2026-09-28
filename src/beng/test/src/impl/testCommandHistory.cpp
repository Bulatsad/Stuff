#include <blib/test/src/test.h>

#include <beng/core/commandHistory.h>
#include <beng/core/scene.h>
#include <beng/components/transform.h>

#include <blib/core/json/json.h>
#include <blib/core/verifyHelper.h>

#include <vector>

// Сериализуемый тестовый компонент (для снимков destroy/remove):
// JSON {value, isActive}
struct HistoryTestComponent : public beng::IComponent
{
    static constexpr const char* componentTypeName = "test.HistoryComponent";

    using blib::core::IStrongComparable::strongCompare;

    HistoryTestComponent()
        : value(0)
    {
    }

    explicit HistoryTestComponent(bint32 v)
        : value(v)
    {
    }

    blib::core::SaveStatus save(_In blib::core::IOutputStream& os) const __blib_override
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();
        doc.set("value", blib::core::json::JsonValue(static_cast<bint64>(value)));
        doc.set("isActive", blib::core::json::JsonValue(isActive));
        if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
        {
            return blib::core::SaveStatus::WriteFailed;
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus load(_In blib::core::IInputStream& is) __blib_override
    {
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
        {
            return blib::core::LoadStatus::InvalidData;
        }
        if (__blib_unlikely(!doc.isObject() ||
            !doc.has("value") || !doc.get("value").isNumber() ||
            !doc.has("isActive") || !doc.get("isActive").isBool()))
        {
            return blib::core::LoadStatus::InvalidData;
        }
        value = static_cast<bint32>(doc.get("value").asBint64());
        isActive = doc.get("isActive").asBool();
        return blib::core::LoadStatus::None;
    }

    bool strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const __blib_override
    {
        if (!session.enter(this, &other))
        {
            return true;
        }
        const HistoryTestComponent& o = static_cast<const HistoryTestComponent&>(other);
        return getOwnerId() == o.getOwnerId() && isActive == o.isActive && value == o.value;
    }

    bool verify() const __blib_override
    {
        // standalone-сериализация: verifyRoundTrip по save/load
        return blib::core::verifyRoundTrip(*this);
    }

    bint32 value;
};

BLIB_TEST_CASE("command history: field command undo/redo via reflection")
{
    beng::Scene scene;
    beng::EntityID entity = scene.createEntity();
    beng::TransformComponent& transform = scene.getComponent<beng::TransformComponent>(entity);

    const beng::ComponentTypeDescriptor& descriptor = beng::TransformComponent::componentReflection();
    const beng::IComponentField* positionField = descriptor.getField(0);

    // Старое значение (дефолт 0,0,0) и новое (10,20,30): применяем
    // полем, как это делает Inspector, затем пишем в историю
    beng::FieldValue oldValue;
    positionField->getValue(transform, oldValue);

    beng::FieldValue newValue = beng::FieldValue::fromVector3(
        blib::math::Vector<float, 3>(10.0f, 20.0f, 30.0f));
    positionField->setValue(transform, newValue);

    beng::editor::CommandHistory history;
    history.recordFieldChange(scene, entity, 0, 0, oldValue, newValue);
    BLIB_TEST_CHECK(history.getUndoCount() == 1);

    // Undo — возврат старого значения
    BLIB_TEST_CHECK(history.undo());
    BLIB_TEST_CHECK_CLOSE(transform.getLocalPosition().x, 0.0f, 0.001f);
    BLIB_TEST_CHECK(history.getRedoCount() == 1);
    BLIB_TEST_CHECK(!history.undo()); // undo-стек пуст — false

    // Redo — снова новое значение
    BLIB_TEST_CHECK(history.redo());
    BLIB_TEST_CHECK_CLOSE(transform.getLocalPosition().y, 20.0f, 0.001f);
}

BLIB_TEST_CASE("command history: transform (TRS) command undo/redo")
{
    beng::Scene scene;
    beng::EntityID entity = scene.createEntity();
    beng::TransformComponent& transform = scene.getComponent<beng::TransformComponent>(entity);

    beng::editor::TransformSnapshot oldTrs;
    oldTrs.position = transform.getLocalPosition();
    oldTrs.rotation = transform.getLocalRotation();
    oldTrs.scale = transform.getLocalScale();

    beng::editor::TransformSnapshot newTrs;
    newTrs.position = blib::math::Vector<float, 3>(5.0f, 6.0f, 7.0f);
    newTrs.rotation = transform.getLocalRotation();
    newTrs.scale = blib::math::Vector<float, 3>(2.0f, 2.0f, 2.0f);

    transform.setLocalPosition(newTrs.position);
    transform.setLocalScale(newTrs.scale);

    beng::editor::CommandHistory history;
    history.recordTransformChange(scene, entity, oldTrs, newTrs);

    BLIB_TEST_CHECK(history.undo());
    BLIB_TEST_CHECK_CLOSE(transform.getLocalPosition().x, 0.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(transform.getLocalScale().x, 1.0f, 0.001f);

    BLIB_TEST_CHECK(history.redo());
    BLIB_TEST_CHECK_CLOSE(transform.getLocalPosition().z, 7.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(transform.getLocalScale().y, 2.0f, 0.001f);
}

BLIB_TEST_CASE("command history: entity create/destroy undo/redo")
{
    beng::Scene scene;
    beng::editor::CommandHistory history;

    // Create
    const beng::EntityID created = history.recordEntityCreate(scene);
    BLIB_TEST_CHECK(created != beng::invalidEntity);
    BLIB_TEST_CHECK(scene.getEntityCount() == 1);

    // Undo — сущность удалена
    BLIB_TEST_CHECK(history.undo());
    BLIB_TEST_CHECK(scene.getEntityCount() == 0);

    // Redo — пересоздана с НОВЫМ ID (ID не переиспользуются)
    BLIB_TEST_CHECK(history.redo());
    BLIB_TEST_CHECK(scene.getEntityCount() == 1);
    const beng::EntityID recreated = scene.getEntityId(0);
    BLIB_TEST_CHECK(recreated != created);
    BLIB_TEST_CHECK(recreated != beng::invalidEntity);

    // Destroy с восстановлением TRS
    beng::TransformComponent& transform = scene.getComponent<beng::TransformComponent>(recreated);
    transform.setLocalPosition(blib::math::Vector<float, 3>(3.0f, 4.0f, 5.0f));

    BLIB_TEST_CHECK(history.recordEntityDestroy(scene, recreated));
    BLIB_TEST_CHECK(scene.getEntityCount() == 0);

    BLIB_TEST_CHECK(history.undo());
    BLIB_TEST_CHECK(scene.getEntityCount() == 1);
    const beng::EntityID restored = scene.getEntityId(0);
    BLIB_TEST_CHECK(scene.tryGetComponent<beng::TransformComponent>(restored) != nullptr);
    BLIB_TEST_CHECK_CLOSE(scene.getComponent<beng::TransformComponent>(restored).getLocalPosition().y, 4.0f, 0.001f);

    BLIB_TEST_CHECK(history.redo());
    BLIB_TEST_CHECK(scene.getEntityCount() == 0);
}

BLIB_TEST_CASE("command history: component add/remove undo/redo")
{
    beng::Scene scene;
    scene.registerComponentType<HistoryTestComponent>();
    beng::EntityID entity = scene.createEntity();

    beng::editor::CommandHistory history;

    // Add
    BLIB_TEST_CHECK(history.recordComponentAdd(scene, entity, 1));
    BLIB_TEST_CHECK(scene.hasComponent(entity, 1));
    BLIB_TEST_CHECK(history.undo());
    BLIB_TEST_CHECK(!scene.hasComponent(entity, 1));
    BLIB_TEST_CHECK(history.redo());
    BLIB_TEST_CHECK(scene.hasComponent(entity, 1));

    // Remove со снимком значения
    scene.getComponent<HistoryTestComponent>(entity).value = 42;
    BLIB_TEST_CHECK(history.recordComponentRemove(scene, entity, 1));
    BLIB_TEST_CHECK(!scene.hasComponent(entity, 1));

    BLIB_TEST_CHECK(history.undo());
    BLIB_TEST_CHECK(scene.hasComponent(entity, 1));
    BLIB_TEST_CHECK(scene.getComponent<HistoryTestComponent>(entity).value == 42);

    BLIB_TEST_CHECK(history.redo());
    BLIB_TEST_CHECK(!scene.hasComponent(entity, 1));
}

BLIB_TEST_CASE("command history: undo skips stale entities safely")
{
    beng::Scene scene;
    beng::EntityID entity = scene.createEntity();

    beng::editor::CommandHistory history;
    history.recordEntityCreate(scene);
    history.recordEntityCreate(scene);

    // Прямое удаление сущности вне истории — команды «протухли»,
    // но undo/redo не должны ронять процесс
    scene.destroyEntity(entity);

    BLIB_TEST_CHECK(history.undo());
    BLIB_TEST_CHECK(history.undo());
    BLIB_TEST_CHECK(history.redo());
}

BLIB_TEST_CASE("command history: limit trims oldest commands")
{
    beng::Scene scene;
    beng::EntityID entity = scene.createEntity();
    beng::TransformComponent& transform = scene.getComponent<beng::TransformComponent>(entity);

    const beng::ComponentTypeDescriptor& descriptor = beng::TransformComponent::componentReflection();
    const beng::IComponentField* positionField = descriptor.getField(0);

    beng::editor::CommandHistory history;
    const buint32 total = beng::editor::CommandHistory::maxHistorySize + 5;

    for (buint32 i = 0; i < total; ++i)
    {
        beng::FieldValue oldValue;
        positionField->getValue(transform, oldValue);
        beng::FieldValue newValue = beng::FieldValue::fromVector3(
            blib::math::Vector<float, 3>(static_cast<float>(i), 0.0f, 0.0f));
        positionField->setValue(transform, newValue);
        history.recordFieldChange(scene, entity, 0, 0, oldValue, newValue);
    }

    // История ограничена лимитом: старые команды вытеснены
    BLIB_TEST_CHECK(history.getUndoCount() == beng::editor::CommandHistory::maxHistorySize);
}
