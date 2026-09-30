#include <blib/test/src/test.h>

#include <beng/core/componentReflection.h>
#include <beng/core/replicationClientState.h>
#include <beng/core/replicationCodec.h>
#include <beng/core/replicationFramer.h>
#include <beng/core/replicationSchema.h>
#include <beng/core/scene.h>

#include <blib/core/console/console.h>
#include <blib/core/memoryStream.h>

#include <cstring>

// Тесты рефлексивной репликации (beng-core): схема, кодек полей и
// снапшотов, фреймер, клиентское зеркало (ReplicationClientState)

namespace
{
    /**
     * Тестовый компонент с рефлексией: health/speed реплицируются,
     * tag — нет (локальный кеш), target — Entity-поле (реплицируется,
     * проверяет kind-кодек Entity).
     */
    class ReplTestComponent : public beng::IComponent
    {
    private:
        bint32 health;
        bfloat speed;
        bool tag;
        beng::EntityID target;

    public:
        static constexpr const char* componentTypeName = "test.ReplTest";

        using blib::core::IStrongComparable::strongCompare;

        ReplTestComponent()
            : health(0)
            , speed(0.0f)
            , tag(false)
            , target(beng::invalidEntity)
        {
        }

        bint32 getHealth() const { return this->health; }
        bfloat getSpeed() const { return this->speed; }
        bool getTag() const { return this->tag; }
        beng::EntityID getTarget() const { return this->target; }

        void setHealth(bint32 v) { this->health = v; }
        void setSpeed(bfloat v) { this->speed = v; }
        void setTag(bool v) { this->tag = v; }
        void setTarget(beng::EntityID v) { this->target = v; }

        static const beng::ComponentTypeDescriptor& componentReflection();

        bool strongCompare(_In const blib::core::IStrongComparable& other,
            _In blib::core::CompareSession& session) const __blib_override
        {
            if (!session.enter(this, &other))
            {
                return true;
            }
            const ReplTestComponent& o = static_cast<const ReplTestComponent&>(other);
            return getOwnerId() == o.getOwnerId() && isActive == o.isActive
                && this->health == o.health && this->speed == o.speed
                && this->tag == o.tag && this->target == o.target;
        }

        bool verify() const __blib_override
        {
            return false;
        }
    };

    const beng::FunctionField s_healthField(
        "health", beng::FieldValue::Kind::Int,
        [](_In const beng::IComponent& c, _Out beng::FieldValue& out)
        {
            out = beng::FieldValue::fromInt(static_cast<const ReplTestComponent&>(c).getHealth());
        },
        [](_In beng::IComponent& c, _In const beng::FieldValue& v)
        {
            static_cast<ReplTestComponent&>(c).setHealth(v.intValue);
        },
        true);

    const beng::FunctionField s_speedField(
        "speed", beng::FieldValue::Kind::Float,
        [](_In const beng::IComponent& c, _Out beng::FieldValue& out)
        {
            out = beng::FieldValue::fromFloat(static_cast<const ReplTestComponent&>(c).getSpeed());
        },
        [](_In beng::IComponent& c, _In const beng::FieldValue& v)
        {
            static_cast<ReplTestComponent&>(c).setSpeed(v.floatValue);
        },
        true);

    const beng::FunctionField s_tagField(
        "tag", beng::FieldValue::Kind::Bool,
        [](_In const beng::IComponent& c, _Out beng::FieldValue& out)
        {
            out = beng::FieldValue::fromBool(static_cast<const ReplTestComponent&>(c).getTag());
        },
        [](_In beng::IComponent& c, _In const beng::FieldValue& v)
        {
            static_cast<ReplTestComponent&>(c).setTag(v.boolValue);
        });

    const beng::FunctionField s_targetField(
        "target", beng::FieldValue::Kind::Entity,
        [](_In const beng::IComponent& c, _Out beng::FieldValue& out)
        {
            out = beng::FieldValue::fromEntity(static_cast<const ReplTestComponent&>(c).getTarget());
        },
        [](_In beng::IComponent& c, _In const beng::FieldValue& v)
        {
            static_cast<ReplTestComponent&>(c).setTarget(v.entityValue);
        },
        true);

    const beng::IComponentField* const s_replTestFields[] = {
        &s_healthField,
        &s_speedField,
        &s_tagField,
        &s_targetField
    };

