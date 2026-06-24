#pragma once

#include <vector>

#include <blib/config.h>
#include <blib/graphics/vertex.h>
#include <blib/graphics/drawable.h>
#include <blib/graphics/rendercontext.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api VertexBuffer : public blib::graphics::IDrawable
        {
            void* ctx;
        public:
            VertexBuffer();

            int create(RenderContext& ctx, const std::vector<blib::graphics::Vertex>& vertexBufferData);
        };
    }
}