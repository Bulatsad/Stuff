#include <blib/graphics/rendercontext.h>

#include <blib/graphics/camera.h>

blib::graphics::RenderContext::RenderContext()
{
    this->vievMatrix.loadIdentity();
    this->projectionMatrix.loadIdentity();
}


void blib::graphics::RenderContext::applyTransform(const Transformable& transformable)
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

    const void* pViewMatrix = static_cast<const void*>(&(this->pCamera->getTransform().data));
    
    if (location == -1)
        throw std::exception("no location for view matrix uniform");
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, 1, GL_TRUE, reinterpret_cast<const GLfloat*>(pViewMatrix));
}

void blib::graphics::RenderContext::setCamera(const blib::graphics::Camera* a_pCamera)
{
    this->pCamera = a_pCamera;
}

void blib::graphics::RenderContext::sendProjectionMatrixToShaderProgram()
{
    GLint location = this->api.ogl.ext.__blib_gl_glGetUniformLocation(this->lastShader->getContext(), "gProjectionMatrix");

    const void* pProjectionMatrix = static_cast<const void*>(&(this->pCamera->getProjectionMatrix().data));

    if (location == -1)
        throw std::exception("no location for projection matrix uniform");
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, 1, GL_TRUE, reinterpret_cast<const GLfloat*>(pProjectionMatrix));
}

void blib::graphics::RenderContext::sendModelMatrixToShaderProgram(const blib::graphics::Transform& modelMatix)
{
    GLint location = this->api.ogl.ext.__blib_gl_glGetUniformLocation(this->lastShader->getContext(), "gModelMatrix");

    const void* pModelMatrix = static_cast<const void*>(&(modelMatix.data));

    if (location == -1)
        throw std::exception("no location for projection matrix uniform");
    this->api.ogl.ext.__blib_gl_glUniformMatrix4fv(location, 1, GL_TRUE, reinterpret_cast<const GLfloat*>(pModelMatrix));
}
