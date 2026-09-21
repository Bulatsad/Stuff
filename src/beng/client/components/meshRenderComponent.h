#pragma once

#include <beng/config.h>
#include <beng/core/icomponent.h>

#include <blib/graphics/mesh.h>

namespace beng
{
    // ---------------------------------------------------------------
    // Слои отрисовки мира. RenderSystem рисует слои в фиксированном
    // порядке (инвариант — см. BENG.md):
    //
    //   Ground      — земля/тайлы (непрозрачное);
    //   Shadow      — полупрозрачные blob-тени: альфа-блендинг,
    //                 запись глубины выключена;
    //   AlphaTested — рисованные плоскости (discard в шейдере);
    //   Opaque      — непрозрачные lit-объекты и скелетные модели.
    // ---------------------------------------------------------------
    enum class RenderLayer : buint8
    {
        Ground = 0,
        Shadow = 1,
        AlphaTested = 2,
        Opaque = 3
    };

    /**
     * MeshRenderComponent — статический (неанимируемый) меш в мире.
     *
     * Назначение:
     * - Владеет blib::graphics::Mesh по значению (move-only: меш
     *   передаётся из билдера/примитива и живёт в компоненте до
     *   уничтожения сущности);
     * - Слой отрисовки определяет порядок и поведение (blend,
     *   alpha-test) — см. RenderLayer;
     * - Трансформация сущности — через TransformComponent (читает
     *   RenderSystem).
     *
     * Компонент некопируем (Mesh владеет GL-ресурсами); ComponentPool
     * конструирует компоненты placement-new и не перемещает их —
     * move-only член безопасен.
     *
     * Использование:
     *   scene.addComponent<MeshRenderComponent>(entity, std::move(mesh), RenderLayer::Ground);
     */
    class __beng_api MeshRenderComponent : public beng::IComponent
    {
    private:
        blib::graphics::Mesh mesh;
        RenderLayer layer;

    public:
        // Стабильное имя типа — идентичность типа в таблице типов Scene
        // (регистрация, резолв в шаблонных методах, будущий save/load)
        static constexpr const char* componentTypeName = "beng.MeshRender";

        MeshRenderComponent(blib::graphics::Mesh&& sourceMesh, RenderLayer renderLayer = RenderLayer::Opaque);
        ~MeshRenderComponent() override;

        MeshRenderComponent(const MeshRenderComponent&) = delete;
        MeshRenderComponent& operator=(const MeshRenderComponent&) = delete;

        blib::graphics::Mesh& getMesh();
        const blib::graphics::Mesh& getMesh() const;
        RenderLayer getLayer() const;
    };

} // namespace beng
