#include <blib/graphics/rendercontext.h>

#include <blib/graphics/iCamera.h>
#include <blib/graphics/shader.h>

#include <Windows.h>
#include <gl/GL.h>

namespace
{
    // Пиксель плоской белой текстуры-заглушки (RGBA8)
    constexpr buint8 flatWhiteComponent = 255;
    const GLubyte flatWhitePixel[4] = { flatWhiteComponent, flatWhiteComponent, flatWhiteComponent, flatWhiteComponent };

    // Дефолтные цвета света (мягкий Hades-подобный свет):
    // тёплое «солнце» + холодный приглушённый эмбиент
    constexpr float defaultLightDirectionX = 0.4f;
    constexpr float defaultLightDirectionY = -0.8f;
    constexpr float defaultLightDirectionZ = 0.3f;
    constexpr float defaultLightColorR = 1.0f;
    constexpr float defaultLightColorG = 0.95f;
    constexpr float defaultLightColorB = 0.85f;
    constexpr float defaultLightIntensity = 0.9f;

    constexpr float defaultAmbientColorR = 0.35f;
    constexpr float defaultAmbientColorG = 0.4f;
    constexpr float defaultAmbientColorB = 0.5f;
    constexpr float defaultAmbientIntensity = 0.5f;
}

blib::graphics::RenderContext::RenderContext()
    : directionalLight()
    , ambientLight()
{
    this->vievMatrix.loadIdentity();
    this->projectionMatrix.loadIdentity();

    // Дефолты света выставляются здесь, а не в инициализаторах полей —
    // Vector3f не имеет constexpr-конструктора в стиле проекта
    this->directionalLight.direction = blib::graphics::Vector3f(
        defaultLightDirectionX, defaultLightDirectionY, defaultLightDirectionZ);
    this->directionalLight.color = blib::graphics::Vector3f(
        defaultLightColorR, defaultLightColorG, defaultLightColorB);
    this->directionalLight.intensity = defaultLightIntensity;

    this->ambientLight.color = blib::graphics::Vector3f(
        defaultAmbientColorR, defaultAmbientColorG, defaultAmbientColorB);
    this->ambientLight.intensity = defaultAmbientIntensity;
}

GLuint blib::graphics::RenderContext::getFlatWhiteTexture() const
{
    // Ленивая инициализация при первом вызове. Текстура живёт до
    // конца процесса: GL-контекст в приложении один, освобождать
    // её отдельно не нужно
    static GLuint flatWhiteTextureId = 0;
    if (flatWhiteTextureId == 0)
    {
        this->api.ogl.ext.__blib_gl_glGenTextures(1, &flatWhiteTextureId);
        this->api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, flatWhiteTextureId);
        this->api.ogl.ext.__blib_gl_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, flatWhitePixel);
        this->api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        this->api.ogl.ext.__blib_gl_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        this->api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, 0);
    }

    return flatWhiteTextureId;
}


void blib::graphics::RenderContext::applyTransform(const Transform& transformable)
{
    //this->api.ogl.__blib_glRotatef(-transformable.getRotation().x, 1, 0, 0);
    //this->api.ogl.__blib_glRotatef(-transformable.getRotation().y, 0, 1, 0);
    //this->api.ogl.__blib_glRotatef(-transformable.getRotation().z, 0, 0, 1);
    //
    //this->api.ogl.__blib_glTranslatef(-(transformable.getPosition().x), -(transformable.getPosition().y), -(transformable.getPosition().z));
    //
    //this->api.ogl.__blib_glRotatef(transformable.getScale().x, transformable.getScale().y, transformable.getScale().z, 1);
}

void blib::graphics::RenderContext::setShaderProgram(blib::graphics::ShaderProgram* pShaderProgram)
{
    this->lastShader = pShaderProgram;
    this->lastShader->use();
}

