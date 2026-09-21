#include <blib/test/src/test.h>

#include <beng/core/scene.h>
#include <beng/components/transform.h>

#include <blib/core/verifyHelper.h>
#include <blib/core/memoryStream.h>
#include <blib/core/json/json.h>
#include <blib/core/math/vector.h>

namespace
{
    // Компонент без save/load (Unsupported по умолчанию) — для проверки
    // отказа Scene::save на неподдерживаемом типе
    struct NonSerializableComponent : public beng::IComponent
    {
        NonSerializableComponent()
            : value(0)
        {
        }

        explicit NonSerializableComponent(bint32 v)
            : value(v)
        {
        }

        using blib::core::IStrongComparable::strongCompare;

    bool verify() const __blib_override
        {
            return blib::core::verifyRoundTrip(*this);
        }

        bool strongCompare(_In const blib::core::IStrongComparable& other, _In blib::core::CompareSession& session) const __blib_override
        {
            if (!session.enter(this, &other))
            {
                return true;
            }
            const NonSerializableComponent& o = static_cast<const NonSerializableComponent&>(other);
            return strongCompareBase(o) && value == o.value;
        }

        bint32 value;
    };

    // Сборка тестовой сцены с иерархией Transform:
    // parent <- child (позиция 1,2,3) + независимая сущность (scale 2)
    void buildTestScene(_Out beng::Scene& scene)
    {
        scene.registerComponentType<beng::TransformComponent>();

        beng::EntityID parent = scene.createEntity();
        scene.addComponent<beng::TransformComponent>(parent, &scene);

        beng::EntityID child = scene.createEntity();
        beng::TransformComponent& childTransform = scene.addComponent<beng::TransformComponent>(child, &scene);
        childTransform.setLocalPosition(blib::math::Vector<float, 3>(1.0f, 2.0f, 3.0f));
        childTransform.setParent(parent);

        beng::EntityID independent = scene.createEntity();
        beng::TransformComponent& indTransform = scene.addComponent<beng::TransformComponent>(independent, &scene);
        indTransform.setLocalScale(blib::math::Vector<float, 3>(2.0f, 2.0f, 2.0f));
    }
}

BLIB_TEST_CASE("scene save: json magic and valid document")
{
    beng::Scene scene;
    buildTestScene(scene);

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(scene.save(mem) == blib::core::SaveStatus::None);

    // Magic: "JSON" + NUL = 5 байт в начале файла
    mem.seek(0, blib::core::SeekOrigin::Begin);
    buint8 magic[beng::sceneSaveMagicSize];
    BLIB_TEST_REQUIRE(mem.read(magic, beng::sceneSaveMagicSize) == beng::sceneSaveMagicSize);
    BLIB_TEST_CHECK(magic[0] == 'J' && magic[1] == 'S' && magic[2] == 'O' && magic[3] == 'N');
    BLIB_TEST_CHECK(magic[4] == 0);

    // После magic — валидный JSON-документ
    blib::core::json::JsonParser parser;
    blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();
    BLIB_TEST_CHECK(parser.parse(mem, doc) == blib::core::json::JsonError::None);
    BLIB_TEST_CHECK(doc.isObject());
    BLIB_TEST_CHECK(doc.has(beng::sceneSaveFormatFieldName));
    BLIB_TEST_CHECK(doc.has(beng::sceneSaveVersionField));
}

