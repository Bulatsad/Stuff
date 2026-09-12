#include <blib/graphics/postprocess.h>

#include <blib/core/console/console.h>
#include <blib/graphics/shaderPaths.h>
#include <blib/system/memory/globalAllocator.h>

#include <Windows.h>
#include <gl/GL.h>

// ---------------------------------------------------------------
// Внутренний контекст пост-процессинга: VAO полноэкранного
// треугольника + контекст рендера, создавший GL-ресурсы
// (паттерн oglMeshContext из mesh.cpp)
// ---------------------------------------------------------------
struct oglPostProcessContext
{
    GLuint vao = 0;
    blib::graphics::RenderContext* pRenderContext = nullptr;
};

#define __blib_this_context(_this) (static_cast<oglPostProcessContext*>(_this->ctx))

namespace
{
    // Дефолтные настройки пост-пасса: тёмная дымка вдаль (Hades-стиль),
    // НЕЙТРАЛЬНЫЙ grading и гамма 1.0. Гамма выключена намеренно:
    // конвейер пока не линейный (текстуры авторятся в sRGB), гамма
    // 2.2 дала бы двойное осветление. Полноценный sRGB-конвейер —
    // TODO (GL_SRGB8_ALPHA8-текстуры + линейная работа, см. GRAPHICS.md)
    constexpr float defaultFogColorR = 0.08f;
    constexpr float defaultFogColorG = 0.10f;
    constexpr float defaultFogColorB = 0.14f;
    constexpr float defaultFogStart = 280.0f;
    constexpr float defaultFogEnd = 700.0f;

    constexpr float defaultLift = 0.0f;
    constexpr float defaultGamma = 1.0f;
    constexpr float defaultGain = 1.0f;
    constexpr float defaultSaturation = 1.0f;
    constexpr float defaultVignette = 0.4f;

    constexpr float defaultNearPlane = 0.1f;
    constexpr float defaultFarPlane = 1000.0f;

    // Выходная гамма: 1.0 — без коррекции (см. комментарий выше)
    constexpr float defaultGammaOutput = 1.0f;
}

blib::graphics::PostProcess::PostProcess()
    : ctx(nullptr)
    , baked(false)
    , vertexShader()
    , fragmentShader()
    , program()
    , settings()
{
    // Выделяющая форма new запрещена проектом: контекст аллоцируется
    // через GlobalAllocator + placement new (см. AGENTS.md)
    this->ctx = blib::memory::GlobalAllocator::instance().allocate(sizeof(oglPostProcessContext));
    memset(this->ctx, 0, sizeof(oglPostProcessContext));

    this->settings.fogColor = blib::graphics::Vector3f(defaultFogColorR, defaultFogColorG, defaultFogColorB);
    this->settings.fogStart = defaultFogStart;
    this->settings.fogEnd = defaultFogEnd;
    this->settings.lift = blib::graphics::Vector3f(defaultLift, defaultLift, defaultLift);
    this->settings.gamma = blib::graphics::Vector3f(defaultGamma, defaultGamma, defaultGamma);
    this->settings.gain = blib::graphics::Vector3f(defaultGain, defaultGain, defaultGain);
    this->settings.saturation = defaultSaturation;
    this->settings.vignetteStrength = defaultVignette;
    this->settings.nearPlane = defaultNearPlane;
    this->settings.farPlane = defaultFarPlane;
    this->settings.gammaOutput = defaultGammaOutput;
}

blib::graphics::PostProcess::~PostProcess()
{
    // GL-ресурсы освобождаются, если создавались (bake) и контекст
    // рендера ещё жив — порядок разрушения гарантирует вызывающий
    // (пост-пасс живёт в impl клиента, умирает раньше окна/таргета)
    if (this->ctx)
    {
        oglPostProcessContext* postCtx = __blib_this_context(this);
        if (postCtx->vao != 0 && postCtx->pRenderContext &&
            postCtx->pRenderContext->api.ogl.ext.__blib_glDeleteVertexArrays)
        {
            postCtx->pRenderContext->api.ogl.ext.__blib_glDeleteVertexArrays(1, &postCtx->vao);
            postCtx->vao = 0;
        }

        blib::memory::GlobalAllocator::instance().deallocate(this->ctx, sizeof(oglPostProcessContext));
        this->ctx = nullptr;
    }
}

void blib::graphics::PostProcess::setSettings(_In const PostProcessSettings& newSettings)
{
    this->settings = newSettings;
}

const blib::graphics::PostProcessSettings& blib::graphics::PostProcess::getSettings() const
{
    return this->settings;
}

