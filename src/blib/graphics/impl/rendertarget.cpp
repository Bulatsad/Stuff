#include <blib/graphics/rendertarget.h>

#include <Windows.h>
#include <gl/GL.h>

static __blib_inline buint8 selectNextFrameBufferIndex(buint8 currentIndex, buint8 frameBufferCount)
{
    ++currentIndex;
    if (currentIndex >= frameBufferCount)
        currentIndex = 0;
    return currentIndex;
}

blib::graphics::IRenderTarget::~IRenderTarget()
{
}

blib::graphics::IRenderTarget::IRenderTarget(buint32 a_viewportWidth, buint32 a_viewportHeight
    , buint8 a_frameBuffersCount)
{
    // Init Render Context
    this->rc.api.InitGraphicsApi();

    // Init Global enables
    this->rc.api.ogl.__blib_glEnable(GL_DEPTH_TEST);


    // Populate ctx

    this->ctx.frameBuffersCount = a_frameBuffersCount;
    this->ctx.currentFrameBufferIndex = 0;
    this->ctx.viewportWidth = a_viewportWidth;
    this->ctx.viewportHeight = a_viewportHeight;

    if (this->ctx.frameBuffersCount == 0)
    {
        throw std::runtime_error("Invalid frame buffers count");
    }

    this->ctx.pdctx.frameBufferIds.resize(this->ctx.frameBuffersCount);
    this->ctx.pdctx.renderBufferIds.resize(this->ctx.frameBuffersCount);
    this->ctx.frameTextures.resize(this->ctx.frameBuffersCount);


    // Create frame buffers
    this->rc.api.ogl.ext.__blib_gl_glGenFramebuffers(this->ctx.frameBuffersCount, this->ctx.pdctx.frameBufferIds.data());

    // Create and bind render frame buffer objects
    {
        // Create textures for fbo and bind it to frame buffers
        for(size_t i = 0; i < this->ctx.frameBuffersCount; ++i)
        {
            // Creating
            this->ctx.frameTextures[i].create(nullptr, a_viewportWidth, a_viewportHeight, blib::graphics::Color::bytesPerPixel(), this->rc);
            
            // Binding

            // Binding fbo
            this->rc.api.ogl.ext.__blib_gl_glBindFramebuffer(GL_FRAMEBUFFER, this->ctx.pdctx.frameBufferIds[i]);

            // Binding texure
            this->rc.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, this->ctx.frameTextures[i].getContext().textureID);
            this->rc.api.ogl.ext.__blib_gl_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->ctx.frameTextures[i].getContext().textureID, 0);
        }

        // Create render buffers and binding it to frame buffers
        this->rc.api.ogl.ext.__blib_gl_glGenRenderbuffers(this->ctx.frameBuffersCount, this->ctx.pdctx.renderBufferIds.data());
        for (size_t i = 0; i < this->ctx.frameBuffersCount; ++i)
        {
            // Creating
            this->rc.api.ogl.ext.__blib_gl_glBindRenderbuffer(GL_RENDERBUFFER, this->ctx.pdctx.renderBufferIds[i]);
            this->rc.api.ogl.ext.__blib_gl_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, a_viewportWidth, a_viewportHeight);

            // Binding
            
            // Binding fbo // MB usless??
            this->rc.api.ogl.ext.__blib_gl_glBindFramebuffer(GL_FRAMEBUFFER, this->ctx.pdctx.frameBufferIds[i]);

            // Binding render buffer to frame buffer
            this->rc.api.ogl.ext.__blib_gl_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, this->ctx.pdctx.frameBufferIds[i]);
        }
    }

    // Binding current frame buffer
    this->clear();
}

void blib::graphics::IRenderTarget::applySettings()
{
}

void blib::graphics::IRenderTarget::clear(const Color& color)
{
    this->ctx.currentFrameBufferIndex = selectNextFrameBufferIndex(this->ctx.currentFrameBufferIndex, this->ctx.frameBuffersCount);
    this->bindFrameBuffer(this->ctx.currentFrameBufferIndex);
    this->rc.api.ogl.__blib_glViewport(0, 0, this->ctx.viewportWidth, this->ctx.viewportHeight);
    this->rc.api.ogl.__blib_glEnable(GL_DEPTH_TEST);

    this->rc.api.ogl.__blib_gl_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //this->rc.api.ogl.glClearColor(
    //    (color.red) / static_cast<float>(255),
    //    color.green / static_cast<float>(255),
    //    color.blue / static_cast<float>(255),
    //    color.alpha / static_cast<float>(255)
    //);
    //this->rc.api.ogl.__blib_glPushMatrix();
}

void blib::graphics::IRenderTarget::draw(const blib::graphics::IDrawable& drawable)
{
    drawable.draw(rc);
}
