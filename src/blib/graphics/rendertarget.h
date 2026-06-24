#pragma once

#include <blib/config.h>
#include <blib/blibint.h>

#include <blib/graphics/drawable.h>
#include <blib/graphics/texture.h>
#include <blib/graphics/color.h>
#include <blib/graphics/rendercontext.h>

#include <blib/graphics/transformable.h>


#ifdef __blib_render_api_opengl
static struct win_gl_RenderTargetPlatfornmDependetCtx
{
    std::vector<GLuint> frameBufferIds;
    std::vector<GLuint> renderBufferIds;
    // TODO : Add render buffer
};
typedef win_gl_RenderTargetPlatfornmDependetCtx RenderTargetPlatfornmDependetCtx;
#endif // __blib_render_api_opengl

namespace blib
{
    namespace graphics
    {
        class __blib_api IDrawable;

        class __blib_api RenderTargetSettings
        {
        public:
            enum class Test
            {

            };
        };

        class __blib_api IRenderTarget
        {
        private:
            struct RenderTargetCtx
            {
                RenderTargetPlatfornmDependetCtx pdctx;
                std::vector<blib::graphics::Texture> frameTextures;
                buint8 frameBuffersCount;
                buint8 currentFrameBufferIndex;

                buint32 viewportWidth;
                buint32 viewportHeight;
            } ctx;
            //RenderTargetCtx ctx;

            void bindFrameBuffer(buint8 currentIndex)
            {
                this->rc.api.ogl.ext.__blib_gl_glBindFramebuffer(GL_FRAMEBUFFER, this->ctx.pdctx.frameBufferIds[currentIndex]);
            }

        public:
            virtual ~IRenderTarget();
            IRenderTarget() = delete;
            IRenderTarget(buint32 a_viewportWidth, buint32 a_viewportHeight, buint8 a_frameBuffersCount = 2);

            void applySettings();

            RenderContext rc;

            void clear(const Color& color = Color::Black);

            // ATTENTION!!!
            // Between clearing and drawing for one render target can not be
            // clearing or drawing to another render target. Cause render target
            // swithing global gl contexts. may be I fix it later, there need 
            // another render thread or render context manager. // TODO : 
            void draw(const blib::graphics::IDrawable& drawable);

            const RenderTargetCtx& getContext() const
            {
                return this->ctx;
            }
        };
    }
}