    const beng::ComponentTypeDescriptor s_replTestReflection(
        ReplTestComponent::componentTypeName, s_replTestFields, 4);

    const beng::ComponentTypeDescriptor& ReplTestComponent::componentReflection()
    {
        return s_replTestReflection;
    }

    /**
     * Зарегистрировать типы в тестовой сцене (порядок регистрации
     * тестового компонента можно менять — маппинг репликации работает
     * по ИМЕНАМ, не по локальным индексам).
     */
    void registerTestTypes(_In beng::Scene& scene)
    {
        if (!scene.isRegisteredComponentType<ReplTestComponent>())
        {
            scene.registerComponentType<ReplTestComponent>();
        }
    }

    /**
     * Собрать полный патч сущности вручную (testreplication линкует
     * только beng-core — ReplicationManager живёт в beng-server).
     */
    bool buildFullPatch(_In const beng::Scene& scene, _In const beng::ReplicationSchema& schema,
        beng::EntityID entityId, _Out beng::ReplicationEntityPatch& patch)
    {
        patch.entityId = entityId;
        patch.flags = beng::replicationEntityFull;
        patch.componentCount = 0;

        const buint32 wireTypeCount = schema.getWireTypeCount();
        for (buint32 w = 0; w < wireTypeCount; ++w)
        {
            const beng::ComponentType localType = schema.getWireType(w)->localTypeId;
            if (!scene.hasComponent(entityId, localType))
            {
                continue;
            }

            const beng::IComponent* component = scene.tryGetComponent(entityId, localType);
            const buint32 fieldCount = schema.getReplicatedFieldCount(localType);
            if (component == nullptr || fieldCount == 0)
            {
                continue;
            }

            beng::ReplicationComponentPatch& cp = patch.components[patch.componentCount];
            cp.localTypeId = localType;
            cp.fieldCount = static_cast<buint8>(fieldCount);
            for (buint32 f = 0; f < fieldCount; ++f)
            {
                const beng::IComponentField* field = schema.getReplicatedField(localType, f);
                cp.fieldIndices[f] = static_cast<buint8>(f);
                field->getValue(*component, cp.fieldValues[f]);
            }
            ++patch.componentCount;
        }
        return true;
    }

    buint8 encodeBuffer[beng::maxReplicationPacketBytes];
    beng::ReplicationEntityPatch patches[beng::maxSnapshotEntities];
}

BLIB_TEST_CASE("replication: schema collects replicated fields")
{
    beng::Scene scene;
    registerTestTypes(scene);

    beng::ReplicationSchema schema;
    schema.build(scene);

    // Wire-типы: beng.Transform (position/scale) + test.ReplTest
    // (health/speed/target); tag не реплицируется
    BLIB_TEST_CHECK(schema.getWireTypeCount() == 2);

    // Transform: 2 реплицируемых поля, тип с ним — wire-тип
    const beng::ComponentType transformType = scene.getTransformTypeId();
    BLIB_TEST_CHECK(schema.getReplicatedFieldCount(transformType) == 2);
    BLIB_TEST_CHECK(schema.getReplicatedField(transformType, 0) != nullptr);
    BLIB_TEST_CHECK(schema.getWireIdForLocalType(transformType) != beng::invalidComponentType);

    // Интерполяция — метаданные полей: position интерполируется
    // (непрерывная величина), scale — нет (применяется сразу)
    const beng::IComponentField* positionField = schema.getReplicatedField(transformType, 0);
    const beng::IComponentField* scaleField = schema.getReplicatedField(transformType, 1);
    BLIB_TEST_CHECK(positionField != nullptr && scaleField != nullptr);
    BLIB_TEST_CHECK(positionField->isInterpolated());
    BLIB_TEST_CHECK(!scaleField->isInterpolated());

    // Имя wire-типа — стабильное имя компонента
    bool foundReplTest = false;
    for (buint32 w = 0; w < schema.getWireTypeCount(); ++w)
    {
        if (std::strcmp(schema.getWireType(w)->name, "test.ReplTest") == 0)
        {
            foundReplTest = true;
            BLIB_TEST_CHECK(schema.getWireType(w)->schemaHash != 0);
        }
    }
    BLIB_TEST_CHECK(foundReplTest);

    // Детерминированность хеша: повторная сборка даёт те же значения
    beng::ReplicationSchema schema2;
    schema2.build(scene);
    for (buint32 w = 0; w < schema.getWireTypeCount(); ++w)
    {
        BLIB_TEST_CHECK(schema.getWireType(w)->schemaHash == schema2.getWireType(w)->schemaHash);
    }
}

