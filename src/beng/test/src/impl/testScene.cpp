#include <blib/test/src/test.h>

#include <beng/core/scene.h>
#include <beng/core/system.h>

#include <vector>

// Локальные типы компонентов для тестов Scene
struct SceneTestComponentA : public beng::IComponent
{
    static constexpr const char* componentTypeName = "test.SceneA";

    using blib::core::IStrongComparable::strongCompare;

    explicit SceneTestComponentA(bint32 v)
        : value(v)
    {
    }

    bool strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const __blib_override
    {
        if (!session.enter(this, &other))
        {
            return true;
        }
        const SceneTestComponentA& o = static_cast<const SceneTestComponentA&>(other);
        return getOwnerId() == o.getOwnerId() && isActive == o.isActive && value == o.value;
    }

    // Не сериализуем (save/load Unsupported) — round-trip неприменим
    bool verify() const __blib_override
    {
        return false;
    }

    bint32 value;
};

struct SceneTestComponentB : public beng::IComponent
{
    static constexpr const char* componentTypeName = "test.SceneB";

    using blib::core::IStrongComparable::strongCompare;

    explicit SceneTestComponentB(float v)
        : value(v)
    {
    }

    bool strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const __blib_override
    {
        if (!session.enter(this, &other))
        {
            return true;
        }
        const SceneTestComponentB& o = static_cast<const SceneTestComponentB&>(other);
        return getOwnerId() == o.getOwnerId() && isActive == o.isActive && value == o.value;
    }

    // Не сериализуем (save/load Unsupported) — round-trip неприменим
    bool verify() const __blib_override
    {
        return false;
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

BLIB_TEST_CASE("scene: isRegisteredComponentType guards against duplicate registration")
{
    beng::Scene scene;

    // До регистрации тип не зарегистрирован
    BLIB_TEST_CHECK(!scene.isRegisteredComponentType<SceneTestComponentA>());

    scene.registerComponentType<SceneTestComponentA>();

    // После — зарегистрирован; guard-паттерн делает повторную
    // регистрацию идемпотентной (повторная регистрация имени —
    // fatal, проверить его в фреймворке нельзя: abort убивает процесс)
    BLIB_TEST_CHECK(scene.isRegisteredComponentType<SceneTestComponentA>());
    if (!scene.isRegisteredComponentType<SceneTestComponentA>())
    {
        scene.registerComponentType<SceneTestComponentA>();
    }

    // Другой тип в той же сцене — независимо
    BLIB_TEST_CHECK(!scene.isRegisteredComponentType<SceneTestComponentB>());

    // Пул доступен и работает
    beng::EntityID id = scene.createEntity();
    scene.addComponent<SceneTestComponentA>(id, 42);

    BLIB_TEST_CHECK(scene.getComponentPool<SceneTestComponentA>().size() == 1);
}

BLIB_TEST_CASE("scene: reset() clears entities but keeps type registry and systems")
{
    beng::Scene scene;
    scene.registerComponentType<SceneTestComponentB>();

    // Система: после reset() список систем сохраняется (Scene::reset
    // не трогает systems — хост не должен перевешивать их заново)
    std::vector<bint32> callLog;
    RecordingSystem sys(5, &callLog);
    scene.addSystem(&sys);

    // Данные: сущность + компонент
    beng::EntityID id = scene.createEntity();
    scene.addComponent<SceneTestComponentB>(id, 2.0f);
    BLIB_TEST_CHECK(scene.getEntityCount() == 1);
    BLIB_TEST_CHECK(scene.tryGetComponent<SceneTestComponentB>(id) != nullptr);

    scene.reset();

    // Сцена пуста...
    BLIB_TEST_CHECK(scene.getEntityCount() == 0);
    BLIB_TEST_CHECK(scene.tryGetComponent<SceneTestComponentB>(id) == nullptr);

    // ...но реестр типов жив (пере-регистрация не требуется)
    BLIB_TEST_CHECK(scene.isRegisteredComponentType<SceneTestComponentB>());
    BLIB_TEST_CHECK(scene.getComponentTypeCount() == 2); // Transform + B

    // Системы живы и вызываются
    BLIB_TEST_CHECK(scene.getSystemCount() == 1);
    scene.update(0.016f);
    BLIB_TEST_CHECK(callLog.size() == 1);

    // Пулы пересозданы фабриками: добавление компонентов работает
    beng::EntityID next = scene.createEntity();
    BLIB_TEST_CHECK(next != beng::invalidEntity);
    scene.addComponent<SceneTestComponentB>(next, 3.0f);
    BLIB_TEST_CHECK(scene.tryGetComponent<SceneTestComponentB>(next) != nullptr);
    BLIB_TEST_CHECK(scene.getEntityCount() == 1);
}
