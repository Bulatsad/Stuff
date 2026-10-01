#include <blib/test/src/test.h>

#include <beng/client/clientPrediction.h>
#include <beng/components/transform.h>
#include <beng/core/replicationClientState.h>
#include <beng/core/replicationCodec.h>
#include <beng/core/replicationSchema.h>
#include <beng/core/scene.h>

// Тесты движкового client-side prediction игрока (beng-client):
// чистая логика ClientPrediction (старт/реконсиляция/снап) и доступ
// к новейшему серверному сэмплу позиции (ReplicationClientState::
// getLatestFieldSample) — см. CLIENT.md.

namespace
{
    buint8 encodeBuffer[beng::maxReplicationPacketBytes];
    beng::ReplicationEntityPatch patches[beng::maxSnapshotEntities];

    /**
     * Принять Welcome от «сервера» (схема строится из той же сцены —
     * имена типов и хеши полей совпадают); игрок — playerEntity.
     */
    bool acceptWelcome(_In beng::ReplicationClientState& clientState,
        _In beng::Scene& scene, _In const beng::ReplicationSchema& schema,
        beng::EntityID playerEntity)
    {
        const buint32 size = beng::encodeReplicationWelcome(
            encodeBuffer, sizeof(encodeBuffer), 60, playerEntity, schema);
        if (size == 0)
        {
            return false;
        }
        beng::DecodedReplicationWelcome welcome;
        if (!beng::decodeReplicationWelcome(encodeBuffer, size, welcome))
        {
            return false;
        }
        buint32 tickRate = 0;
        beng::EntityID outPlayer = beng::invalidEntity;
        return clientState.acceptWelcome(scene, welcome, tickRate, outPlayer)
            && tickRate == 60 && outPlayer == playerEntity;
    }

    /**
     * Применить дельту позиции Transform сущности (паттерн
     * testReplication: position — первое реплицируемое поле Transform).
     */
    bool applyPositionDelta(_In beng::ReplicationClientState& clientState,
        _In beng::Scene& scene, _In const beng::ReplicationSchema& schema,
        beng::EntityID entityId, _In const blib::math::Vector<float, 3>& position,
        bfloat receiveTime)
    {
        patches[0].entityId = entityId;
        patches[0].flags = 0;
        patches[0].componentCount = 1;
        beng::ReplicationComponentPatch& delta = patches[0].components[0];
        delta.localTypeId = scene.getTransformTypeId();
        delta.fieldIndices[0] = 0; // position
        delta.fieldValues[0] = beng::FieldValue::fromVector3(position);
        delta.fieldCount = 1;

        const buint32 size = beng::encodeReplicationSnapshot(
            encodeBuffer, sizeof(encodeBuffer), 1, patches, 1, schema);
        if (size == 0)
        {
            return false;
        }
        beng::DecodedReplicationSnapshot decoded;
        if (!beng::decodeReplicationSnapshot(encodeBuffer, size, decoded))
        {
            return false;
        }
        return clientState.applySnapshot(scene, decoded, receiveTime);
    }
}

