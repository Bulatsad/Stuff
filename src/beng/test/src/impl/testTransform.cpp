#include <blib/test/src/test.h>

#include <beng/core/scene.h>
#include <beng/components/transform.h>

#include <vector>

// Вспомогательная функция: позиция (x, y, z) как Vector<float, 3>
static blib::math::Vector<float, 3> makePos(float x, float y, float z)
{
    return blib::math::Vector<float, 3>(x, y, z);
}

BLIB_TEST_CASE("transform: child world position is parent + local")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID parent = scene.createEntity();
    beng::TransformComponent& parentTransform =
        scene.addComponent<beng::TransformComponent>(parent, &scene);
    parentTransform.setLocalPosition(makePos(10.0f, 0.0f, 0.0f));

    beng::EntityID child = scene.createEntity();
    beng::TransformComponent& childTransform =
        scene.addComponent<beng::TransformComponent>(child, &scene);
    childTransform.setLocalPosition(makePos(5.0f, 0.0f, 0.0f));
    childTransform.setParent(parent);

    auto world = childTransform.getWorldPosition();
    BLIB_TEST_CHECK_CLOSE(world.x, 15.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(world.y, 0.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(world.z, 0.0f, 0.001f);
}

BLIB_TEST_CASE("transform: reparent updates children lists of both parents")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID parentA = scene.createEntity();
    beng::EntityID parentB = scene.createEntity();
    scene.addComponent<beng::TransformComponent>(parentA, &scene);
    beng::TransformComponent& transformB =
        scene.addComponent<beng::TransformComponent>(parentB, &scene);

    beng::EntityID child = scene.createEntity();
    beng::TransformComponent& childTransform =
        scene.addComponent<beng::TransformComponent>(child, &scene);

    childTransform.setParent(parentA);
    beng::TransformComponent& transformA =
        scene.getComponent<beng::TransformComponent>(parentA);

    // Ребёнок в списке первого родителя
    BLIB_TEST_CHECK(transformA.getChildren().size() == 1);
    BLIB_TEST_CHECK(transformA.getChildren()[0] == child);
    BLIB_TEST_CHECK(childTransform.getParent() == parentA);

    // Перепривязываем к другому родителю
    childTransform.setParent(parentB);

    // Старый родитель больше не содержит ребёнка
    BLIB_TEST_CHECK(transformA.getChildren().size() == 0);
    // Новый родитель содержит
    BLIB_TEST_CHECK(transformB.getChildren().size() == 1);
    BLIB_TEST_CHECK(transformB.getChildren()[0] == child);
    BLIB_TEST_CHECK(childTransform.getParent() == parentB);
}

BLIB_TEST_CASE("transform: detach (setParent invalid) removes from parent children")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID parent = scene.createEntity();
    beng::TransformComponent& parentTransform =
        scene.addComponent<beng::TransformComponent>(parent, &scene);

    beng::EntityID child = scene.createEntity();
    beng::TransformComponent& childTransform =
        scene.addComponent<beng::TransformComponent>(child, &scene);
    childTransform.setParent(parent);

    BLIB_TEST_CHECK(parentTransform.getChildren().size() == 1);

    childTransform.setParent(beng::invalidEntity);

    BLIB_TEST_CHECK(parentTransform.getChildren().size() == 0);
    BLIB_TEST_CHECK(childTransform.getParent() == beng::invalidEntity);

    // После отвязки мировая позиция == локальной
    childTransform.setLocalPosition(makePos(3.0f, 4.0f, 0.0f));
    auto world = childTransform.getWorldPosition();
    BLIB_TEST_CHECK_CLOSE(world.x, 3.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(world.y, 4.0f, 0.001f);
}

BLIB_TEST_CASE("transform: moving parent after setParent updates child world position")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID parent = scene.createEntity();
    beng::TransformComponent& parentTransform =
        scene.addComponent<beng::TransformComponent>(parent, &scene);
    parentTransform.setLocalPosition(makePos(10.0f, 0.0f, 0.0f));

    beng::EntityID child = scene.createEntity();
    beng::TransformComponent& childTransform =
        scene.addComponent<beng::TransformComponent>(child, &scene);
    childTransform.setLocalPosition(makePos(5.0f, 0.0f, 0.0f));
    childTransform.setParent(parent);

    // Прогреваем кеш: читаем мировую позицию ребёнка
    auto before = childTransform.getWorldPosition();
    BLIB_TEST_CHECK_CLOSE(before.x, 15.0f, 0.001f);

    // Двигаем родителя ПОСЛЕ установки иерархии — markDirty должен
    // распространиться на ребёнка через children-список
    parentTransform.setLocalPosition(makePos(20.0f, 0.0f, 0.0f));

    auto after = childTransform.getWorldPosition();
    BLIB_TEST_CHECK_CLOSE(after.x, 25.0f, 0.001f);
}

