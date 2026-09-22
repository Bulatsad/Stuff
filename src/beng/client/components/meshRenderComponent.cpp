#include <beng/client/components/meshRenderComponent.h>

#include <blib/core/json/json.h>
#include <blib/core/verifyHelper.h>

namespace beng
{
    namespace
    {
        // Ключи JSON-объекта компонента (формат save/load); содержимое
        // меша сериализует blib::graphics::Mesh (см. Mesh::toJson)
        constexpr const char* keyLayer = "layer";
        constexpr const char* keyMesh = "mesh";
        constexpr const char* keyIsActive = "isActive";
    }

    MeshRenderComponent::MeshRenderComponent(blib::graphics::Mesh&& sourceMesh, RenderLayer renderLayer)
        : mesh(std::move(sourceMesh))
        , layer(renderLayer)
    {
    }

    MeshRenderComponent::MeshRenderComponent()
        : mesh()
        , layer(RenderLayer::Opaque)
    {
    }

    MeshRenderComponent::~MeshRenderComponent() = default;

    blib::graphics::Mesh& MeshRenderComponent::getMesh()
    {
        return this->mesh;
    }

    const blib::graphics::Mesh& MeshRenderComponent::getMesh() const
    {
        return this->mesh;
    }

    RenderLayer MeshRenderComponent::getLayer() const
    {
        return this->layer;
    }

    blib::core::SaveStatus MeshRenderComponent::save(_In blib::core::IOutputStream& os) const
    {
        blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

        doc.set(keyLayer, blib::core::json::JsonValue(static_cast<buint8>(layer)));
        doc.set(keyMesh, mesh.toJson());
        doc.set(keyIsActive, blib::core::json::JsonValue(isActive));

        if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::SaveStatus::WriteFailed,
                "MeshRenderComponent: failed to write JSON to stream");
        }
        return blib::core::SaveStatus::None;
    }

    blib::core::LoadStatus MeshRenderComponent::load(_In blib::core::IInputStream& is)
    {
        blib::core::json::JsonParser parser;
        blib::core::json::JsonValue doc;
        if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "MeshRenderComponent: failed to parse JSON from stream");
        }

        // Валидация полей компонента (меш валидирует Mesh::fromJson —
        // состояние меша при ошибке не меняется)
        if (__blib_unlikely(!doc.isObject()) ||
            __blib_unlikely(!doc.has(keyLayer) || !doc.get(keyLayer).isNumber()) ||
            __blib_unlikely(!doc.has(keyMesh)) ||
            __blib_unlikely(!doc.has(keyIsActive) || !doc.get(keyIsActive).isBool()))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "MeshRenderComponent: missing or malformed field");
        }

        const buint8 layerValue = static_cast<buint8>(doc.get(keyLayer).asBuint64());
        if (__blib_unlikely(layerValue > static_cast<buint8>(RenderLayer::Opaque)))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "MeshRenderComponent: unknown render layer %u",
                static_cast<unsigned int>(layerValue));
        }

        if (__blib_unlikely(mesh.fromJson(doc.get(keyMesh)) != blib::core::LoadStatus::None))
        {
            __blib_return_error(blib::core::LoadStatus::InvalidData,
                "MeshRenderComponent: malformed mesh data");
        }

        layer = static_cast<RenderLayer>(layerValue);
        isActive = doc.get(keyIsActive).asBool();

        return blib::core::LoadStatus::None;
    }

    bool MeshRenderComponent::strongCompare(_In const blib::core::IStrongComparable& other,
        _In blib::core::CompareSession& session) const
    {
        // Защита от циклов
        if (!session.enter(this, &other))
        {
            return true;
        }

        const MeshRenderComponent& o = static_cast<const MeshRenderComponent&>(other);

        // Базовые поля + слой + вся геометрия меша (делегирование Mesh)
        return getOwnerId() == o.getOwnerId() &&
            isActive == o.isActive &&
            layer == o.layer &&
            mesh.strongCompare(o.mesh, session);
    }

    bool MeshRenderComponent::verify() const
    {
        // Round-trip без RTTI (см. blib::core::verifyRoundTrip)
        return blib::core::verifyRoundTrip(*this);
    }

} // namespace beng
