#include <blib/graphics/vertexbuffer.h>

#include <Windows.h>
#include <gl/GL.h>


struct oglVertexBufferContext
{
    GLuint buferHander;
};

#define __blib_this_context(_this) this->ctx

#define __blib_this_vertexbuffer_hander(_this) ((static_cast<oglVertexBufferContext*>(__blib_this_context(_this)))->buferHander)


int blib::graphics::VertexBuffer::create(RenderContext& ctx, const std::vector<blib::graphics::Vertex>& vertexBufferData)
{
    ctx.api.ogl.ext.__blib_glGenBuffers(1, &__blib_this_vertexbuffer_hander(this));

    return 0;
}