void blib::graphics::PostProcess::bake(blib::graphics::RenderContext& ctx) const
{
    // VAO без атрибутов: вершины полноэкранного треугольника
    // генерируются в шейдере из gl_VertexID
    ctx.api.ogl.ext.__blib_glGenVertexArrays(1, &(__blib_this_context(this)->vao));

    // Шейдеры (паттерн Mesh::bake: ошибки не фатальны — warning,
    // объект молча не рисуется)
    this->vertexShader.setPath(std::string(blib::graphics::postShaderBasePath) + blib::graphics::postVertexShaderName);
    this->vertexShader.setType(blib::graphics::Shader::Type::vertex);
    this->vertexShader.setRenderApi(&(ctx.api));
    {
        blib::graphics::ShaderError err = this->vertexShader.compile();
        if (__blib_unlikely(err != blib::graphics::ShaderError::None))
        {
            __blib_log_warning("postprocess bake: vertex shader compile failed (error %u)", static_cast<buint32>(err));
        }
    }

    this->fragmentShader.setPath(std::string(blib::graphics::postShaderBasePath) + blib::graphics::postFragmentShaderName);
    this->fragmentShader.setType(blib::graphics::Shader::Type::fragment);
    this->fragmentShader.setRenderApi(&(ctx.api));
    {
        blib::graphics::ShaderError err = this->fragmentShader.compile();
        if (__blib_unlikely(err != blib::graphics::ShaderError::None))
        {
            __blib_log_warning("postprocess bake: fragment shader compile failed (error %u)", static_cast<buint32>(err));
        }
    }

    this->program.setRenderApi(&(ctx.api));
    this->program.create();
    this->program.AttachShader(this->vertexShader);
    this->program.AttachShader(this->fragmentShader);
    {
        blib::graphics::ShaderError err = this->program.compile();
        if (__blib_unlikely(err != blib::graphics::ShaderError::None))
        {
            __blib_log_warning("postprocess bake: shader program link failed (error %u)", static_cast<buint32>(err));
        }
    }

    __blib_this_context(this)->pRenderContext = &ctx;
    this->baked = true;
}

void blib::graphics::PostProcess::apply(
    _In blib::graphics::RenderContext& ctx,
    _In const blib::graphics::Texture& colorTexture,
    _In const blib::graphics::Texture& depthTexture)
{
    if (!(this->baked))
    {
        this->bake(ctx);
    }

    ctx.setShaderProgram(&(this->program));

    // Входные текстуры сцены: цвет (unit 0), глубина (unit 1)
    ctx.api.ogl.ext.__blib_gl_glActiveTexture(GL_TEXTURE0);
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, colorTexture.getContext().textureID);
    GLint location = this->program.getUniformLocation("gSceneColor");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1i(location, 0);
    }

    ctx.api.ogl.ext.__blib_gl_glActiveTexture(GL_TEXTURE1);
    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, depthTexture.getContext().textureID);
    location = this->program.getUniformLocation("gSceneDepth");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1i(location, 1);
    }

    // Настройки пост-пасса
    location = this->program.getUniformLocation("gFogColor");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform3f(location, this->settings.fogColor.x, this->settings.fogColor.y, this->settings.fogColor.z);
    }

    location = this->program.getUniformLocation("gFogStart");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(location, this->settings.fogStart);
    }

    location = this->program.getUniformLocation("gFogEnd");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(location, this->settings.fogEnd);
    }

    location = this->program.getUniformLocation("gNearPlane");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(location, this->settings.nearPlane);
    }

    location = this->program.getUniformLocation("gFarPlane");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(location, this->settings.farPlane);
    }

    location = this->program.getUniformLocation("gLift");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform3f(location, this->settings.lift.x, this->settings.lift.y, this->settings.lift.z);
    }

    location = this->program.getUniformLocation("gGamma");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform3f(location, this->settings.gamma.x, this->settings.gamma.y, this->settings.gamma.z);
    }

    location = this->program.getUniformLocation("gGain");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform3f(location, this->settings.gain.x, this->settings.gain.y, this->settings.gain.z);
    }

    location = this->program.getUniformLocation("gSaturation");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(location, this->settings.saturation);
    }

    location = this->program.getUniformLocation("gVignetteStrength");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(location, this->settings.vignetteStrength);
    }

    location = this->program.getUniformLocation("gGammaOutput");
    if (location != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(location, this->settings.gammaOutput);
    }

    // Пост-пасс рисует в текущий framebuffer (обычно back-буфер):
    // depth-тест мешает — в back-буфере нет валидной глубины сцены
    ctx.api.ogl.__blib_glDisable(GL_DEPTH_TEST);

    ctx.api.ogl.ext.__blib_glBindVertexArray(__blib_this_context(this)->vao);
    ctx.api.ogl.ext.__blib_gl_glDrawArrays(GL_TRIANGLES, 0, 3);
    ctx.api.ogl.ext.__blib_glBindVertexArray(0);

    ctx.api.ogl.__blib_glEnable(GL_DEPTH_TEST);
}

#undef __blib_this_context
