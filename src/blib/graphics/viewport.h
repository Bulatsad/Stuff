#pragma once

#include <blib/config.h>
#include <blib/blibint.h>

#include <blib/graphics/rendertarget.h>

#ifdef __blib_compile_platform_windows
#ifdef __blib_render_api_opengl
static struct win_gl_ViewportCtx
{
};
typedef win_gl_ViewportCtx ViewportCtx;
#endif // __blib_render_api_opengl
#endif // __blib_compile_platform_windows

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Viewport : public IRenderTarget
        {
        private:
            buint8 frameBuffersCount;
            ViewportCtx ctx;
        public:
            Viewport(buint32 width, buint32 height, buint8 a_frameBuffersCount = 2) : IRenderTarget(width, height, a_frameBuffersCount)
            {
                
            }
        };
    }
}