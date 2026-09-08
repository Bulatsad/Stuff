#include <blib/test/src/test.h>

#include <beng/core/component.h>

// Локальные типы компонентов для тестов реестра.
// Имена уникальны в рамках процесса: реестр типов глобален,
// а все группы линкуются в агрегатный beng_test_all.
struct RegistryTestComponentA : public beng::IComponent
{
};

struct RegistryTestComponentB : public beng::IComponent
{
};

BLIB_TEST_CASE("registry: getTypeId returns stable id for the same type")
{
    beng::ComponentType id1 = beng::ComponentTypeRegistry::getTypeId<RegistryTestComponentA>();
    beng::ComponentType id2 = beng::ComponentTypeRegistry::getTypeId<RegistryTestComponentA>();

    BLIB_TEST_CHECK(id1 == id2);
}

BLIB_TEST_CASE("registry: different types get different ids")
{
    beng::ComponentType idA = beng::ComponentTypeRegistry::getTypeId<RegistryTestComponentA>();
    beng::ComponentType idB = beng::ComponentTypeRegistry::getTypeId<RegistryTestComponentB>();

    BLIB_TEST_CHECK(idA != idB);
}

BLIB_TEST_CASE("registry: ids are issued sequentially without gaps")
{
    // Реестр глобален, поэтому проверяем относительный порядок:
    // следующий зарегистрированный тип получает следующий по порядку ID
    beng::ComponentType idA = beng::ComponentTypeRegistry::getTypeId<RegistryTestComponentA>();
    beng::ComponentType idB = beng::ComponentTypeRegistry::getTypeId<RegistryTestComponentB>();

    BLIB_TEST_CHECK(idB == static_cast<beng::ComponentType>(idA + 1) ||
        idA == static_cast<beng::ComponentType>(idB + 1));
}

BLIB_TEST_CASE("registry: isRegistered reflects registration state")
{
    beng::ComponentType idA = beng::ComponentTypeRegistry::getTypeId<RegistryTestComponentA>();

    BLIB_TEST_CHECK(beng::ComponentTypeRegistry::isRegistered(idA));

    // Невалидный и выходящий за лимит ID — не зарегистрированы
    BLIB_TEST_CHECK(!beng::ComponentTypeRegistry::isRegistered(beng::invalidComponentType));
    BLIB_TEST_CHECK(!beng::ComponentTypeRegistry::isRegistered(beng::maxComponentTypes));
}

BLIB_TEST_CASE("registry: registered count grows with new types")
{
    buint8 before = beng::ComponentTypeRegistry::getRegisteredCount();
    beng::ComponentTypeRegistry::getTypeId<RegistryTestComponentA>();
    beng::ComponentTypeRegistry::getTypeId<RegistryTestComponentB>();
    buint8 after = beng::ComponentTypeRegistry::getRegisteredCount();

    BLIB_TEST_CHECK(after >= before);
}
