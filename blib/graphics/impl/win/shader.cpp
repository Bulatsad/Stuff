#include <blib/graphics/shader.h>

#include <iostream>
#include <fstream>

#include <blib/inline.h>
#include <blib/graphics/opengl.h>

static GLenum blibShaderTypeToOpenGLShaderType(const blib::graphics::Shader::Type& type)
{
    switch (type)
    {
        case blib::graphics::Shader::Type::fragment:
            return GL_FRAGMENT_SHADER;
        case blib::graphics::Shader::Type::vertex:
            return GL_VERTEX_SHADER;
    default:
        throw new std::runtime_error("");
    }
}

static struct oglShaderContext
{
    GLuint glShader;
    GLenum shaderType;
    blib::graphics::RenderApi* pRenderApi;
};

static struct oglShaderProgramContext
{
    GLuint glProgram;
    blib::graphics::RenderApi* pRenderApi;
};

#define __blib_this_context(_this) (static_cast<oglShaderContext*>(_this->ctx))

static __blib_inline GLenum blibShaderTypeToOGL(const blib::graphics::Shader::Type t)
{
    switch (t)
    {
        case blib::graphics::Shader::Type::fragment:
            return GL_FRAGMENT_SHADER;
        case blib::graphics::Shader::Type::vertex:
            return GL_VERTEX_SHADER;
    default:
        return GL_NONE_SHADER;
    }
}

blib::graphics::Shader::Shader()
{
    this->ctx = new oglShaderContext;
    memset(this->ctx, 0, sizeof(oglShaderContext));
}

blib::graphics::Shader::Shader(const std::string& path)
{
    this->setPath(path);
}

void blib::graphics::Shader::setPath(const std::string& path)
{
    this->shaderPath = path;
}

void blib::graphics::Shader::setType(const Type type)
{
    this->type = type;
}

void blib::graphics::Shader::setRenderApi(RenderApi* pRenderApi)
{
    __blib_this_context(this)->pRenderApi = pRenderApi;
}

int blib::graphics::Shader::compile()
{
    __blib_this_context(this)->shaderType = blibShaderTypeToOGL(this->type);

    if (__blib_this_context(this)->shaderType == GL_NONE_SHADER)
    {
        std::cerr << "Invalid shader type";
        throw new std::runtime_error("Invalid shader type");
        return EXIT_FAILURE;
    }

    std::ifstream fin;
    fin.open(this->shaderPath, std::ios::in);

    if (!fin.is_open())
    {
        std::cerr << "Can not open shader file";
        throw new std::runtime_error("Can not open shader file");
    }

    fin.seekg(0, std::ios::end);
    std::streamsize size = fin.tellg();
    fin.seekg(0, std::ios::beg);

    std::string shaderText(size, '\0');
    fin.read(&shaderText[0], size);
    //if (!fin.read(&shaderText[0], size)) {
    //    std::cerr << "Error at reading shader sources by path:" << this->shaderPath;
    //    throw std::runtime_error("Error at reading shader sources by path:: " + this->shaderPath);
    //}

    const char* pSource = shaderText.c_str();

    __blib_this_context(this)->glShader = __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glCreateShader(blibShaderTypeToOpenGLShaderType(this->type));
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glShaderSource(__blib_this_context(this)->glShader, 1, static_cast<const GLchar**>(&pSource), NULL);
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glCompileShader(__blib_this_context(this)->glShader);

    GLint ok = false;
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glGetShaderiv(__blib_this_context(this)->glShader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        std::string errstr;
        errstr.resize(5000);
        __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glGetShaderInfoLog(__blib_this_context(this)->glShader, errstr.size(), NULL, &(errstr[0]));
        std::cerr << "Error on shader compilation: " << errstr;
        throw std::runtime_error("Error on shader compilation: " + errstr);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#undef __blib_this_context

#define __blib_this_context(_this) (static_cast<oglShaderProgramContext*>(_this->ctx))

blib::graphics::ShaderProgram::ShaderProgram()
{
    this->ctx = new oglShaderProgramContext();
    memset(this->ctx, 0, sizeof(oglShaderProgramContext));
}

void blib::graphics::ShaderProgram::AttachShader(Shader& shader)
{
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glAttachShader(__blib_this_context(this)->glProgram, static_cast<oglShaderContext*>(shader.ctx)->glShader);
}

int blib::graphics::ShaderProgram::create()
{
    __blib_this_context(this)->glProgram = __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glCreateProgram();
    return __blib_this_context(this)->glProgram;
}

int blib::graphics::ShaderProgram::compile()
{
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glLinkProgram(__blib_this_context(this)->glProgram);

    GLint ok = false;
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glGetProgramiv(__blib_this_context(this)->glProgram, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        std::string errstr;
        errstr.resize(5000);
        __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glGetProgramInfoLog(__blib_this_context(this)->glProgram, errstr.size(), NULL, &(errstr[0]));
        std::cerr << "Error on shader compilation: " << errstr;
        throw std::runtime_error("Error on shader compilation: " + errstr);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

GLuint blib::graphics::ShaderProgram::getContext()
{
    return __blib_this_context(this)->glProgram;
}

void blib::graphics::ShaderProgram::setRenderApi(RenderApi* pRenderApi)
{
    __blib_this_context(this)->pRenderApi = pRenderApi;
}

void blib::graphics::ShaderProgram::use()
{
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glUseProgram(__blib_this_context(this)->glProgram);
}

void blib::graphics::ShaderProgram::unuse()
{
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glUseProgram(0);
}


#undef __blib_this_context