BLIB_TEST_CASE("transform: world position is order-independent (child created before parent)")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    // Ребёнок создаётся РАНЬШЕ родителя (в dense он будет первым)
    beng::EntityID child = scene.createEntity();
    beng::TransformComponent& childTransform =
        scene.addComponent<beng::TransformComponent>(child, &scene);
    childTransform.setLocalPosition(makePos(5.0f, 0.0f, 0.0f));

    beng::EntityID parent = scene.createEntity();
    beng::TransformComponent& parentTransform =
        scene.addComponent<beng::TransformComponent>(parent, &scene);
    parentTransform.setLocalPosition(makePos(10.0f, 0.0f, 0.0f));

    childTransform.setParent(parent);

    // При пересчёте ребёнок сам тянет матрицу родителя — порядок не важен
    auto world = childTransform.getWorldPosition();
    BLIB_TEST_CHECK_CLOSE(world.x, 15.0f, 0.001f);
}

BLIB_TEST_CASE("transform: self-parent is rejected")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID entity = scene.createEntity();
    beng::TransformComponent& transform =
        scene.addComponent<beng::TransformComponent>(entity, &scene);

    transform.setParent(entity);

    BLIB_TEST_CHECK(transform.getParent() == beng::invalidEntity);
    BLIB_TEST_CHECK(transform.getChildren().size() == 0);
}

BLIB_TEST_CASE("transform: parent cycles are rejected")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID a = scene.createEntity();
    beng::EntityID b = scene.createEntity();
    beng::TransformComponent& transformA =
        scene.addComponent<beng::TransformComponent>(a, &scene);
    beng::TransformComponent& transformB =
        scene.addComponent<beng::TransformComponent>(b, &scene);

    // a ← b (b — ребёнок a)
    transformB.setParent(a);
    BLIB_TEST_CHECK(transformB.getParent() == a);

    // Попытка замкнуть цикл: a — ребёнок b
    transformA.setParent(b);

    // Отклонено: иерархия не изменилась
    BLIB_TEST_CHECK(transformA.getParent() == beng::invalidEntity);
    BLIB_TEST_CHECK(transformB.getParent() == a);
}

BLIB_TEST_CASE("transform: grandchild chain sums all local positions")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID root = scene.createEntity();
    beng::EntityID mid = scene.createEntity();
    beng::EntityID leaf = scene.createEntity();

    beng::TransformComponent& rootTransform =
        scene.addComponent<beng::TransformComponent>(root, &scene);
    beng::TransformComponent& midTransform =
        scene.addComponent<beng::TransformComponent>(mid, &scene);
    beng::TransformComponent& leafTransform =
        scene.addComponent<beng::TransformComponent>(leaf, &scene);

    rootTransform.setLocalPosition(makePos(1.0f, 0.0f, 0.0f));
    midTransform.setLocalPosition(makePos(2.0f, 0.0f, 0.0f));
    leafTransform.setLocalPosition(makePos(3.0f, 0.0f, 0.0f));

    midTransform.setParent(root);
    leafTransform.setParent(mid);

    auto world = leafTransform.getWorldPosition();
    BLIB_TEST_CHECK_CLOSE(world.x, 6.0f, 0.001f);
}

BLIB_TEST_CASE("transform: world scale multiplies through the hierarchy")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID parent = scene.createEntity();
    beng::EntityID child = scene.createEntity();

    beng::TransformComponent& parentTransform =
        scene.addComponent<beng::TransformComponent>(parent, &scene);
    beng::TransformComponent& childTransform =
        scene.addComponent<beng::TransformComponent>(child, &scene);

    parentTransform.setLocalScale(makePos(2.0f, 3.0f, 4.0f));
    childTransform.setLocalScale(makePos(5.0f, 6.0f, 7.0f));
    childTransform.setParent(parent);

    auto scale = childTransform.getWorldScale();
    BLIB_TEST_CHECK_CLOSE(scale.x, 10.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(scale.y, 18.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(scale.z, 28.0f, 0.001f);
}

BLIB_TEST_CASE("transform: world rotation inherits from parent")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID parent = scene.createEntity();
    beng::EntityID child = scene.createEntity();

    beng::TransformComponent& parentTransform =
        scene.addComponent<beng::TransformComponent>(parent, &scene);
    beng::TransformComponent& childTransform =
        scene.addComponent<beng::TransformComponent>(child, &scene);

    // Идентичный поворот родителя — мировой поворот ребёнка == identity
    childTransform.setParent(parent);
    auto rotation = childTransform.getWorldRotation();
    BLIB_TEST_CHECK_CLOSE(rotation.w, 1.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(rotation.x, 0.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(rotation.y, 0.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(rotation.z, 0.0f, 0.001f);

    // Поворот родителя на 90° вокруг Z — ребёнок наследует
    parentTransform.setLocalRotation(
        blib::math::Quaternion<float>(
            blib::math::AngleDegree<float>(90.0f),
            makePos(0.0f, 0.0f, 1.0f)));

    auto inherited = childTransform.getWorldRotation();
    BLIB_TEST_CHECK_CLOSE(inherited.w, 0.7071068f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(inherited.x, 0.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(inherited.y, 0.0f, 0.001f);
    BLIB_TEST_CHECK_CLOSE(inherited.z, 0.7071068f, 0.001f);
}

BLIB_TEST_CASE("transform: setParent to entity without transform is rejected")
{
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID plain = scene.createEntity(); // без Transform
    beng::EntityID child = scene.createEntity();
    beng::TransformComponent& childTransform =
        scene.addComponent<beng::TransformComponent>(child, &scene);

    childTransform.setParent(plain);

    BLIB_TEST_CHECK(childTransform.getParent() == beng::invalidEntity);
}
