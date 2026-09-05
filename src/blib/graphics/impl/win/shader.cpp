#include <blib/graphics/shader.h>

#include <fstream>

#include <blib/inline.h>
#include <blib/core/console/console.h>
#include <blib/graphics/opengl.h>

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

blib::graphics::ShaderError blib::graphics::Shader::compile()
{
    __blib_this_context(this)->shaderType = blibShaderTypeToOGL(this->type);

    if (__blib_unlikely(__blib_this_context(this)->shaderType == GL_NONE_SHADER))
    {
        __blib_return_error(blib::graphics::ShaderError::InvalidType, "invalid shader type");
    }

    std::ifstream fin;
    fin.open(this->shaderPath, std::ios::in);

    if (__blib_unlikely(!fin.is_open()))
    {
        __blib_return_error(blib::graphics::ShaderError::FileNotFound, "can not open shader file '%s'", this->shaderPath.c_str());
    }

    fin.seekg(0, std::ios::end);
    std::streamsize size = fin.tellg();
    fin.seekg(0, std::ios::beg);

    std::string shaderText(size, '\0');
    fin.read(&shaderText[0], size);

    const char* pSource = shaderText.c_str();

    __blib_this_context(this)->glShader = __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glCreateShader(blibShaderTypeToOGL(this->type));
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glShaderSource(__blib_this_context(this)->glShader, 1, static_cast<const GLchar**>(&pSource), NULL);
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glCompileShader(__blib_this_context(this)->glShader);

    GLint ok = false;
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glGetShaderiv(__blib_this_context(this)->glShader, GL_COMPILE_STATUS, &ok);
    if (__blib_unlikely(!ok))
    {
        std::string errstr;
        errstr.resize(5000);
        __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glGetShaderInfoLog(__blib_this_context(this)->glShader, errstr.size(), NULL, &(errstr[0]));
        __blib_return_error(blib::graphics::ShaderError::CompilationFailed, "error on shader compilation: %s", errstr.c_str());
    }

    return blib::graphics::ShaderError::None;
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

blib::graphics::ShaderError blib::graphics::ShaderProgram::compile()
{
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glLinkProgram(__blib_this_context(this)->glProgram);

    GLint ok = false;
    __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glGetProgramiv(__blib_this_context(this)->glProgram, GL_LINK_STATUS, &ok);
    if (__blib_unlikely(!ok))
    {
        std::string errstr;
        errstr.resize(5000);
        __blib_this_context(this)->pRenderApi->ogl.ext.__blib_gl_glGetProgramInfoLog(__blib_this_context(this)->glProgram, errstr.size(), NULL, &(errstr[0]));
        __blib_return_error(blib::graphics::ShaderError::LinkFailed, "error on shader program linking: %s", errstr.c_str());
    }

    return blib::graphics::ShaderError::None;
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
