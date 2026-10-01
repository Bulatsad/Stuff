#include <blib/test/src/test.h>

#include <beng/core/scene.h>
#include <beng/server/replicationClient.h>
#include <beng/server/replicationManager.h>
#include <beng/server/serverApplication.h>
#include <beng/server/worldManager.h>

#include <blib/network/address.h>
#include <blib/network/tcpSocket.h>

#include <windows.h>

#include <cstdio>
#include <cstring>

// Тесты beng-server и клиента репликации: ReplicationManager
// (зеркала/дельты), WorldManager (save/load мира), ServerApplication +
// ReplicationClient (loopback-интеграция сети)

namespace
{
    /**
     * Фейковая игра для ServerApplication: фиксирует хуки, создаёт
     * юнит игрока и пишет принятые команды в лог теста.
     */
    class FakeServerGame : public beng::server::IServerGame
    {
    public:
        static constexpr buint32 fakeTickRate = 60;
        static constexpr buint32 fakePort = 42789;

        beng::Scene* sceneRef;
        beng::EntityID playerEntity;
        buint32 joinCount;
        buint32 leaveCount;
        buint32 commandCount;
        buint8 lastCommandPayload[4];
        buint32 lastCommandSize;

        FakeServerGame()
            : sceneRef(nullptr)
            , playerEntity(beng::invalidEntity)
            , joinCount(0)
            , leaveCount(0)
            , commandCount(0)
            , lastCommandSize(0)
        {
        }

        const char* getGameName() const __blib_override { return "fake"; }
        buint32 getTickRate() const __blib_override { return fakeTickRate; }
        buint32 getDefaultPort() const __blib_override { return fakePort; }

        void onServerInitialize(_In beng::Scene& scene) __blib_override
        {
            // Движковые типы (Transform) уже есть; игра не добавляет своих
            this->sceneRef = &scene;
        }

        void onWorldBuild(_In beng::Scene& scene) __blib_override
        {
            // Контент мира: один статический объект (Transform-only)
            scene.createEntity();
        }

        beng::EntityID onClientJoin(buint32 clientId) __blib_override
        {
            ++this->joinCount;
            // Игрок: сущность с позицией по clientId (различима в снапшотах)
            this->playerEntity = this->sceneRef->createEntity();
            this->sceneRef->getComponent<beng::TransformComponent>(this->playerEntity)
                .setLocalPosition(blib::math::Vector<float, 3>(
                    1.0f + static_cast<bfloat>(clientId), 0.0f, 0.0f));
            return this->playerEntity;
        }

        void onClientLeave(buint32 clientId) __blib_override
        {
            (void)clientId;
            ++this->leaveCount;
        }

        void onClientCommand(buint32 clientId,
            _In const buint8* payload, buint32 payloadSize) __blib_override
        {
            (void)clientId;
            ++this->commandCount;
            this->lastCommandSize = payloadSize < sizeof(this->lastCommandPayload)
                ? payloadSize
                : sizeof(this->lastCommandPayload);
            std::memcpy(this->lastCommandPayload, payload, this->lastCommandSize);
        }
    };

    // Сон в цикле опроса неблокирующего сокета (паттерн blib network test)
    constexpr buint32 pollSleepMs = 1;

    void pollSleep()
    {
        Sleep(static_cast<DWORD>(pollSleepMs));
    }

    /**
     * Неблокирующее подключение loopback-клиента (WouldBlock = в процессе).
     */
    bool connectNonBlocking(_In blib::network::TcpSocket& client, _In_Out blib::network::address::Tcp& endpoint)
    {
        for (buint32 attempt = 0; attempt < 1000; ++attempt)
        {
            const blib::network::SocketStatus status = client.connect(endpoint);
            if (status == blib::network::SocketStatus::OK)
            {
                return true;
            }
            if (status != blib::network::SocketStatus::WouldBlock)
            {
                return false;
            }
            pollSleep();
        }
        return false;
    }

    /**
     * Вычитать сообщение из сокета через фреймер (с таймаутом).
     */
    bool readMessage(_In blib::network::TcpSocket& client, _In_Out beng::ReplicationFramer& framer,
        _Out beng::ReplicationPacketType& outType,
        _Out buint8* outPayload, buint32 payloadCapacity, _Out buint32& outPayloadSize,
        buint32 maxAttempts)
    {
        buint8 buffer[beng::maxReplicationPacketBytes];
        for (buint32 attempt = 0; attempt < maxAttempts; ++attempt)
        {
            int received = static_cast<int>(sizeof(buffer));
            const blib::network::SocketStatus status = client.recv(buffer, received);
            if (status == blib::network::SocketStatus::OK && received > 0)
            {
                if (framer.pushBytes(buffer, static_cast<buint32>(received)) &&
                    framer.nextMessage(outPayload, payloadCapacity, outPayloadSize, outType))
                {
                    return true;
                }
            }
            pollSleep();
        }
        return false;
    }
}

