#include <blib/test/src/test.h>

#include <beng/core/componentPool.h>

// Локальный тип компонента для тестов пула
struct PoolTestComponent : public beng::IComponent
{
    explicit PoolTestComponent(float v)
        : value(v)
    {
    }

    float value;
};

BLIB_TEST_CASE("pool: create returns pointer and binds owner id")
{
    beng::ComponentPool<PoolTestComponent> pool(8);

    beng::EntityID entityId = 42;
    PoolTestComponent* comp = pool.create(entityId, 3.5f);

    BLIB_TEST_REQUIRE(comp != nullptr);
    BLIB_TEST_CHECK(pool.size() == 1);
    BLIB_TEST_CHECK(!pool.empty());
    BLIB_TEST_CHECK(comp->getOwnerId() == entityId);
}

BLIB_TEST_CASE("pool: get returns the same component as create")
{
    beng::ComponentPool<PoolTestComponent> pool(8);

    beng::EntityID entityId = 7;
    PoolTestComponent* created = pool.create(entityId, 1.0f);
    PoolTestComponent* fetched = pool.get(entityId);

    BLIB_TEST_REQUIRE(created != nullptr);
    BLIB_TEST_CHECK(fetched == created);
}

BLIB_TEST_CASE("pool: get returns nullptr for missing entity")
{
    beng::ComponentPool<PoolTestComponent> pool(8);

    BLIB_TEST_CHECK(pool.get(123) == nullptr);
    BLIB_TEST_CHECK(pool.get(beng::invalidEntity) == nullptr);
}

BLIB_TEST_CASE("pool: duplicate create returns existing component")
{
    beng::ComponentPool<PoolTestComponent> pool(8);

    beng::EntityID entityId = 5;
    PoolTestComponent* first = pool.create(entityId, 1.0f);
    PoolTestComponent* second = pool.create(entityId, 99.0f);

    BLIB_TEST_REQUIRE(first != nullptr);
    BLIB_TEST_CHECK(second == first);
    BLIB_TEST_CHECK(pool.size() == 1);
}

BLIB_TEST_CASE("pool: destroy removes component and frees slot")
{
    beng::ComponentPool<PoolTestComponent> pool(8);

    beng::EntityID entityId = 9;
    pool.create(entityId, 1.0f);
    pool.destroy(entityId);

    BLIB_TEST_CHECK(pool.size() == 0);
    BLIB_TEST_CHECK(pool.empty());
    BLIB_TEST_CHECK(pool.get(entityId) == nullptr);
}

BLIB_TEST_CASE("pool: destroy missing component is a no-op")
{
    beng::ComponentPool<PoolTestComponent> pool(8);

    pool.create(1, 1.0f);
    pool.destroy(999);

    BLIB_TEST_CHECK(pool.size() == 1);
    BLIB_TEST_CHECK(pool.get(1) != nullptr);
}

BLIB_TEST_CASE("pool: destroy middle keeps sparse/dense mapping intact (swap-and-pop)")
{
    beng::ComponentPool<PoolTestComponent> pool(8);

    // Три компонента; удаляем средний — последний встаёт на его место
    pool.create(10, 10.0f);
    pool.create(20, 20.0f);
    pool.create(30, 30.0f);

    pool.destroy(20);

    BLIB_TEST_REQUIRE(pool.size() == 2);
    BLIB_TEST_CHECK(pool.get(20) == nullptr);

    // Выжившие компоненты доступны и связаны с правильными EntityID
    PoolTestComponent* first = pool.get(10);
    PoolTestComponent* last = pool.get(30);
    BLIB_TEST_REQUIRE(first != nullptr);
    BLIB_TEST_REQUIRE(last != nullptr);
    BLIB_TEST_CHECK(first->getOwnerId() == 10);
    BLIB_TEST_CHECK(last->getOwnerId() == 30);

    // Инвариант sparse set: для любого dense-индекса getEntityId →
    // get должен возвращать тот же компонент, что и getByIndex
    for (buint32 i = 0; i < pool.size(); ++i)
    {
        beng::EntityID id = pool.getEntityId(i);
        BLIB_TEST_CHECK(pool.get(id) == pool.getByIndex(i));
    }
}

BLIB_TEST_CASE("pool: destroy all then create again reuses storage correctly")
{
    beng::ComponentPool<PoolTestComponent> pool(8);

    pool.create(1, 1.0f);
    pool.create(2, 2.0f);
    pool.create(3, 3.0f);

    pool.destroy(1);
    pool.destroy(2);
    pool.destroy(3);

    BLIB_TEST_CHECK(pool.empty());

    PoolTestComponent* fresh = pool.create(4, 4.0f);
    BLIB_TEST_REQUIRE(fresh != nullptr);
    BLIB_TEST_CHECK(pool.size() == 1);
    BLIB_TEST_CHECK(pool.get(4) == fresh);
}

BLIB_TEST_CASE("pool: component storage exceeds chunk size (grows)")
{
    constexpr buint32 smallChunk = 4;
    beng::ComponentPool<PoolTestComponent> pool(smallChunk);

    // Создаём больше компонентов, чем в одном chunk
    constexpr beng::EntityID entityCount = 10;
    for (beng::EntityID id = 1; id <= entityCount; ++id)
    {
        PoolTestComponent* comp = pool.create(id, static_cast<float>(id));
        BLIB_TEST_REQUIRE(comp != nullptr);
    }

    BLIB_TEST_CHECK(pool.size() == entityCount);

    // Все компоненты доступны по своим EntityID
    for (beng::EntityID id = 1; id <= entityCount; ++id)
    {
        PoolTestComponent* comp = pool.get(id);
        BLIB_TEST_REQUIRE(comp != nullptr);
        BLIB_TEST_CHECK(comp->getOwnerId() == id);
        BLIB_TEST_CHECK(comp->value == static_cast<float>(id));
    }
}