BLIB_TEST_CASE("replication: schema hash changes with field set")
{
    // Схема «той же» сцены с одним типом: хеш Transform обязан
    // отличаться от хеша Transform из сцены, где у него нет
    // реплицируемых полей — такой сцены не бывает (Transform всегда
    // реплицируем), поэтому проверяем через два типа с разными
    // наборами: hash(ReplTest) != hash(Transform)
    beng::Scene scene;
    registerTestTypes(scene);

    beng::ReplicationSchema schema;
    schema.build(scene);

    const beng::ReplicationSchema::WireType* transformWire = nullptr;
    const beng::ReplicationSchema::WireType* replTestWire = nullptr;
    for (buint32 w = 0; w < schema.getWireTypeCount(); ++w)
    {
        const beng::ReplicationSchema::WireType* wire = schema.getWireType(w);
        if (std::strcmp(wire->name, "beng.Transform") == 0)
        {
            transformWire = wire;
        }
        else if (std::strcmp(wire->name, "test.ReplTest") == 0)
        {
            replTestWire = wire;
        }
    }
    BLIB_TEST_CHECK(transformWire != nullptr && replTestWire != nullptr);
    BLIB_TEST_CHECK(transformWire->schemaHash != replTestWire->schemaHash);
}

BLIB_TEST_CASE("replication: field codec roundtrips all kinds")
{
    blib::core::MemoryStream stream;

    // Float
    BLIB_TEST_CHECK(beng::writeFieldValue(stream, beng::FieldValue::fromFloat(-3.25f)));
    // Int
    BLIB_TEST_CHECK(beng::writeFieldValue(stream, beng::FieldValue::fromInt(-123456)));
    // Bool
    BLIB_TEST_CHECK(beng::writeFieldValue(stream, beng::FieldValue::fromBool(true)));
    // Vector3
    BLIB_TEST_CHECK(beng::writeFieldValue(stream,
        beng::FieldValue::fromVector3(blib::math::Vector<float, 3>(1.0f, -2.5f, 42.0f))));
    // Entity
    BLIB_TEST_CHECK(beng::writeFieldValue(stream, beng::FieldValue::fromEntity(987654321ull)));

    stream.seek(0, blib::core::SeekOrigin::Begin);

    beng::FieldValue v;
    BLIB_TEST_CHECK(beng::readFieldValue(stream, v));
    BLIB_TEST_CHECK(v.kind == beng::FieldValue::Kind::Float);
    BLIB_TEST_CHECK_CLOSE(v.floatValue, -3.25f, 0.0001f);

    BLIB_TEST_CHECK(beng::readFieldValue(stream, v));
    BLIB_TEST_CHECK(v.kind == beng::FieldValue::Kind::Int);
    BLIB_TEST_CHECK(v.intValue == -123456);

    BLIB_TEST_CHECK(beng::readFieldValue(stream, v));
    BLIB_TEST_CHECK(v.kind == beng::FieldValue::Kind::Bool);
    BLIB_TEST_CHECK(v.boolValue);

    BLIB_TEST_CHECK(beng::readFieldValue(stream, v));
    BLIB_TEST_CHECK(v.kind == beng::FieldValue::Kind::Vector3);
    BLIB_TEST_CHECK_CLOSE(v.vector3Value.x, 1.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(v.vector3Value.y, -2.5f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(v.vector3Value.z, 42.0f, 0.0001f);

    BLIB_TEST_CHECK(beng::readFieldValue(stream, v));
    BLIB_TEST_CHECK(v.kind == beng::FieldValue::Kind::Entity);
    BLIB_TEST_CHECK(v.entityValue == 987654321ull);

    // Unset — запись отклоняется
    beng::FieldValue unset;
    unset.kind = beng::FieldValue::Kind::Unset;
    blib::core::MemoryStream bad;
    BLIB_TEST_CHECK(!beng::writeFieldValue(bad, unset));
}

BLIB_TEST_CASE("replication: field codec rejects truncated streams")
{
    // Полное значение Vector3 = 1 + 12 байт; обрежем поток — read
    // обязан отказать (частичный payload = обрыв)
    blib::core::MemoryStream full;
    BLIB_TEST_CHECK(beng::writeFieldValue(full,
        beng::FieldValue::fromVector3(blib::math::Vector<float, 3>(1.0f, 2.0f, 3.0f))));

    // Копия первых 10 байт полного значения (payload обрезан)
    blib::core::MemoryStream truncated;
    BLIB_TEST_CHECK(truncated.write(full.getData().data(), 10) == 10);
    truncated.seek(0, blib::core::SeekOrigin::Begin);

    beng::FieldValue v;
    BLIB_TEST_CHECK(!beng::readFieldValue(truncated, v));

    // Пустой поток — false
    blib::core::MemoryStream empty;
    BLIB_TEST_CHECK(!beng::readFieldValue(empty, v));
}

BLIB_TEST_CASE("replication: framer reassembles split stream")
{
    beng::ReplicationFramer framer;

    // Два сообщения, приходят тремя кусками
    buint8 msg1[] = { 0x01, 0x03, 0x00, 0xAA, 0xBB, 0xCC };       // Command, payload 3
    buint8 msg2[] = { 0x02, 0x02, 0x00, 0x11, 0x22 };             // Snapshot, payload 2

    BLIB_TEST_CHECK(framer.pushBytes(msg1, 2));
    BLIB_TEST_CHECK(framer.pushBytes(msg1 + 2, 2));
    BLIB_TEST_CHECK(framer.pushBytes(msg1 + 4, sizeof(msg1) - 4));
    BLIB_TEST_CHECK(framer.pushBytes(msg2, sizeof(msg2)));

    buint8 payload[16];
    buint32 payloadSize = 0;
    beng::ReplicationPacketType type = beng::ReplicationPacketType::None;

    BLIB_TEST_CHECK(framer.nextMessage(payload, sizeof(payload), payloadSize, type));
    BLIB_TEST_CHECK(type == beng::ReplicationPacketType::Command);
    BLIB_TEST_CHECK(payloadSize == 3);
    BLIB_TEST_CHECK(payload[0] == 0xAA && payload[2] == 0xCC);

    BLIB_TEST_CHECK(framer.nextMessage(payload, sizeof(payload), payloadSize, type));
    BLIB_TEST_CHECK(type == beng::ReplicationPacketType::Snapshot);
    BLIB_TEST_CHECK(payloadSize == 2);

    // Неизвестный тип — поток сбрасывается, сообщений нет
    buint8 garbage[] = { 0x7F, 0x00, 0x00, 0x00 };
    BLIB_TEST_CHECK(framer.pushBytes(garbage, sizeof(garbage)));
    BLIB_TEST_CHECK(!framer.nextMessage(payload, sizeof(payload), payloadSize, type));
}

BLIB_TEST_CASE("replication: welcome roundtrip")
{
    beng::Scene scene;
    registerTestTypes(scene);
    beng::ReplicationSchema schema;
    schema.build(scene);

    const buint32 size = beng::encodeReplicationWelcome(
        encodeBuffer, sizeof(encodeBuffer), 60, 7, schema);
    BLIB_TEST_CHECK(size > 0);

    beng::DecodedReplicationWelcome welcome;
    BLIB_TEST_CHECK(beng::decodeReplicationWelcome(encodeBuffer, size, welcome));
    BLIB_TEST_CHECK(welcome.tickRate == 60);
    BLIB_TEST_CHECK(welcome.playerEntityId == 7);
    BLIB_TEST_CHECK(welcome.typeCount == 2);

    // Имена типов на месте
    bool foundTransform = false;
    for (buint32 t = 0; t < welcome.typeCount; ++t)
    {
        if (std::strcmp(welcome.types[t].name, "beng.Transform") == 0)
        {
            foundTransform = true;
        }
    }
    BLIB_TEST_CHECK(foundTransform);
}

BLIB_TEST_CASE("replication: client state accepts welcome by names")
{
    beng::Scene scene;
    registerTestTypes(scene);
    beng::ReplicationSchema schema;
    schema.build(scene);

    const buint32 size = beng::encodeReplicationWelcome(
        encodeBuffer, sizeof(encodeBuffer), 60, 5, schema);
    beng::DecodedReplicationWelcome welcome;
    BLIB_TEST_CHECK(beng::decodeReplicationWelcome(encodeBuffer, size, welcome));

    beng::ReplicationClientState clientState;
    buint32 tickRate = 0;
    beng::EntityID playerEntity = beng::invalidEntity;
    BLIB_TEST_CHECK(clientState.acceptWelcome(scene, welcome, tickRate, playerEntity));
    BLIB_TEST_CHECK(tickRate == 60);
    BLIB_TEST_CHECK(playerEntity == 5);
}

BLIB_TEST_CASE("replication: client state rejects schema mismatch")
{
    beng::Scene scene;
    registerTestTypes(scene);
    beng::ReplicationSchema schema;
    schema.build(scene);

    const buint32 size = beng::encodeReplicationWelcome(
        encodeBuffer, sizeof(encodeBuffer), 60, 5, schema);
    beng::DecodedReplicationWelcome welcome;
    BLIB_TEST_CHECK(beng::decodeReplicationWelcome(encodeBuffer, size, welcome));

    // Подменяем хеш одного из типов — сверка обязана отказать
    welcome.types[0].schemaHash = ~welcome.types[0].schemaHash;

    beng::ReplicationClientState clientState;
    buint32 tickRate = 0;
    beng::EntityID playerEntity = beng::invalidEntity;
    BLIB_TEST_CHECK(!clientState.acceptWelcome(scene, welcome, tickRate, playerEntity));
}

BLIB_TEST_CASE("replication: snapshot full/delta/destroy roundtrip")
{
    beng::Scene scene;
    registerTestTypes(scene);
    beng::ReplicationSchema schema;
    schema.build(scene);

    // Сущность с Transform + тестовым компонентом
    const beng::EntityID entityId = scene.createEntity();
    scene.getComponent<beng::TransformComponent>(entityId).setLocalPosition(
        blib::math::Vector<float, 3>(10.0f, 20.0f, 30.0f));
    scene.addComponent<ReplTestComponent>(entityId).setHealth(42);

    // Полный патч
    buint32 entityCount = 0;
    BLIB_TEST_CHECK(buildFullPatch(scene, schema, entityId, patches[0]));
    entityCount = 1;

    const buint32 size = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 100, patches, entityCount, schema);
    BLIB_TEST_CHECK(size > 0);

    beng::DecodedReplicationSnapshot decoded;
    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, size, decoded));
    BLIB_TEST_CHECK(decoded.tickNumber == 100);
    BLIB_TEST_CHECK(decoded.entityCount == 1);
    BLIB_TEST_CHECK(decoded.entities[0].entityId == entityId);
    BLIB_TEST_CHECK(decoded.entities[0].full);
    BLIB_TEST_CHECK(!decoded.entities[0].destroy);

    // Destroy-запись
    patches[0].entityId = entityId;
    patches[0].flags = beng::replicationEntityDestroy;
    patches[0].componentCount = 0;
    const buint32 destroySize = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 101, patches, 1, schema);
    BLIB_TEST_CHECK(destroySize > 0);

    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, destroySize, decoded));
    BLIB_TEST_CHECK(decoded.entities[0].destroy);
    BLIB_TEST_CHECK(decoded.entities[0].componentCount == 0);
}