BLIB_TEST_CASE("server: replication manager spawn/delta/destroy")
{
    beng::Scene scene;
    beng::ReplicationSchema schema;
    schema.build(scene);

    beng::server::ReplicationManager manager;
    manager.initialize(schema);
    manager.onClientConnected(0);

    // Статический объект + юнит (Transform-only: position реплицируется)
    const beng::EntityID staticEntity = scene.createEntity();
    scene.getComponent<beng::TransformComponent>(staticEntity).setLocalPosition(
        blib::math::Vector<float, 3>(0.0f, 0.0f, 0.0f));
    const beng::EntityID unit = scene.createEntity();
    scene.getComponent<beng::TransformComponent>(unit).setLocalPosition(
        blib::math::Vector<float, 3>(1.0f, 2.0f, 3.0f));

    beng::ReplicationEntityPatch patches[beng::maxSnapshotEntities];
    buint32 entityCount = 0;

    // Первый снапшот: оба — полные (клиент ничего не знает)
    BLIB_TEST_CHECK(manager.buildSnapshot(0, scene, 1, false, patches, entityCount));
    BLIB_TEST_CHECK(entityCount == 2);
    BLIB_TEST_CHECK((patches[0].flags & beng::replicationEntityFull) != 0);
    BLIB_TEST_CHECK((patches[1].flags & beng::replicationEntityFull) != 0);

    // Без изменений: пустой снапшот (нет патчей)
    BLIB_TEST_CHECK(manager.buildSnapshot(0, scene, 2, false, patches, entityCount));
    BLIB_TEST_CHECK(entityCount == 0);

    // Движение юнита: дельта с одним компонентом
    scene.getComponent<beng::TransformComponent>(unit).setLocalPosition(
        blib::math::Vector<float, 3>(4.0f, 2.0f, 3.0f));
    BLIB_TEST_CHECK(manager.buildSnapshot(0, scene, 3, false, patches, entityCount));
    BLIB_TEST_CHECK(entityCount == 1);
    BLIB_TEST_CHECK(patches[0].entityId == unit);
    BLIB_TEST_CHECK((patches[0].flags & beng::replicationEntityFull) == 0);
    BLIB_TEST_CHECK(patches[0].componentCount == 1);

    // Уничтожение юнита: destroy-патч
    scene.destroyEntity(unit);
    BLIB_TEST_CHECK(manager.buildSnapshot(0, scene, 4, false, patches, entityCount));
    BLIB_TEST_CHECK(entityCount == 1);
    BLIB_TEST_CHECK(patches[0].entityId == unit);
    BLIB_TEST_CHECK((patches[0].flags & beng::replicationEntityDestroy) != 0);

    // Второй клиент подключается после движения: его зеркало пусто —
    // снапшот полностью полный (статический объект)
    manager.onClientConnected(1);
    BLIB_TEST_CHECK(manager.buildSnapshot(1, scene, 5, false, patches, entityCount));
    BLIB_TEST_CHECK(entityCount == 1);
    BLIB_TEST_CHECK((patches[0].flags & beng::replicationEntityFull) != 0);
    BLIB_TEST_CHECK(patches[0].entityId == staticEntity);
}

