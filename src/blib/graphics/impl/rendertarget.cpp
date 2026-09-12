#include <blib/graphics/rendertarget.h>

#include <cstdlib>

#include <blib/core/console/console.h>

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
    // Освобождение GL-ресурсов FBO: цветовые текстуры, depth-текстуры
    // и сами framebuffers. Окно (и его GL-контекст) объявлено раньше
    // рендер-таргета и разрушается позже, поэтому контекст на момент
    // деструктора ещё текущий
    for (size_t i = 0; i < this->ctx.frameBuffersCount; ++i)
    {
        this->ctx.frameTextures[i].free(this->rc);
        this->ctx.frameDepthTextures[i].free(this->rc);
    }

    this->rc.api.ogl.ext.__blib_gl_glDeleteRenderbuffers(
        this->ctx.frameBuffersCount, this->ctx.pdctx.renderBufferIds.data());
    this->rc.api.ogl.ext.__blib_gl_glDeleteFramebuffers(
        this->ctx.frameBuffersCount, this->ctx.pdctx.frameBufferIds.data());
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

    if (__blib_unlikely(this->ctx.frameBuffersCount == 0))
    {
        // Конструктор не может вернуть код ошибки, а рендер-таргет
        // без буферов бессмыслен — это программистская (fatal) ошибка
        __blib_fatal("invalid frame buffers count: 0");
    }

    this->ctx.pdctx.frameBufferIds.resize(this->ctx.frameBuffersCount);
    this->ctx.pdctx.renderBufferIds.resize(this->ctx.frameBuffersCount);
    this->ctx.frameTextures.resize(this->ctx.frameBuffersCount);
    this->ctx.frameDepthTextures.resize(this->ctx.frameBuffersCount);


    // Create frame buffers
    this->rc.api.ogl.ext.__blib_gl_glGenFramebuffers(this->ctx.frameBuffersCount, this->ctx.pdctx.frameBufferIds.data());

    // Create and bind render frame buffer objects
    {
        // Create textures for fbo and bind it to frame buffers
        for(size_t i = 0; i < this->ctx.frameBuffersCount; ++i)
        {
            // Цветовая текстура: всегда 4 байта на пиксель — ошибка
            // формата тут невозможна, но проверяем по правилам проекта
            blib::graphics::TextureError terr = this->ctx.frameTextures[i].create(nullptr, a_viewportWidth, a_viewportHeight, blib::graphics::Color::bytesPerPixel(), this->rc);
            if (__blib_unlikely(terr != blib::graphics::TextureError::None))
            {
                __blib_log_warning("failed to create fbo texture #%zu (error %u)", i, static_cast<buint32>(terr));
            }

            // Depth-текстура: сэмплируется пост-процессингом (fog),
            // данные пишет рендер (GL_DEPTH_COMPONENT24)
            blib::graphics::TextureError depthErr = this->ctx.frameDepthTextures[i].createDepth(a_viewportWidth, a_viewportHeight, this->rc);
            if (__blib_unlikely(depthErr != blib::graphics::TextureError::None))
            {
                __blib_log_warning("failed to create fbo depth texture #%zu (error %u)", i, static_cast<buint32>(depthErr));
            }

            // Binding fbo
            this->rc.api.ogl.ext.__blib_gl_glBindFramebuffer(GL_FRAMEBUFFER, this->ctx.pdctx.frameBufferIds[i]);

            // Binding texure
            this->rc.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, this->ctx.frameTextures[i].getContext().textureID);
            this->rc.api.ogl.ext.__blib_gl_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->ctx.frameTextures[i].getContext().textureID, 0);

            // Binding depth texture
            this->rc.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, this->ctx.frameDepthTextures[i].getContext().textureID);
            this->rc.api.ogl.ext.__blib_gl_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, this->ctx.frameDepthTextures[i].getContext().textureID, 0);
        }
    }

    // Binding current frame buffer
    this->clear();
}

void blib::graphics::IRenderTarget::applySettings()
{
}

void blib::graphics::IRenderTarget::resize(buint32 a_viewportWidth, buint32 a_viewportHeight)
{
    // Защита от вырожденных размеров (схлопнутое окно/панель)
    // и от бесполезного повторного ресайза в тот же размер
    if (__blib_unlikely(a_viewportWidth < 1 || a_viewportHeight < 1))
    {
        return;
    }
    if (a_viewportWidth == this->ctx.viewportWidth && a_viewportHeight == this->ctx.viewportHeight)
    {
        return;
    }

    this->ctx.viewportWidth = a_viewportWidth;
    this->ctx.viewportHeight = a_viewportHeight;

    for (size_t i = 0; i < this->ctx.frameBuffersCount; ++i)
    {
        // Цветовая текстура: перезаливка хранилища того же объекта
        // (идентификатор не меняется — внешние биндинги валидны)
        blib::graphics::TextureError terr = this->ctx.frameTextures[i].resize(
            static_cast<bint16>(a_viewportWidth),
            static_cast<bint16>(a_viewportHeight),
            this->rc);
        if (__blib_unlikely(terr != blib::graphics::TextureError::None))
        {
            __blib_log_warning("fbo resize: failed to resize texture #%zu (error %u)", i, static_cast<buint32>(terr));
        }

        // Depth-текстура: переаллокация хранилища того же объекта
        // (привязки к FBO сохраняются)
        blib::graphics::TextureError depthErr = this->ctx.frameDepthTextures[i].resizeDepth(
            static_cast<bint16>(a_viewportWidth),
            static_cast<bint16>(a_viewportHeight),
            this->rc);
        if (__blib_unlikely(depthErr != blib::graphics::TextureError::None))
        {
            __blib_log_warning("fbo resize: failed to resize depth texture #%zu (error %u)", i, static_cast<buint32>(depthErr));
        }
    }

    // Viewport обновится в следующем clear()
}

void blib::graphics::IRenderTarget::clear(const Color& color)
{
    this->ctx.currentFrameBufferIndex = selectNextFrameBufferIndex(this->ctx.currentFrameBufferIndex, this->ctx.frameBuffersCount);
    this->bindFrameBuffer(this->ctx.currentFrameBufferIndex);
    this->rc.api.ogl.__blib_glViewport(0, 0, this->ctx.viewportWidth, this->ctx.viewportHeight);
    this->rc.api.ogl.__blib_glEnable(GL_DEPTH_TEST);

    // BUG-FIX: аргумент цвета игнорировался (glClearColor был
    // закомментирован) — фон всегда оставался чёрным дефолтом GL.
    // Теперь фон сцены задаётся цветом из clear()
    this->rc.api.ogl.__blib_glClearColor(
        static_cast<float>(color.red) / 255.0f,
        static_cast<float>(color.green) / 255.0f,
        static_cast<float>(color.blue) / 255.0f,
        static_cast<float>(color.alpha) / 255.0f);

    this->rc.api.ogl.__blib_gl_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void blib::graphics::IRenderTarget::draw(const blib::graphics::IDrawable& drawable)
{
    drawable.draw(rc);
}
