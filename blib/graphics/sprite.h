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
        class __blib_api Sprite : public Transformable, public IDrawable
        {
        private:
            blib::graphics::Mesh mesh;
        public:
            Sprite();
            virtual ~Sprite();

            void setTexture(const Texture& texture);

            // Унаследовано через IDrawable
            virtual void draw(RenderTarget& target, RenderContext& ctx) const __blib_override;

        };
    }
}