BLIB_TEST_CASE("replication: decoder rejects malformed snapshots")
{
    // Пустой payload
    beng::DecodedReplicationSnapshot decoded;
    BLIB_TEST_CHECK(!beng::decodeReplicationSnapshot(encodeBuffer, 0, decoded));

    // Заголовок без данных сущности (обрыв)
    encodeBuffer[0] = 10; encodeBuffer[1] = 0; encodeBuffer[2] = 0; encodeBuffer[3] = 0;
    encodeBuffer[4] = 1; encodeBuffer[5] = 0; // entityCount = 1, данных нет
    BLIB_TEST_CHECK(!beng::decodeReplicationSnapshot(encodeBuffer, 6, decoded));
}

BLIB_TEST_CASE("replication: client state applies spawn/update/destroy")
{
    // ===== Серверная сцена (авторитет) =====
    beng::Scene serverScene;
    registerTestTypes(serverScene);
    beng::ReplicationSchema serverSchema;
    serverSchema.build(serverScene);

    const beng::EntityID unitId = serverScene.createEntity();
    serverScene.getComponent<beng::TransformComponent>(unitId).setLocalPosition(
        blib::math::Vector<float, 3>(5.0f, 0.0f, 8.0f));
    serverScene.getComponent<beng::TransformComponent>(unitId).setLocalScale(
        blib::math::Vector<float, 3>(2.0f, 3.0f, 4.0f));
    serverScene.addComponent<ReplTestComponent>(unitId).setHealth(100);

    // ===== Клиентская сцена (зеркало) =====
    beng::Scene clientScene;
    registerTestTypes(clientScene);

    // Welcome (схема сервера → клиент сверяет по именам)
    const buint32 welcomeSize = beng::encodeReplicationWelcome(
        encodeBuffer, sizeof(encodeBuffer), 60, unitId, serverSchema);
    beng::DecodedReplicationWelcome welcome;
    BLIB_TEST_CHECK(beng::decodeReplicationWelcome(encodeBuffer, welcomeSize, welcome));

    beng::ReplicationClientState clientState;
    buint32 tickRate = 0;
    beng::EntityID playerEntity = beng::invalidEntity;
    BLIB_TEST_CHECK(clientState.acceptWelcome(clientScene, welcome, tickRate, playerEntity));
    BLIB_TEST_CHECK(playerEntity == unitId);

    // ===== Spawn: полный снапшот (время приёма t=1.0) =====
    BLIB_TEST_CHECK(buildFullPatch(serverScene, serverSchema, unitId, patches[0]));
    buint32 spawnSize = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 1, patches, 1, serverSchema);
    BLIB_TEST_CHECK(spawnSize > 0);

    beng::DecodedReplicationSnapshot decoded;
    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, spawnSize, decoded));
    BLIB_TEST_CHECK(clientState.applySnapshot(clientScene, decoded, 1.0f));
    BLIB_TEST_CHECK(clientState.getKnownEntityCount() == 1);

    // Зеркало: та же сущность создана; position — интерполируемое поле,
    // буферизовано и в сцену ещё НЕ записано (дефолт (0,0,0)); scale —
    // неинтерполируемое, применено сразу
    const beng::TransformComponent* mirrorTransform =
        clientScene.tryGetComponent<beng::TransformComponent>(unitId);
    BLIB_TEST_CHECK(mirrorTransform != nullptr);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 0.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalScale().x, 2.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalScale().z, 4.0f, 0.0001f);

    const ReplTestComponent* mirrorComponent =
        clientScene.tryGetComponent<ReplTestComponent>(unitId);
    BLIB_TEST_CHECK(mirrorComponent != nullptr);
    BLIB_TEST_CHECK(mirrorComponent->getHealth() == 100);

    // Событие спавна сливается игрой
    beng::EntityID spawnEvents[beng::maxSnapshotEntities];
    BLIB_TEST_CHECK(clientState.takeSpawnEvents(spawnEvents, beng::maxSnapshotEntities) == 1);
    BLIB_TEST_CHECK(spawnEvents[0] == unitId);
    BLIB_TEST_CHECK(clientState.takeSpawnEvents(spawnEvents, beng::maxSnapshotEntities) == 0);

    // renderMirror(now=1.0): время рендера (1.0 − delay) раньше
    // старейшего сэмпла — держим старейший (5,0,8)
    clientState.renderMirror(clientScene, 1.0f);
    mirrorTransform = clientScene.tryGetComponent<beng::TransformComponent>(unitId);
    BLIB_TEST_CHECK(mirrorTransform != nullptr);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 5.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().z, 8.0f, 0.0001f);

    // ===== Update: дельта меняет позицию (t=2.0) =====
    serverScene.getComponent<beng::TransformComponent>(unitId).setLocalPosition(
        blib::math::Vector<float, 3>(6.0f, 0.0f, 9.0f));

    patches[0].entityId = unitId;
    patches[0].flags = 0;
    patches[0].componentCount = 1;
    beng::ReplicationComponentPatch& delta = patches[0].components[0];
    delta.localTypeId = serverScene.getTransformTypeId();
    delta.fieldIndices[0] = 0; // position — первое реплицируемое поле Transform
    delta.fieldValues[0] = beng::FieldValue::fromVector3(
        blib::math::Vector<float, 3>(6.0f, 0.0f, 9.0f));
    delta.fieldCount = 1;

    const buint32 deltaSize = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 2, patches, 1, serverSchema);
    BLIB_TEST_CHECK(deltaSize > 0);
    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, deltaSize, decoded));
    BLIB_TEST_CHECK(clientState.applySnapshot(clientScene, decoded, 2.0f));

    // renderMirror(now=2.0): интерполяция между t=1.0 (5,0,8) и
    // t=2.0 (6,0,9) — время рендера 2.0 − delay ≈ 1.9667
    clientState.renderMirror(clientScene, 2.0f);
    mirrorTransform = clientScene.tryGetComponent<beng::TransformComponent>(unitId);
    BLIB_TEST_CHECK(mirrorTransform != nullptr);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 5.9667f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().z, 8.9667f, 0.001f);

    // renderMirror(now=3.0): время рендера новее новейшего сэмпла —
    // держим новейший (6,0,9), без экстраполяции
    clientState.renderMirror(clientScene, 3.0f);
    mirrorTransform = clientScene.tryGetComponent<beng::TransformComponent>(unitId);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 6.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().z, 9.0f, 0.0001f);

    // ===== Destroy (t=4.0) =====
    serverScene.destroyEntity(unitId);
    patches[0].entityId = unitId;
    patches[0].flags = beng::replicationEntityDestroy;
    patches[0].componentCount = 0;
    const buint32 destroySize = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 3, patches, 1, serverSchema);
    BLIB_TEST_CHECK(destroySize > 0);
    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, destroySize, decoded));
    BLIB_TEST_CHECK(clientState.applySnapshot(clientScene, decoded, 4.0f));
    BLIB_TEST_CHECK(clientState.getKnownEntityCount() == 0);
    BLIB_TEST_CHECK(clientScene.tryGetComponent<beng::TransformComponent>(unitId) == nullptr);

    // Событие уничтожения сливается игрой
    beng::EntityID destroyEvents[beng::maxSnapshotEntities];
    BLIB_TEST_CHECK(clientState.takeDestroyEvents(destroyEvents, beng::maxSnapshotEntities) == 1);
    BLIB_TEST_CHECK(destroyEvents[0] == unitId);
}