BLIB_TEST_CASE("scene save/load roundtrip: verify passes and hierarchy restored")
{
    beng::Scene scene;
    buildTestScene(scene);

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(scene.save(mem) == blib::core::SaveStatus::None);

    beng::Scene loaded;
    loaded.registerComponentType<beng::TransformComponent>();
    mem.seek(0, blib::core::SeekOrigin::Begin);
    BLIB_TEST_REQUIRE(loaded.load(mem) == blib::core::LoadStatus::None);

    // Entity ID сохранены как были
    BLIB_TEST_CHECK(loaded.getEntityCount() == scene.getEntityCount());

    // Строгое сравнение загруженной сцены с оригиналом
    BLIB_TEST_CHECK(scene.strongCompare(loaded));

    // Полный round-trip валидируется verify()
    BLIB_TEST_CHECK(scene.verify());

    // Иерархия восстановлена: parent (1) <- child (2)
    const beng::TransformComponent& parentTransform =
        loaded.getComponent<beng::TransformComponent>(1);
    BLIB_TEST_CHECK(parentTransform.getChildren().size() == 1);
    BLIB_TEST_CHECK(parentTransform.getChildren()[0] == 2);
    BLIB_TEST_CHECK(loaded.getComponent<beng::TransformComponent>(2).getParent() == 1);
}

BLIB_TEST_CASE("scene load: unknown magic rejected")
{
    beng::Scene loaded;
    loaded.registerComponentType<beng::TransformComponent>();

    blib::core::MemoryStream mem;
    const char badMagic[] = { 'X', 'M', 'L', '\0', '\0' };
    mem.write(badMagic, beng::sceneSaveMagicSize);
    mem.seek(0, blib::core::SeekOrigin::Begin);

    BLIB_TEST_CHECK(loaded.load(mem) == blib::core::LoadStatus::UnknownFormat);
    BLIB_TEST_CHECK(loaded.getEntityCount() == 0);
}

BLIB_TEST_CASE("scene load: unregistered component type rejected")
{
    beng::Scene scene;
    buildTestScene(scene);

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(scene.save(mem) == blib::core::SaveStatus::None);

    // Свежая сцена БЕЗ регистрации типов: тип из файла неизвестен
    beng::Scene loaded;
    mem.seek(0, blib::core::SeekOrigin::Begin);
    BLIB_TEST_CHECK(loaded.load(mem) == blib::core::LoadStatus::ComponentTypeNotRegistered);
}

BLIB_TEST_CASE("scene load: non-empty scene rejected")
{
    beng::Scene scene;
    buildTestScene(scene);

    blib::core::MemoryStream mem;
    BLIB_TEST_REQUIRE(scene.save(mem) == blib::core::SaveStatus::None);

    beng::Scene loaded;
    loaded.registerComponentType<beng::TransformComponent>();
    loaded.createEntity(); // непустая сцена

    mem.seek(0, blib::core::SeekOrigin::Begin);
    BLIB_TEST_CHECK(loaded.load(mem) == blib::core::LoadStatus::SceneNotEmpty);
}

BLIB_TEST_CASE("scene save: non-serializable component rejected")
{
    beng::Scene scene;
    scene.registerComponentType<NonSerializableComponent>();

    beng::EntityID id = scene.createEntity();
    scene.addComponent<NonSerializableComponent>(id, 42);

    blib::core::MemoryStream mem;
    BLIB_TEST_CHECK(scene.save(mem) == blib::core::SaveStatus::ComponentNotSerializable);
}

BLIB_TEST_CASE("transform standalone verify: passes without scene binding")
{
    // Компонент вне сцены: ownerId = invalidEntity, ownerScene = nullptr —
    // standalone-копия воспроизводится бит-в-бит
    beng::TransformComponent transform;
    transform.setLocalPosition(blib::math::Vector<float, 3>(4.0f, 5.0f, 6.0f));

    BLIB_TEST_CHECK(transform.verify());
}

BLIB_TEST_CASE("transform verify in scene: expected false (strict model)")
{
    // Компонент, живущий в сцене: standalone-копия теряет контекст
    // (ownerScene и ownerId) — строгое сравнение честно отвечает false.
    // Валидация таких компонентов — через Scene::verify().
    beng::Scene scene;
    scene.registerComponentType<beng::TransformComponent>();

    beng::EntityID id = scene.createEntity();
    beng::TransformComponent& transform = scene.addComponent<beng::TransformComponent>(id, &scene);

    BLIB_TEST_CHECK(!transform.verify());
}
