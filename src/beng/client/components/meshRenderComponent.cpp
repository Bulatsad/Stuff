#include <beng/client/components/meshRenderComponent.h>

namespace beng
{
    MeshRenderComponent::MeshRenderComponent(blib::graphics::Mesh&& sourceMesh, RenderLayer renderLayer)
        : mesh(std::move(sourceMesh))
        , layer(renderLayer)
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

} // namespace beng
