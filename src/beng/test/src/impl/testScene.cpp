#include <blib/test/src/test.h>

#include <beng/core/scene.h>
#include <beng/core/system.h>

#include <vector>

// Локальные типы компонентов для тестов Scene
struct SceneTestComponentA : public beng::IComponent
{
    explicit SceneTestComponentA(bint32 v)
        : value(v)
    {
    }

    bint32 value;
};

struct SceneTestComponentB : public beng::IComponent
{
    explicit SceneTestComponentB(float v)
        : value(v)
    {
    }

    float value;
};

// Система, записывающая порядок вызовов update
struct RecordingSystem : public beng::ISystem
{
    bint32 priority;
    std::vector<bint32>* callLog; // указатель на лог (живёт в тесте)

    RecordingSystem(bint32 p, std::vector<bint32>* log)
        : priority(p)
        , callLog(log)
    {
    }

    void update(_In beng::Scene& scene, float deltaTime) __blib_override
    {
        (void)scene;
        (void)deltaTime;
        callLog->push_back(priority);
    }

    bint32 getPriority() const __blib_override { return priority; }

    const char* getName() const __blib_override { return "RecordingSystem"; }
};

BLIB_TEST_CASE("scene: createEntity issues unique ids and grows count")
{
    beng::Scene scene;

    beng::EntityID first = scene.createEntity();
    beng::EntityID second = scene.createEntity();

    BLIB_TEST_CHECK(first != second);
    BLIB_TEST_CHECK(first != beng::invalidEntity);
    BLIB_TEST_CHECK(scene.getEntityCount() == 2);
}

BLIB_TEST_CASE("scene: destroyed entity ids are never reused")
{
    beng::Scene scene;

    beng::EntityID id = scene.createEntity();
    scene.destroyEntity(id);

    beng::EntityID next = scene.createEntity();
    BLIB_TEST_CHECK(next != id);
    BLIB_TEST_CHECK(scene.getEntityCount() == 1);
}

BLIB_TEST_CASE("scene: destroyEntity removes all components from pools")
{
    beng::Scene scene;
    scene.registerComponentType<SceneTestComponentA>();
    scene.registerComponentType<SceneTestComponentB>();

    beng::EntityID id = scene.createEntity();
    scene.addComponent<SceneTestComponentA>(id, 1);
    scene.addComponent<SceneTestComponentB>(id, 2.0f);

    // Компоненты есть до удаления
    BLIB_TEST_CHECK(scene.hasComponent<SceneTestComponentA>(id));
    BLIB_TEST_CHECK(scene.hasComponent<SceneTestComponentB>(id));

    scene.destroyEntity(id);

    // Пул очищен: мёртвые компоненты не итерируются системами
    beng::ComponentPool<SceneTestComponentA>& poolA = scene.getComponentPool<SceneTestComponentA>();
    beng::ComponentPool<SceneTestComponentB>& poolB = scene.getComponentPool<SceneTestComponentB>();
    BLIB_TEST_CHECK(poolA.size() == 0);
    BLIB_TEST_CHECK(poolB.size() == 0);
    BLIB_TEST_CHECK(scene.getEntityCount() == 0);
}

BLIB_TEST_CASE("scene: destroyEntity of unknown id is a no-op")
{
    beng::Scene scene;

    beng::EntityID id = scene.createEntity();
    scene.destroyEntity(id + 1); // не существует

    BLIB_TEST_CHECK(scene.getEntityCount() == 1);
}

BLIB_TEST_CASE("scene: destroy middle entity keeps survivors' components (swap-and-pop)")
{
    beng::Scene scene;
    scene.registerComponentType<SceneTestComponentA>();

    beng::EntityID first = scene.createEntity();
    beng::EntityID second = scene.createEntity();
    beng::EntityID third = scene.createEntity();

    scene.addComponent<SceneTestComponentA>(first, 1);
    scene.addComponent<SceneTestComponentA>(second, 2);
    scene.addComponent<SceneTestComponentA>(third, 3);

    // Удаляем среднюю Entity
    scene.destroyEntity(second);

    BLIB_TEST_CHECK(scene.getEntityCount() == 2);
    BLIB_TEST_CHECK(!scene.hasComponent<SceneTestComponentA>(second));

    // Выжившие сохранили свои компоненты и значения
    BLIB_TEST_CHECK(scene.hasComponent<SceneTestComponentA>(first));
    BLIB_TEST_CHECK(scene.hasComponent<SceneTestComponentA>(third));

    SceneTestComponentA& compFirst = scene.getComponent<SceneTestComponentA>(first);
    SceneTestComponentA& compThird = scene.getComponent<SceneTestComponentA>(third);
    BLIB_TEST_CHECK(compFirst.value == 1);
    BLIB_TEST_CHECK(compThird.value == 3);
}

BLIB_TEST_CASE("scene: systems execute in priority order")
{
    beng::Scene scene;
    std::vector<bint32> callLog;

    RecordingSystem sysHigh(30, &callLog);
    RecordingSystem sysLow(10, &callLog);
    RecordingSystem sysMid(20, &callLog);

    // Добавляем в случайном порядке — сортировка должна выправить
    scene.addSystem(&sysHigh);
    scene.addSystem(&sysLow);
    scene.addSystem(&sysMid);

    scene.update(0.016f);

    BLIB_TEST_REQUIRE(callLog.size() == 3);
    BLIB_TEST_CHECK(callLog[0] == 10);
    BLIB_TEST_CHECK(callLog[1] == 20);
    BLIB_TEST_CHECK(callLog[2] == 30);
}

BLIB_TEST_CASE("scene: duplicate addSystem is ignored")
{
    beng::Scene scene;
    std::vector<bint32> callLog;

    RecordingSystem sys(0, &callLog);
    scene.addSystem(&sys);
    scene.addSystem(&sys);

    BLIB_TEST_CHECK(scene.getSystemCount() == 1);

    scene.update(0.016f);
    BLIB_TEST_CHECK(callLog.size() == 1);
}

BLIB_TEST_CASE("scene: removeSystem stops its updates")
{
    beng::Scene scene;
    std::vector<bint32> callLog;

    RecordingSystem sys(0, &callLog);
    scene.addSystem(&sys);
    scene.update(0.016f);
    BLIB_TEST_CHECK(callLog.size() == 1);

    scene.removeSystem(&sys);
    BLIB_TEST_CHECK(scene.getSystemCount() == 0);

    scene.update(0.016f);
    BLIB_TEST_CHECK(callLog.size() == 1); // больше не вызывается
}

BLIB_TEST_CASE("scene: duplicate registerComponentType is a no-op")
{
    beng::Scene scene;
    scene.registerComponentType<SceneTestComponentA>();
    scene.registerComponentType<SceneTestComponentA>();

    // Пул доступен и работает
    beng::EntityID id = scene.createEntity();
    scene.addComponent<SceneTestComponentA>(id, 42);

    BLIB_TEST_CHECK(scene.getComponentPool<SceneTestComponentA>().size() == 1);
}