BLIB_TEST_CASE("client_prediction: initial state is inactive")
{
    beng::client::ClientPrediction prediction;

    BLIB_TEST_CHECK(!prediction.isActive());
    BLIB_TEST_CHECK_CLOSE(prediction.getPredictedPosition().x, 0.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(prediction.getPredictedPosition().z, 0.0f, 0.0001f);

    // Без серверного сэмпла предсказание не стартует
    BLIB_TEST_CHECK(!prediction.reconcile(
        false, blib::math::Vector<float, 3>(5.0f, 0.0f, 5.0f), 80.0f));
    BLIB_TEST_CHECK(!prediction.isActive());
}

BLIB_TEST_CASE("client_prediction: starts from first server sample")
{
    beng::client::ClientPrediction prediction;
    const blib::math::Vector<float, 3> serverPosition(3.0f, 0.0f, 4.0f);

    // Первый снапшот игрока: старт от серверной позиции (скачка нет)
    BLIB_TEST_CHECK(prediction.reconcile(true, serverPosition, 80.0f));
    BLIB_TEST_CHECK(prediction.isActive());
    BLIB_TEST_CHECK_CLOSE(prediction.getPredictedPosition().x, 3.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(prediction.getPredictedPosition().z, 4.0f, 0.0001f);

    // Повторный старт невозможен: активен — обычная сверка
    BLIB_TEST_CHECK(!prediction.reconcile(true, serverPosition, 80.0f));
}

BLIB_TEST_CASE("client_prediction: small divergence trusts prediction")
{
    beng::client::ClientPrediction prediction;
    prediction.reconcile(true, blib::math::Vector<float, 3>(0.0f, 0.0f, 0.0f), 80.0f);

    // Игра предсказала вперёд (штатный лаг сервера: v·латентность) —
    // расхождение меньше порога: серверу не снапаем (иначе rubber-band
    // на остановке)
    prediction.setPredictedPosition(blib::math::Vector<float, 3>(10.0f, 0.0f, 0.0f));
    BLIB_TEST_CHECK(!prediction.reconcile(
        true, blib::math::Vector<float, 3>(5.0f, 0.0f, 0.0f), 80.0f));
    BLIB_TEST_CHECK_CLOSE(prediction.getPredictedPosition().x, 10.0f, 0.0001f);
}

BLIB_TEST_CASE("client_prediction: snaps on large divergence")
{
    beng::client::ClientPrediction prediction;
    prediction.reconcile(true, blib::math::Vector<float, 3>(0.0f, 0.0f, 0.0f), 80.0f);

    // Реальная рассинхронизация (телепорт/потеря пакетов) — снап
    prediction.setPredictedPosition(blib::math::Vector<float, 3>(500.0f, 0.0f, 0.0f));
    BLIB_TEST_CHECK(prediction.reconcile(
        true, blib::math::Vector<float, 3>(100.0f, 0.0f, 0.0f), 80.0f));
    BLIB_TEST_CHECK_CLOSE(prediction.getPredictedPosition().x, 100.0f, 0.0001f);
}

BLIB_TEST_CASE("client_prediction: missing sample keeps state and reset clears")
{
    beng::client::ClientPrediction prediction;
    prediction.reconcile(true, blib::math::Vector<float, 3>(0.0f, 0.0f, 0.0f), 80.0f);
    prediction.setPredictedPosition(blib::math::Vector<float, 3>(7.0f, 0.0f, 0.0f));

    // Сервер молчит (снапшот не пришёл) — предсказание продолжается
    BLIB_TEST_CHECK(!prediction.reconcile(
        false, blib::math::Vector<float, 3>(0.0f, 0.0f, 0.0f), 80.0f));
    BLIB_TEST_CHECK(prediction.isActive());
    BLIB_TEST_CHECK_CLOSE(prediction.getPredictedPosition().x, 7.0f, 0.0001f);

    // reset (разрыв сессии) гасит предсказание и обнуляет позицию —
    // следующее подключение стартует от первого снапшота
    prediction.reset();
    BLIB_TEST_CHECK(!prediction.isActive());
    BLIB_TEST_CHECK_CLOSE(prediction.getPredictedPosition().x, 0.0f, 0.0001f);
}

BLIB_TEST_CASE("client_prediction: latest field sample tracks snapshots")
{
    // Зеркало только с Transform: welcome + дельты позиции
    beng::Scene scene;
    beng::ReplicationSchema schema;
    schema.build(scene);

    const beng::EntityID playerId = 100;
    beng::ReplicationClientState clientState;
    BLIB_TEST_CHECK(acceptWelcome(clientState, scene, schema, playerId));

    // До снапшотов сэмпла нет
    beng::FieldValue sample;
    BLIB_TEST_CHECK(!clientState.getLatestFieldSample(playerId,
        beng::TransformComponent::componentTypeName, "position", sample));

    // Спавн-дельта: позиция (10, 0, 0) на t=1.0
    BLIB_TEST_CHECK(applyPositionDelta(clientState, scene, schema, playerId,
        blib::math::Vector<float, 3>(10.0f, 0.0f, 0.0f), 1.0f));
    BLIB_TEST_CHECK(clientState.getLatestFieldSample(playerId,
        beng::TransformComponent::componentTypeName, "position", sample));
    BLIB_TEST_CHECK(sample.kind == beng::FieldValue::Kind::Vector3);
    BLIB_TEST_CHECK_CLOSE(sample.vector3Value.x, 10.0f, 0.0001f);

    // Вторая дельта (20, 0, 0) на t=2.0 — новейший сэмпл обновился
    BLIB_TEST_CHECK(applyPositionDelta(clientState, scene, schema, playerId,
        blib::math::Vector<float, 3>(20.0f, 0.0f, 0.0f), 2.0f));
    BLIB_TEST_CHECK(clientState.getLatestFieldSample(playerId,
        beng::TransformComponent::componentTypeName, "position", sample));
    BLIB_TEST_CHECK_CLOSE(sample.vector3Value.x, 20.0f, 0.0001f);

    // Неизвестный тип/поле — false
    BLIB_TEST_CHECK(!clientState.getLatestFieldSample(playerId, "test.Nope", "position", sample));
    BLIB_TEST_CHECK(!clientState.getLatestFieldSample(playerId,
        beng::TransformComponent::componentTypeName, "nope", sample));
    BLIB_TEST_CHECK(!clientState.getLatestFieldSample(playerId + 1,
        beng::TransformComponent::componentTypeName, "position", sample));

    // Destroy чистит кольца — сэмпла больше нет
    patches[0].entityId = playerId;
    patches[0].flags = beng::replicationEntityDestroy;
    patches[0].componentCount = 0;
    const buint32 destroySize = beng::encodeReplicationSnapshot(
        encodeBuffer, sizeof(encodeBuffer), 2, patches, 1, schema);
    BLIB_TEST_CHECK(destroySize > 0);
    beng::DecodedReplicationSnapshot destroyed;
    BLIB_TEST_CHECK(beng::decodeReplicationSnapshot(encodeBuffer, destroySize, destroyed));
    BLIB_TEST_CHECK(clientState.applySnapshot(scene, destroyed, 3.0f));
    BLIB_TEST_CHECK(!clientState.getLatestFieldSample(playerId,
        beng::TransformComponent::componentTypeName, "position", sample));
}
