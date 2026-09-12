#include <blib/graphics/texture.h>

#include <blib/core/console/console.h>

#include <Windows.h>
#include <gl/GL.h>

#define __blib_this_context(_this) static_cast<win_gl_TexturePDCtx*>(&(_this->ctx))
#define __blib_get_gl_texture_id(_this) __blib_this_context(_this)->textureID

#define GL_CLAMP_TO_EDGE 0x812F

blib::graphics::Texture::Texture()
{
}

blib::graphics::Texture::~Texture()
{
    if (__blib_get_gl_texture_id(this))
    {
        // Текстура уничтожена без free() — GL-ресурс остался висеть в контексте
        __blib_log_warning("memory leak detected: texture destroyed without free()");
    }
}

blib::graphics::TextureError blib::graphics::Texture::create(const blib::graphics::Image& image, blib::graphics::RenderContext& ctx, genFlags flags)
{
    // Image всегда 4 канала (RGBA) — формат гарантирован типом Color,
    // поэтому код ошибки отсюда не ожидается
    return this->create(reinterpret_cast<const void*>(image.getData()), image.width, image.height, 4, ctx, flags);
}

blib::graphics::TextureError blib::graphics::Texture::create(const void* pdata, bint16 width, bint16 height, buint8 bytesPerPixel, blib::graphics::RenderContext& ctx, genFlags flags)
{
    this->width = width;
    this->height = height;

    // create texure id
    ctx.api.ogl.ext.__blib_gl_glGenTextures(1, &__blib_get_gl_texture_id(this));

    // bind texture
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, __blib_get_gl_texture_id(this));

    // populate texture
    switch (bytesPerPixel)
    {
    case 3:
        ctx.api.ogl.ext.__blib_gl_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, this->width, this->height, 0, GL_RGB, GL_UNSIGNED_BYTE, pdata);
        break;
    case 4:
        ctx.api.ogl.ext.__blib_gl_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pdata);
        break;
    default:
        __blib_return_error(blib::graphics::TextureError::UnsupportedFormat, "unsupported bytes per pixel: %u", static_cast<buint32>(bytesPerPixel));
    }

    // TODO : rewrite normaly
    if (static_cast<buint8>(flags) & static_cast<buint8>(genFlags::clamp_to_edge))
    {
        ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    // Unbind texture
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, 0);

    return blib::graphics::TextureError::None;
}

blib::graphics::TextureError blib::graphics::Texture::createDepth(bint16 width, bint16 height, blib::graphics::RenderContext& ctx)
{
    this->width = width;
    this->height = height;

    ctx.api.ogl.ext.__blib_gl_glGenTextures(1, &__blib_get_gl_texture_id(this));
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, __blib_get_gl_texture_id(this));

    // Depth-текстура: данные пишет рендер (FBO-attachment), сэмплинг —
    // в пост-процессинге (fog). Формат GL_DEPTH_COMPONENT24
    ctx.api.ogl.ext.__blib_gl_glTexImage2D(
        GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
        this->width, this->height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, 0);

    return blib::graphics::TextureError::None;
}

void blib::graphics::Texture::free(blib::graphics::RenderContext& ctx)
{
    ctx.api.ogl.ext.__blib_gl_glDeleteTextures(1, &__blib_get_gl_texture_id(this));
    __blib_get_gl_texture_id(this) = 0;
}

blib::graphics::TextureError blib::graphics::Texture::resize(bint16 aWidth, bint16 aHeight, blib::graphics::RenderContext& ctx)
{
    if (__blib_unlikely(aWidth <= 0 || aHeight <= 0))
    {
        __blib_return_error(blib::graphics::TextureError::UnsupportedFormat,
            "texture resize: invalid dimensions %dx%d", aWidth, aHeight);
    }

    this->width = aWidth;
    this->height = aHeight;

    // Перезаливка хранилища того же GL-объекта: идентификатор
    // текстуры не меняется, старые данные затираются новым
    // буфером (nullptr — неинициализированное содержимое)
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, __blib_get_gl_texture_id(this));
    ctx.api.ogl.ext.__blib_gl_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, 0);

    return blib::graphics::TextureError::None;
}

blib::graphics::TextureError blib::graphics::Texture::resizeDepth(bint16 aWidth, bint16 aHeight, blib::graphics::RenderContext& ctx)
{
    if (__blib_unlikely(aWidth <= 0 || aHeight <= 0))
    {
        __blib_return_error(blib::graphics::TextureError::UnsupportedFormat,
            "depth texture resize: invalid dimensions %dx%d", aWidth, aHeight);
    }

    this->width = aWidth;
    this->height = aHeight;

    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, __blib_get_gl_texture_id(this));
    ctx.api.ogl.ext.__blib_gl_glTexImage2D(
        GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
        this->width, this->height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, 0);

    return blib::graphics::TextureError::None;
}

TextureCtx blib::graphics::Texture::getContext() const
{
    return this->ctx;
}

void blib::graphics::Texture::update(const std::vector<blib::graphics::Color>&, unsigned int width, unsigned int height, unsigned int x, unsigned int y, blib::graphics::RenderContext& ctx)
{
    (ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D,  __blib_get_gl_texture_id(this)));
    //(glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels));
    (ctx.api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    
    ctx.api.ogl.ext.__blib_gl_glFlush();
}

blib::graphics::Texture blib::graphics::Texture::makeTextureAtlas(const std::vector<Texture>& textures)
{
    buint32 resWidth = 0;
    buint32 resHeight = 0;

    for (const blib::graphics::Texture& t : textures)
    {
        if (__blib_unlikely(resWidth + t.width >= buint16Max))
        {
            // Атлас не помещается в 16-битную текстуру — возвращаем
            // пустую текстуру (сигнатура возвращает значение, код
            // ошибки передать нельзя)
            __blib_log_error("texture atlas is to large: width overflow (%u + %d)", resWidth, t.width);
            return blib::graphics::Texture();
        }
        if (__blib_unlikely(resHeight + t.height >= buint16Max))
        {
            __blib_log_error("texture atlas is to large: height overflow (%u + %d)", resHeight, t.height);
            return blib::graphics::Texture();
        }

        resWidth += t.width;
        if (resHeight < t.height)
            resHeight = t.height;
    }

    blib::graphics::Image img(resHeight,resHeight);
    //img.update();

    // TODO : недописано — упаковка текстур в атлас (как и раньше)
    return blib::graphics::Texture();
}