void blib::graphics::RenderContext::sendVievMatrixToShaderProgram()
{
    // Локация идёт из кеша ShaderProgram (см. ShaderProgram::getUniformLocation)
    const GLint location = this->lastShader->getUniformLocation("gViewMatrix");
    if (location == -1)
    {
        return;
    }

    const void* pViewMatrix = static_cast<const void*>(&(this->pCamera->getViewMatrix().data));
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, 1, GL_TRUE, reinterpret_cast<const GLfloat*>(pViewMatrix));
}

void blib::graphics::RenderContext::setCamera(blib::graphics::ICamera* a_pCamera)
{
    this->pCamera = a_pCamera;
}

void blib::graphics::RenderContext::sendProjectionMatrixToShaderProgram()
{
    const GLint location = this->lastShader->getUniformLocation("gProjectionMatrix");
    if (location == -1)
    {
        return;
    }

    const void* pProjectionMatrix = static_cast<const void*>(&(this->pCamera->getProjectionMatrix().data));
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, 1, GL_TRUE, reinterpret_cast<const GLfloat*>(pProjectionMatrix));
}

void blib::graphics::RenderContext::sendModelMatrixToShaderProgram(const blib::graphics::TransformMatrix& modelMatix)
{
    const GLint location = this->lastShader->getUniformLocation("gModelMatrix");
    if (location == -1)
    {
        return;
    }

    const void* pModelMatrix = static_cast<const void*>(&(modelMatix.data));
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, 1, GL_TRUE, reinterpret_cast<const GLfloat*>(pModelMatrix));
}

void blib::graphics::RenderContext::sendBoneMatricesToShaderProgram(const std::vector<blib::graphics::TransformMatrix>& boneMatrices)
{
    if (boneMatrices.empty())
    {
        return;
    }

    const GLint location = this->lastShader->getUniformLocation("gBones");
    if (location == -1)
    {
        return;
    }

    size_t count = boneMatrices.size() > __blib_max_bones ? __blib_max_bones : boneMatrices.size();
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, static_cast<GLsizei>(count), GL_TRUE, reinterpret_cast<const GLfloat*>(boneMatrices.data()));
}

void blib::graphics::RenderContext::sendLightsToShaderProgram()
{
    // Направление света: нормализованный direction (из источника
    // к поверхности), цвет с учётом интенсивности
    const blib::graphics::Vector3f lightDir = blib::math::normalize(this->directionalLight.direction);

    GLint location = this->lastShader->getUniformLocation("gLightDir");
    if (location != -1)
    {
        this->api.ogl.ext.__blib_gl_glUniform3f(location, lightDir.x, lightDir.y, lightDir.z);
    }

    location = this->lastShader->getUniformLocation("gLightColor");
    if (location != -1)
    {
        const GLfloat lightColor[3] = {
            this->directionalLight.color.x * this->directionalLight.intensity,
            this->directionalLight.color.y * this->directionalLight.intensity,
            this->directionalLight.color.z * this->directionalLight.intensity
        };
        this->api.ogl.ext.__blib_gl_glUniform3fv(location, 1, lightColor);
    }

    location = this->lastShader->getUniformLocation("gAmbientColor");
    if (location != -1)
    {
        const GLfloat ambientColor[3] = {
            this->ambientLight.color.x * this->ambientLight.intensity,
            this->ambientLight.color.y * this->ambientLight.intensity,
            this->ambientLight.color.z * this->ambientLight.intensity
        };
        this->api.ogl.ext.__blib_gl_glUniform3fv(location, 1, ambientColor);
    }
}

void blib::graphics::RenderContext::sendCameraPositionToShaderProgram()
{
    if (this->pCamera == nullptr)
    {
        return;
    }

    const GLint location = this->lastShader->getUniformLocation("gCameraPosition");
    if (location == -1)
    {
        return;
    }

    const blib::graphics::Vector3f& cameraPosition = this->pCamera->getPosition();
    this->api.ogl.ext.__blib_gl_glUniform3f(location, cameraPosition.x, cameraPosition.y, cameraPosition.z);
}
