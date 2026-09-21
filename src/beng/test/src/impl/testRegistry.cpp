#include <blib/test/src/test.h>

#include <beng/core/scene.h>

// Локальные типы компонентов для тестов таблицы типов Scene.
// Имя типа — статический член класса (идентичность типа в сцене);
// имена уникальны в рамках процесса — реестр типов теперь per-scene.
struct RegistryTestComponentA : public beng::IComponent
{
    static constexpr const char* componentTypeName = "test.RegistryA";
};

struct RegistryTestComponentB : public beng::IComponent
{
    static constexpr const char* componentTypeName = "test.RegistryB";
};

BLIB_TEST_CASE("registry: isRegisteredComponentType reflects registration state")
{
    beng::Scene scene;

    // До регистрации — не зарегистрирован
    BLIB_TEST_CHECK(!scene.isRegisteredComponentType<RegistryTestComponentA>());
    BLIB_TEST_CHECK(!scene.isRegisteredComponentType<RegistryTestComponentB>());

    scene.registerComponentType<RegistryTestComponentA>();

    BLIB_TEST_CHECK(scene.isRegisteredComponentType<RegistryTestComponentA>());
    BLIB_TEST_CHECK(!scene.isRegisteredComponentType<RegistryTestComponentB>());
}

BLIB_TEST_CASE("registry: guard pattern makes registration idempotent")
{
    beng::Scene scene;

    // Повторная регистрация имени в сцене — fatal (abort), поэтому
    // идемпотентность достигается guard-паттерном; повторный вызов
    // под guard не выполняется и не роняет процесс
    if (!scene.isRegisteredComponentType<RegistryTestComponentA>())
    {
        scene.registerComponentType<RegistryTestComponentA>();
    }
    if (!scene.isRegisteredComponentType<RegistryTestComponentA>())
    {
        scene.registerComponentType<RegistryTestComponentA>();
    }

    BLIB_TEST_CHECK(scene.isRegisteredComponentType<RegistryTestComponentA>());
}

BLIB_TEST_CASE("registry: different types get distinct pools in one scene")
{
    beng::Scene scene;
    scene.registerComponentType<RegistryTestComponentA>();
    scene.registerComponentType<RegistryTestComponentB>();

    beng::ComponentPool<RegistryTestComponentA>& poolA =
        scene.getComponentPool<RegistryTestComponentA>();
    beng::ComponentPool<RegistryTestComponentB>& poolB =
        scene.getComponentPool<RegistryTestComponentB>();

    // Пул каждого типа свой: шаблонный резолв по имени не перепутал типы
    BLIB_TEST_CHECK(static_cast<void*>(&poolA) != static_cast<void*>(&poolB));
}

BLIB_TEST_CASE("registry: registration is per-scene")
{
    beng::Scene sceneOne;
    beng::Scene sceneTwo;

    // Тип регистрируется только для текущей сцены — глобального реестра нет
    sceneOne.registerComponentType<RegistryTestComponentA>();
    BLIB_TEST_CHECK(sceneOne.isRegisteredComponentType<RegistryTestComponentA>());
    BLIB_TEST_CHECK(!sceneTwo.isRegisteredComponentType<RegistryTestComponentA>());

    // Та же регистрация в другой сцене — легальна (словарь типов свой)
    sceneTwo.registerComponentType<RegistryTestComponentA>();
    BLIB_TEST_CHECK(sceneTwo.isRegisteredComponentType<RegistryTestComponentA>());
}

BLIB_TEST_CASE("registry: masks of two scenes with same types are independent")
{
    beng::Scene sceneOne;
    beng::Scene sceneTwo;

    sceneOne.registerComponentType<RegistryTestComponentA>();
    sceneTwo.registerComponentType<RegistryTestComponentA>();

    beng::EntityID idOne = sceneOne.createEntity();
    sceneOne.addComponent<RegistryTestComponentA>(idOne);

    // Во второй сцене тип зарегистрирован, но у Entity компонента нет
    beng::EntityID idTwo = sceneTwo.createEntity();
    BLIB_TEST_CHECK(sceneOne.hasComponent<RegistryTestComponentA>(idOne));
    BLIB_TEST_CHECK(!sceneTwo.hasComponent<RegistryTestComponentA>(idTwo));
}
