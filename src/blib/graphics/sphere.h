#pragma once

#include <blib/config.h>
#include <blib/utilmacro.h>

#include <blib/graphics/mesh.h>

#include <blib/graphics/rendertarget.h>
#include <blib/graphics/rendercontext.h>
#include <blib/graphics/transformable.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Sphere : public ITransformable, public IDrawable
        {
        private:
            // mutable: draw() (const, контракт IDrawable) синхронизирует
            // трансформ сферы в трансформ меша перед отрисовкой —
            // тот же паттерн, что SkinModel::draw
            mutable blib::graphics::Mesh sphereMesh;
        public:

            // Procedural generation mesh for spehere
            void createSpere(float radius = 1.f, buint32 pointPerCircle = 4, blib::graphics::Color color = blib::graphics::Color::White);

            // Доступ к сгенерированному мешу по ссылке (Mesh некопируем
            // по смыслу — владеет GL-ресурсами, см. GRAPHICS.md)
            const blib::graphics::Mesh& getMesh() const;

            // Передать владение мешем наружу (NPR/ECS: меш уходит в
            // рендер-компонент, см. MeshRenderComponent). После вызова
            // объект пуст и не пригоден для отрисовки
            blib::graphics::Mesh takeMesh();

            // Release IDrawable api
            virtual void draw(RenderContext& ctx) const override;
        };
    }
}