BLIB_TEST_CASE("server: world manager saves and loads world")
{
    FakeServerGame game;
    beng::server::ServerApplication server;
    BLIB_TEST_CHECK(server.initialize(game, FakeServerGame::fakePort));

    beng::server::WorldManager& world = server.getWorldManager();

    // Сцену строим сами: два Transform-объекта с разными позициями
    beng::Scene& scene = server.getScene();
    scene.reset();
    const beng::EntityID a = scene.createEntity();
    scene.getComponent<beng::TransformComponent>(a).setLocalPosition(
        blib::math::Vector<float, 3>(1.0f, 0.0f, 0.0f));
    const beng::EntityID b = scene.createEntity();
    scene.getComponent<beng::TransformComponent>(b).setLocalPosition(
        blib::math::Vector<float, 3>(0.0f, 5.0f, 0.0f));

    // Сохранить в temp-файл
    const char* path = "test_world_scene.json";
    BLIB_TEST_CHECK(world.saveWorld(path));

    // Сброс + загрузка
    world.buildWorld();
    BLIB_TEST_CHECK(world.loadWorld(path));

    // Мир восстановлен: две сущности, позиции совпадают
    BLIB_TEST_CHECK(scene.getEntityCount() == 2);
    const beng::TransformComponent* ta = scene.tryGetComponent<beng::TransformComponent>(a);
    const beng::TransformComponent* tb = scene.tryGetComponent<beng::TransformComponent>(b);
    BLIB_TEST_CHECK(ta != nullptr && tb != nullptr);
    BLIB_TEST_CHECK_CLOSE(ta->getLocalPosition().x, 1.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(tb->getLocalPosition().y, 5.0f, 0.0001f);

    server.shutdown();
    std::remove(path);
}

BLIB_TEST_CASE("server: application loopback welcome/command/snapshot")
{
    FakeServerGame game;
    beng::server::ServerApplication server;
    BLIB_TEST_CHECK(server.initialize(game, FakeServerGame::fakePort));

    // ===== Loopback-клиент (сокет создаётся typed-конструктором) =====
    blib::network::TcpSocket client(blib::network::address::AddressType::IPv4);
    BLIB_TEST_CHECK(client.setBlocking(false));

    blib::network::address::Tcp endpoint;
    endpoint.ip = blib::network::address::Address::LocalhostIPv4;
    endpoint.port = static_cast<buint16>(FakeServerGame::fakePort);

    // Подключение + серверные тики (accept в poll) — в одном потоке
    bool connected = false;
    for (buint32 attempt = 0; attempt < 3000 && !connected; ++attempt)
    {
        server.tick();
        const blib::network::SocketStatus status = client.connect(endpoint);
        if (status == blib::network::SocketStatus::OK)
        {
            connected = true;
        }
        else if (status != blib::network::SocketStatus::WouldBlock)
        {
            break;
        }
        pollSleep();
    }
    BLIB_TEST_CHECK(connected);

    beng::ReplicationFramer framer;
    buint8 payload[beng::maxReplicationPacketBytes];
    buint32 payloadSize = 0;
    beng::ReplicationPacketType type = beng::ReplicationPacketType::None;

    // ===== Welcome =====
    bool gotWelcome = false;
    beng::DecodedReplicationWelcome welcome;
    for (buint32 attempt = 0; attempt < 3000 && !gotWelcome; ++attempt)
    {
        server.tick();
        if (readMessage(client, framer, type, payload, sizeof(payload), payloadSize, 1))
        {
            if (type == beng::ReplicationPacketType::Welcome)
            {
                gotWelcome = beng::decodeReplicationWelcome(payload, payloadSize, welcome);
            }
        }
        pollSleep();
    }
    BLIB_TEST_CHECK(gotWelcome);
    BLIB_TEST_CHECK(welcome.tickRate == FakeServerGame::fakeTickRate);
    BLIB_TEST_CHECK(welcome.playerEntityId != beng::invalidEntity);
    BLIB_TEST_CHECK(game.joinCount == 1);

    // ===== Команда клиента (payload — непрозрачные байты) =====
    const buint8 commandPayload[2] = { 0xAB, 0xCD };
    const buint32 packetSize = beng::replicationHeaderSize + 2;
    buint8 packet[16];
    packet[0] = static_cast<buint8>(beng::ReplicationPacketType::Command);
    packet[1] = 2;
    packet[2] = 0;
    packet[3] = commandPayload[0];
    packet[4] = commandPayload[1];

    int sent = 0;
    bool commandSent = false;
    for (buint32 attempt = 0; attempt < 3000 && !commandSent; ++attempt)
    {
        const blib::network::SocketStatus status = client.send(packet, static_cast<int>(packetSize), &sent);
        if (status == blib::network::SocketStatus::OK && sent == static_cast<int>(packetSize))
        {
            commandSent = true;
        }
        pollSleep();
    }
    BLIB_TEST_CHECK(commandSent);

    // Сервер обрабатывает команду за несколько тиков
    for (buint32 attempt = 0; attempt < 3000 && game.commandCount == 0; ++attempt)
    {
        server.tick();
        pollSleep();
    }
    BLIB_TEST_CHECK(game.commandCount == 1);
    BLIB_TEST_CHECK(game.lastCommandSize == 2);
    BLIB_TEST_CHECK(game.lastCommandPayload[0] == 0xAB && game.lastCommandPayload[1] == 0xCD);

    // ===== Снапшот после тика (60 Гц — ждём до ~2 секунд) =====
    bool gotSnapshot = false;
    beng::DecodedReplicationSnapshot snapshot;
    for (buint32 attempt = 0; attempt < 3000 && !gotSnapshot; ++attempt)
    {
        server.tick();
        if (readMessage(client, framer, type, payload, sizeof(payload), payloadSize, 1))
        {
            if (type == beng::ReplicationPacketType::Snapshot)
            {
                gotSnapshot = beng::decodeReplicationSnapshot(payload, payloadSize, snapshot);
            }
        }
        pollSleep();
    }
    BLIB_TEST_CHECK(gotSnapshot);

    // Снапшот содержит статический объект и игрока (полные патчи)
    buint32 fullEntityCount = 0;
    bool playerFound = false;
    for (buint32 e = 0; e < snapshot.entityCount; ++e)
    {
        if (snapshot.entities[e].full)
        {
            ++fullEntityCount;
        }
        if (snapshot.entities[e].entityId == welcome.playerEntityId)
        {
            playerFound = true;
        }
    }
    BLIB_TEST_CHECK(fullEntityCount >= 1);
    BLIB_TEST_CHECK(playerFound);

    // ===== Отключение клиента =====
    client.getSocket()->close();
    for (buint32 attempt = 0; attempt < 3000 && game.leaveCount == 0; ++attempt)
    {
        server.tick();
        pollSleep();
    }
    BLIB_TEST_CHECK(game.leaveCount == 1);

    server.shutdown();
}

BLIB_TEST_CASE("server: replication client loopback mirror")
{
    FakeServerGame game;
    beng::server::ServerApplication server;
    BLIB_TEST_CHECK(server.initialize(game, FakeServerGame::fakePort));

    // Mirror-сцена клиента (только движковые типы — их же регистрирует
    // игра на сервере: fake-игра своих типов не добавляет)
    beng::Scene clientScene;

    // Клиент репликации: connect → poll в одном потоке с сервером
    beng::ReplicationClient client;
    client.connect(FakeServerGame::fakePort);

    // Дождаться открытия сессии (Welcome → сверка схем → маппинг)
    for (buint32 attempt = 0; attempt < 3000 && !client.isSessionReady(); ++attempt)
    {
        server.tick();
        client.poll(clientScene);
        pollSleep();
    }
    BLIB_TEST_CHECK(client.isSessionReady());
    BLIB_TEST_CHECK(client.getConnection() == beng::ReplicationClient::Connection::Connected);
    BLIB_TEST_CHECK(client.getServerTickRate() == FakeServerGame::fakeTickRate);
    BLIB_TEST_CHECK(client.getPlayerEntityId() != beng::invalidEntity);

    // Зеркало игрока: серверный ID, позиция из onClientJoin (1,0,0)
    const beng::EntityID playerEntity = client.getPlayerEntityId();
    for (buint32 attempt = 0;
        attempt < 3000 && clientScene.tryGetComponent<beng::TransformComponent>(playerEntity) == nullptr;
        ++attempt)
    {
        server.tick();
        client.poll(clientScene);
        pollSleep();
    }

    const beng::TransformComponent* mirrorTransform =
        clientScene.tryGetComponent<beng::TransformComponent>(playerEntity);
    BLIB_TEST_CHECK(mirrorTransform != nullptr);
    BLIB_TEST_CHECK_CLOSE(mirrorTransform->getLocalPosition().x, 1.0f, 0.0001f);

    // Команда игрока (непрозрачный payload) доходит до игры сервера
    const buint8 commandPayload[2] = { 0x11, 0x22 };
    bool commandSent = false;
    for (buint32 attempt = 0; attempt < 3000 && !commandSent; ++attempt)
    {
        server.tick();
        client.poll(clientScene);
        commandSent = client.sendCommand(commandPayload, 2);
        pollSleep();
    }
    BLIB_TEST_CHECK(commandSent);

    for (buint32 attempt = 0; attempt < 3000 && game.commandCount == 0; ++attempt)
    {
        server.tick();
        client.poll(clientScene);
        pollSleep();
    }
    BLIB_TEST_CHECK(game.commandCount == 1);
    BLIB_TEST_CHECK(game.lastCommandSize == 2);
    BLIB_TEST_CHECK(game.lastCommandPayload[0] == 0x11 && game.lastCommandPayload[1] == 0x22);

    // Destroy игрока на сервере → зеркало удаляется
    server.getScene().destroyEntity(playerEntity);
    for (buint32 attempt = 0;
        attempt < 3000 && clientScene.tryGetComponent<beng::TransformComponent>(playerEntity) != nullptr;
        ++attempt)
    {
        server.tick();
        client.poll(clientScene);
        pollSleep();
    }
    BLIB_TEST_CHECK(clientScene.tryGetComponent<beng::TransformComponent>(playerEntity) == nullptr);
    // В зеркале остаётся статический объект мира (onWorldBuild)
    BLIB_TEST_CHECK(client.getMirror().getKnownEntityCount() == 1);

    client.shutdown();
    server.shutdown();
}
