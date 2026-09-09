#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/components/transform.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/system/memory/globalAllocator.h>

int main()
{
    // Включить вывод Console в stdout (для консольного приложения)
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    __blib_log_info("=== ECS Test Application ===");

    // Создать сцену
    beng::Scene scene;

    // Зарегистрировать тип компонента Transform
    scene.registerComponentType<beng::TransformComponent>();

    // Добавить TransformSystem
    beng::TransformSystem transformSystem;
    scene.addSystem(&transformSystem);

    // Создать родительскую Entity
    beng::EntityID parent = scene.createEntity();
    beng::TransformComponent& parentTransform =
        scene.addComponent<beng::TransformComponent>(parent, &scene);
    parentTransform.setLocalPosition(blib::math::Vector<float, 3>(10.0f, 0.0f, 0.0f));

    __blib_log_info("Created parent Entity %llu with position (10, 0, 0)",
        static_cast<unsigned long long>(parent));

    // Создать дочернюю Entity
    beng::EntityID child = scene.createEntity();
    beng::TransformComponent& childTransform =
        scene.addComponent<beng::TransformComponent>(child, &scene);
    childTransform.setLocalPosition(blib::math::Vector<float, 3>(5.0f, 0.0f, 0.0f));
    childTransform.setParent(parent);

    __blib_log_info("Created child Entity %llu with local position (5, 0, 0), parent = %llu",
        static_cast<unsigned long long>(child),
        static_cast<unsigned long long>(parent));

    // Создать ещё одну независимую Entity
    beng::EntityID independent = scene.createEntity();
    beng::TransformComponent& independentTransform =
        scene.addComponent<beng::TransformComponent>(independent, &scene);
    independentTransform.setLocalPosition(blib::math::Vector<float, 3>(-5.0f, 10.0f, 0.0f));

    __blib_log_info("Created independent Entity %llu with position (-5, 10, 0)",
        static_cast<unsigned long long>(independent));

    // Обновить сцену (пересчёт мировых координат)
    __blib_log_info("Updating scene...");
    beng::Time time;
    time.tick();
    scene.update(time.getDeltaTime());

    // Вывести мировые координаты
    __blib_log_info("--- World Positions After Update ---");

    auto parentWorldPos = parentTransform.getWorldPosition();
    __blib_log_info("Parent Entity %llu world position: (%.2f, %.2f, %.2f)",
        static_cast<unsigned long long>(parent),
        parentWorldPos.x, parentWorldPos.y, parentWorldPos.z);

    auto childWorldPos = childTransform.getWorldPosition();
    __blib_log_info("Child Entity %llu world position: (%.2f, %.2f, %.2f)",
        static_cast<unsigned long long>(child),
        childWorldPos.x, childWorldPos.y, childWorldPos.z);
    __blib_log_info("  Expected: (15.00, 0.00, 0.00) [parent(10,0,0) + child_local(5,0,0)]");

    auto independentWorldPos = independentTransform.getWorldPosition();
    __blib_log_info("Independent Entity %llu world position: (%.2f, %.2f, %.2f)",
        static_cast<unsigned long long>(independent),
        independentWorldPos.x, independentWorldPos.y, independentWorldPos.z);

    // Тест resolveComponent
    __blib_log_info("--- Testing resolveComponent ---");
    beng::TransformComponent& resolvedTransform =
        scene.resolveComponent<beng::TransformComponent>(child, &scene);
    __blib_log_info("Resolved existing TransformComponent for child Entity %llu",
        static_cast<unsigned long long>(child));

    // Тест hasComponent
    __blib_log_info("--- Testing hasComponent ---");
    if (scene.hasComponent<beng::TransformComponent>(parent))
    {
        __blib_log_info("Parent Entity %llu has TransformComponent: YES",
            static_cast<unsigned long long>(parent));
    }

    // Статистика сцены
    __blib_log_info("--- Scene Statistics ---");
    __blib_log_info("Total entities: %u", static_cast<unsigned int>(scene.getEntityCount()));
    __blib_log_info("Total systems: %u", static_cast<unsigned int>(scene.getSystemCount()));

    __blib_log_info("=== Test Completed Successfully! ===");

    return 0;
}
