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
        // ---------------------------------------------------------------
        // BlobShadow — мягкая blob-тень под объектом (NPR-пайплайн,
        // фаза 7): полупрозрачный тёмный диск на земле. Дёшево и
        // «сажает» персонажа/декор на землю в стиле Hades — вместо
        // честного shadow-mapping.
        //
        // Квад лежит в локальной плоскости XZ (нормаль +Y, нуль —
        // центр), «прижимается» к земле позицией (setPosition с
        // небольшим y-сдвигом над землёй, чтобы избежать z-fighting
        // с тайлами).
        //
        // Материал: ShadingMode::Unlit + градиентная текстура
        // (тёмный круг с мягким краем — альфа канала). При draw()
        // включается альфа-блендинг и отключается запись глубины
        // (тень не должна спорить с землёй/другими тенями), после
        // отрисовки состояние восстанавливается.
        //
        // Отрисовка — ПОСЛЕ непрозрачной земли, ДО персонажей:
        // одна плоскость на объект, сортировка не требуется
        // (тени не перекрываются между собой при разных y-сдвигах).
        // ---------------------------------------------------------------
        class __blib_graphics_api BlobShadow : public ITransformable, public IDrawable
        {
        private:
            // mutable: draw() (const, контракт IDrawable) синхронизирует
            // трансформ тени в меш — тот же паттерн, что Sphere
            mutable blib::graphics::Mesh mesh;

        public:
            BlobShadow();

            // Строит квад радиуса radius в плоскости XZ. image — тёмный
            // градиент: rgb = цвет тени, alpha = мягкость края
            void create(float radius, _In const blib::graphics::Image& image);

            const blib::graphics::Mesh& getMesh() const;

            // Release IDrawable api. Включает блендинг и запрещает
            // запись глубины на время отрисовки, затем восстанавливает
            virtual void draw(RenderContext& ctx) const override;
        };
    }
}
