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
}

blib::graphics::RenderContext::RenderContext()
{
    this->vievMatrix.loadIdentity();
    this->projectionMatrix.loadIdentity();
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
    GLint location = this->api.ogl.ext.__blib_gl_glGetUniformLocation(this->lastShader->getContext(), "gViewMatrix");

    const void* pViewMatrix = static_cast<const void*>(&(this->pCamera->getViewMatrix().data));
    
    // Uniform нет в шейдере — пропускаем отправку (hot path,
    // как и sendBoneMatricesToShaderProgram ниже — без логов)
    if (location == -1)
        return;
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, 1, GL_TRUE, reinterpret_cast<const GLfloat*>(pViewMatrix));
}

void blib::graphics::RenderContext::setCamera(blib::graphics::ICamera* a_pCamera)
{
    this->pCamera = a_pCamera;
}

void blib::graphics::RenderContext::sendProjectionMatrixToShaderProgram()
{
    GLint location = this->api.ogl.ext.__blib_gl_glGetUniformLocation(this->lastShader->getContext(), "gProjectionMatrix");

    const void* pProjectionMatrix = static_cast<const void*>(&(this->pCamera->getProjectionMatrix().data));

    // Uniform нет в шейдере — пропускаем отправку
    if (location == -1)
        return;
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, 1, GL_TRUE, reinterpret_cast<const GLfloat*>(pProjectionMatrix));
}

void blib::graphics::RenderContext::sendModelMatrixToShaderProgram(const blib::graphics::TransformMatrix& modelMatix)
{
    GLint location = this->api.ogl.ext.__blib_gl_glGetUniformLocation(this->lastShader->getContext(), "gModelMatrix");

    const void* pModelMatrix = static_cast<const void*>(&(modelMatix.data));

    // Uniform нет в шейдере — пропускаем отправку
    if (location == -1)
        return;
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, 1, GL_TRUE, reinterpret_cast<const GLfloat*>(pModelMatrix));
}

void blib::graphics::RenderContext::sendBoneMatricesToShaderProgram(const std::vector<blib::graphics::TransformMatrix>& boneMatrices)
{
    if (boneMatrices.empty())
        return;

    GLint location = this->api.ogl.ext.__blib_gl_glGetUniformLocation(this->lastShader->getContext(), "gBones");

    if (location == -1)
        return;

    size_t count = boneMatrices.size() > __blib_max_bones ? __blib_max_bones : boneMatrices.size();
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, static_cast<GLsizei>(count), GL_TRUE, reinterpret_cast<const GLfloat*>(boneMatrices.data()));
}
