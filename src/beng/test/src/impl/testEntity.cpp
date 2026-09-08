#include <blib/test/src/test.h>

#include <beng/core/scene.h>

// Локальный тип компонента для тестов Scene API
struct EntityTestComponent : public beng::IComponent
{
    explicit EntityTestComponent(bint32 v)
        : value(v)
    {
    }

    bint32 value;
};

BLIB_TEST_CASE("entity api: addComponent returns component with owner bound")
{
    beng::Scene scene;
    scene.registerComponentType<EntityTestComponent>();

    beng::EntityID id = scene.createEntity();
    EntityTestComponent& comp = scene.addComponent<EntityTestComponent>(id, 7);

    BLIB_TEST_CHECK(comp.value == 7);
    BLIB_TEST_CHECK(comp.getOwnerId() == id);
    BLIB_TEST_CHECK(scene.hasComponent<EntityTestComponent>(id));
}

BLIB_TEST_CASE("entity api: getComponent returns the same instance as addComponent")
{
    beng::Scene scene;
    scene.registerComponentType<EntityTestComponent>();

    beng::EntityID id = scene.createEntity();
    EntityTestComponent& added = scene.addComponent<EntityTestComponent>(id, 5);
    EntityTestComponent& fetched = scene.getComponent<EntityTestComponent>(id);

    BLIB_TEST_CHECK(&added == &fetched);
}

BLIB_TEST_CASE("entity api: tryGetComponent returns nullptr for missing entity or component")
{
    beng::Scene scene;
    scene.registerComponentType<EntityTestComponent>();

    beng::EntityID withComponent = scene.createEntity();
    beng::EntityID withoutComponent = scene.createEntity();
    scene.addComponent<EntityTestComponent>(withComponent, 1);

    BLIB_TEST_CHECK(scene.tryGetComponent<EntityTestComponent>(withComponent) != nullptr);
    BLIB_TEST_CHECK(scene.tryGetComponent<EntityTestComponent>(withoutComponent) == nullptr);
    BLIB_TEST_CHECK(scene.tryGetComponent<EntityTestComponent>(9999) == nullptr);
}

BLIB_TEST_CASE("entity api: resolveComponent returns existing or creates new")
{
    beng::Scene scene;
    scene.registerComponentType<EntityTestComponent>();

    beng::EntityID existing = scene.createEntity();
    EntityTestComponent& added = scene.addComponent<EntityTestComponent>(existing, 10);

    // Компонент есть — resolve возвращает его, не создавая новый
    EntityTestComponent& resolved = scene.resolveComponent<EntityTestComponent>(existing, 99);
    BLIB_TEST_CHECK(&resolved == &added);
    BLIB_TEST_CHECK(resolved.value == 10); // значение не перезатёрто

    // Компонента нет — resolve создаёт новый
    beng::EntityID fresh = scene.createEntity();
    EntityTestComponent& created = scene.resolveComponent<EntityTestComponent>(fresh, 20);
    BLIB_TEST_CHECK(created.value == 20);
    BLIB_TEST_CHECK(scene.hasComponent<EntityTestComponent>(fresh));
}

BLIB_TEST_CASE("entity api: removeComponent removes from mask and pool")
{
    beng::Scene scene;
    scene.registerComponentType<EntityTestComponent>();

    beng::EntityID id = scene.createEntity();
    scene.addComponent<EntityTestComponent>(id, 1);

    scene.removeComponent<EntityTestComponent>(id);

    BLIB_TEST_CHECK(!scene.hasComponent<EntityTestComponent>(id));
    BLIB_TEST_CHECK(scene.tryGetComponent<EntityTestComponent>(id) == nullptr);
    BLIB_TEST_CHECK(scene.getComponentPool<EntityTestComponent>().size() == 0);

    // Повторное удаление — no-op
    scene.removeComponent<EntityTestComponent>(id);
    BLIB_TEST_CHECK(!scene.hasComponent<EntityTestComponent>(id));
}

BLIB_TEST_CASE("entity api: component pointers stay stable while pool grows")
{
    beng::Scene scene;
    scene.registerComponentType<EntityTestComponent>();

    beng::EntityID first = scene.createEntity();
    EntityTestComponent* firstComp = &scene.addComponent<EntityTestComponent>(first, 1);

    // Много новых Entity с компонентами — пул растёт, но T не двигаются
    constexpr beng::EntityID addCount = 300;
    for (beng::EntityID i = 0; i < addCount; ++i)
    {
        beng::EntityID id = scene.createEntity();
        scene.addComponent<EntityTestComponent>(id, static_cast<bint32>(i));
    }

    BLIB_TEST_CHECK(scene.getComponentPool<EntityTestComponent>().get(first) == firstComp);
    BLIB_TEST_CHECK(firstComp->getOwnerId() == first);
}