BLIB_TEST_CASE("replication: client state interpolates between snapshots")
{
    // ===== Серверная сцена (авторитет) =====
    beng::Scene serverScene;
    registerTestTypes(serverScene);
    beng::ReplicationSchema serverSchema;
    serverSchema.build(serverScene);

    const beng::EntityID unitId = serverScene.createEntity();

    // ===== Клиентская сцена (зеркало) =====
    beng::Scene clientScene;
    registerTestTypes(clientScene);

    const buint32 welcomeSize = beng::encodeReplicationWelcome(
        encodeBuffer, sizeof(encodeBuffer), 60, unitId, serverSchema);
    beng::DecodedReplicationWelcome welcome;
    BLIB_TEST_CHECK(beng::decodeReplicationWelcome(encodeBuffer, welcomeSize, welcome));

    beng::ReplicationClientState clientState;
    buint32 tickRate = 0;
    beng::EntityID playerEntity = beng::invalidEntity;
    BLIB_TEST_CHECK(clientState.acceptWelcome(clientScene, welcome, tickRate, playerEntity));

    // Спавн: полный снапшот, позиция (0,0,0) на t=1.0
    BLIB_TEST_CHECK(buildFullPatch(serverScene, serverSchema, unitId, patches[0]));
    buint32 size = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 10, patches, 1, serverSchema);
    beng::DecodedReplicationSnapshot decoded;
    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, size, decoded));
    BLIB_TEST_CHECK(clientState.applySnapshot(clientScene, decoded, 1.0f));

    // Дельты позиции: t=2.0 → x=10, t=3.0 → x=20, t=4.0 → x=30
    const float deltaPositions[3] = { 10.0f, 20.0f, 30.0f };
    for (buint32 i = 0; i < 3; ++i)
    {
        patches[0].entityId = unitId;
        patches[0].flags = 0;
        patches[0].componentCount = 1;
        beng::ReplicationComponentPatch& delta = patches[0].components[0];
        delta.localTypeId = serverScene.getTransformTypeId();
        delta.fieldIndices[0] = 0; // position
        delta.fieldValues[0] = beng::FieldValue::fromVector3(
            blib::math::Vector<float, 3>(deltaPositions[i], 0.0f, 0.0f));
        delta.fieldCount = 1;
        size = beng::encodeReplicationSnapshot(
            encodeBuffer, sizeof(encodeBuffer), 11 + i, patches, 1, serverSchema);
        BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, size, decoded));
        BLIB_TEST_CHECK(clientState.applySnapshot(clientScene, decoded, 2.0f + static_cast<float>(i)));
    }

    const beng::TransformComponent* mirrorTransform = nullptr;

    // now=1.0: время рендера раньше старейшего сэмпла — держим его (x=0)
    clientState.renderMirror(clientScene, 1.0f);
    mirrorTransform = clientScene.tryGetComponent<beng::TransformComponent>(unitId);
    BLIB_TEST_CHECK(mirrorTransform != nullptr);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 0.0f, 0.0001f);

    // now=2.0: lerp между t=1.0 (0) и t=2.0 (10), alpha ≈ 0.9667
    clientState.renderMirror(clientScene, 2.0f);
    mirrorTransform = clientScene.tryGetComponent<beng::TransformComponent>(unitId);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 9.6667f, 0.001f);

    // now=2.5: lerp между t=2.0 (10) и t=3.0 (20), alpha ≈ 0.4667
    clientState.renderMirror(clientScene, 2.5f);
    mirrorTransform = clientScene.tryGetComponent<beng::TransformComponent>(unitId);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 14.6667f, 0.001f);

    // now=10.0: время рендера новее новейшего сэмпла — без
    // экстраполяции держим новейший (x=30)
    clientState.renderMirror(clientScene, 10.0f);
    mirrorTransform = clientScene.tryGetComponent<beng::TransformComponent>(unitId);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 30.0f, 0.0001f);

    // ===== Full-ресинк на t=5.0 (позиция 100): кольца переписываются =====
    serverScene.getComponent<beng::TransformComponent>(unitId).setLocalPosition(
        blib::math::Vector<float, 3>(100.0f, 0.0f, 0.0f));
    BLIB_TEST_CHECK(buildFullPatch(serverScene, serverSchema, unitId, patches[0]));
    size = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 20, patches, 1, serverSchema);
    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, size, decoded));
    BLIB_TEST_CHECK(clientState.applySnapshot(clientScene, decoded, 5.0f));

    // now=5.0: после ресинка в кольце только сэмпл (100) — держим его
    clientState.renderMirror(clientScene, 5.0f);
    mirrorTransform = clientScene.tryGetComponent<beng::TransformComponent>(unitId);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 100.0f, 0.0001f);

    // ===== Destroy чистит кольца =====
    patches[0].entityId = unitId;
    patches[0].flags = beng::replicationEntityDestroy;
    patches[0].componentCount = 0;
    size = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 21, patches, 1, serverSchema);
    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, size, decoded));
    BLIB_TEST_CHECK(clientState.applySnapshot(clientScene, decoded, 6.0f));
    BLIB_TEST_CHECK(clientScene.tryGetComponent<beng::TransformComponent>(unitId) == nullptr);

    // ===== Повторный спавн (НОВАЯ серверная сущность) стартует заново =====
    // ID не переиспользуются (контракт Scene): сервер рождает новую
    // сущность, зеркало — свежие кольца и событие спавна
    const beng::EntityID unitId2 = serverScene.createEntity();
    serverScene.getComponent<beng::TransformComponent>(unitId2).setLocalPosition(
        blib::math::Vector<float, 3>(7.0f, 0.0f, 0.0f));
    BLIB_TEST_CHECK(buildFullPatch(serverScene, serverSchema, unitId2, patches[0]));
    size = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 22, patches, 1, serverSchema);
    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, size, decoded));
    BLIB_TEST_CHECK(clientState.applySnapshot(clientScene, decoded, 7.0f));

    clientState.renderMirror(clientScene, 7.0f);
    mirrorTransform = clientScene.tryGetComponent<beng::TransformComponent>(unitId2);
    BLIB_TEST_CHECK(mirrorTransform != nullptr);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 7.0f, 0.0001f);
}
