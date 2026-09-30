#include <blib/test/src/test.h>

#include <beng/client/components/cameraComponent.h>
#include <beng/client/systems/cameraSystem.h>
#include <beng/client/componentCameraAdapter.h>
#include <beng/components/transform.h>
#include <beng/core/scene.h>

#include <blib/core/memoryStream.h>
#include <blib/core/math/vector.h>

#include <cmath>

namespace
{
    // Эпсилон сравнения чисел с плавающей точкой (матрицы камеры
    // считаются через тригонометрию — точное равенство не гарантируется)
    constexpr float floatEpsilon = 0.001f;

    bool nearEqual(float lhs, float rhs)
    {
        return std::fabs(lhs - rhs) < floatEpsilon;
    }
}

BLIB_TEST_CASE("camera component: save/load roundtrip")
{
    beng::Scene scene;
    scene.registerComponentType<beng::CameraComponent>();

    const beng::EntityID cameraEntity = scene.createEntity();
    beng::CameraComponent& camera = scene.addComponent<beng::CameraComponent>(cameraEntity);
    camera.setFovDegrees(45.0f);
    camera.setNearDistance(0.5f);
    camera.setFarDistance(500.0f);
    camera.setPixelWidth(640);
    camera.setPixelHeight(360);
    camera.setActive(true);

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(scene.save(mem) == blib::core::SaveStatus::None);

    beng::Scene loaded;
    loaded.registerComponentType<beng::CameraComponent>();
    mem.seek(0, blib::core::SeekOrigin::Begin);
    BLIB_TEST_REQUIRE(loaded.load(mem) == blib::core::LoadStatus::None);

    // Строгое сравнение и полный round-trip
    BLIB_TEST_CHECK(scene.strongCompare(loaded));
    BLIB_TEST_CHECK(scene.verify());

    // Поля камеры восстановлены
    const beng::CameraComponent& loadedCamera =
        loaded.getComponent<beng::CameraComponent>(cameraEntity);
    BLIB_TEST_CHECK(nearEqual(loadedCamera.getFovDegrees(), 45.0f));
    BLIB_TEST_CHECK(nearEqual(loadedCamera.getNearDistance(), 0.5f));
    BLIB_TEST_CHECK(nearEqual(loadedCamera.getFarDistance(), 500.0f));
    BLIB_TEST_CHECK(loadedCamera.getPixelWidth() == 640);
    BLIB_TEST_CHECK(loadedCamera.getPixelHeight() == 360);
    BLIB_TEST_CHECK(loadedCamera.getActive());
}

BLIB_TEST_CASE("camera component: load rejects invalid parameters")
{
    // Файл с корректной камерой, но вырожденные параметры подменяются
    // вручную — загрузчик обязан отклонить документ целиком
    beng::Scene scene;
    scene.registerComponentType<beng::CameraComponent>();
    const beng::EntityID cameraEntity = scene.createEntity();
    beng::CameraComponent& camera = scene.addComponent<beng::CameraComponent>(cameraEntity);
    camera.setPixelWidth(0);

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(scene.save(mem) == blib::core::SaveStatus::None);

    beng::Scene loaded;
    loaded.registerComponentType<beng::CameraComponent>();
    mem.seek(0, blib::core::SeekOrigin::Begin);
    BLIB_TEST_CHECK(loaded.load(mem) == blib::core::LoadStatus::InvalidData);
}

BLIB_TEST_CASE("camera system: single active camera invariant")
{
    beng::Scene scene;
    scene.registerComponentType<beng::CameraComponent>();
    beng::CameraSystem cameraSystem;
    scene.addSystem(&cameraSystem);

    // Две камеры, обе активны по умолчанию: вторая создана позже —
    // её штамп свежее
    const beng::EntityID cameraA = scene.createEntity();
    const beng::EntityID cameraB = scene.createEntity();
    scene.addComponent<beng::CameraComponent>(cameraA);
    scene.addComponent<beng::CameraComponent>(cameraB);

    // Нормализация на первом же кадре
    scene.update(0.016f);

    beng::CameraComponent& camA = scene.getComponent<beng::CameraComponent>(cameraA);
    beng::CameraComponent& camB = scene.getComponent<beng::CameraComponent>(cameraB);
    BLIB_TEST_CHECK(!camA.getActive());
    BLIB_TEST_CHECK(camB.getActive());

    // Включение A делает её единственной (последняя включённая)
    camA.setActive(true);
    scene.update(0.016f);
    BLIB_TEST_CHECK(camA.getActive());
    BLIB_TEST_CHECK(!camB.getActive());

    // Сцена без активных камер: система ничего не ломает
    camA.setActive(false);
    scene.update(0.016f);
    BLIB_TEST_CHECK(!camA.getActive());
    BLIB_TEST_CHECK(!camB.getActive());
}

BLIB_TEST_CASE("component camera adapter: view/projection from entity")
{
    beng::Scene scene;
    scene.registerComponentType<beng::CameraComponent>();

    const beng::EntityID cameraEntity = scene.createEntity();
    beng::CameraComponent& camera = scene.addComponent<beng::CameraComponent>(cameraEntity);
    camera.setFovDegrees(90.0f);
    camera.setNearDistance(0.1f);
    camera.setFarDistance(100.0f);

    // Identity-поворот: взгляд вдоль локальной +Z
    beng::TransformComponent& transform =
        scene.getComponent<beng::TransformComponent>(cameraEntity);
    transform.setLocalPosition(blib::math::Vector<float, 3>(0.0f, 10.0f, 0.0f));

    beng::ComponentCameraAdapter adapter;
    adapter.sync(camera, transform, 1.0f);

    // Позиция — позиция сущности
    BLIB_TEST_CHECK(nearEqual(adapter.getPosition().x, 0.0f));
    BLIB_TEST_CHECK(nearEqual(adapter.getPosition().y, 10.0f));
    BLIB_TEST_CHECK(nearEqual(adapter.getPosition().z, 0.0f));

    // Взгляд вдоль +Z (identity-поворот): камера смотрит по мировой
    // +Z — во view-пространстве GL это взгляд вдоль -Z (стандартный
    // lookAt): колонка 0 = (-1,0,0), колонка 1 = (0,1,0), колонка 2 =
    // (0,0,-1); трансляция = (-dot(s,eye), -dot(u,eye), dot(f,eye)) =
    // (0, -10, 0)
    const blib::graphics::TransformMatrix& view = adapter.getViewMatrix();
    BLIB_TEST_CHECK(nearEqual(view.data[0][0], -1.0f));
    BLIB_TEST_CHECK(nearEqual(view.data[1][1], 1.0f));
    BLIB_TEST_CHECK(nearEqual(view.data[2][2], -1.0f));
    BLIB_TEST_CHECK(nearEqual(view.data[3][1], -10.0f));

    // FOV 90° → f = 1/tan(45°) = 1; aspect 1 → data[1][1] = 1
    const blib::graphics::TransformMatrix& projection = adapter.getProjectionMatrix();
    BLIB_TEST_CHECK(nearEqual(projection.data[0][0], 1.0f));
    BLIB_TEST_CHECK(nearEqual(projection.data[1][1], 1.0f));
}
