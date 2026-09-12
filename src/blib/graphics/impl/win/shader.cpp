#include <blib/graphics/shader.h>

#include <fstream>
#include <vector>

#include <blib/inline.h>
#include <blib/core/console/console.h>
#include <blib/graphics/opengl.h>
#include <blib/system/memory/globalAllocator.h>

#include <Windows.h>

// ---------------------------------------------------------------
// Внутренние контексты шейдеров (память — GlobalAllocator + placement
// new, проектные правила запрещают new/delete; см. AGENTS.md)
// ---------------------------------------------------------------

struct oglShaderContext
{
    GLuint glShader = 0;
    GLenum shaderType = GL_NONE_SHADER;
    blib::graphics::RenderApi* pRenderApi = nullptr;
};

struct oglShaderProgramContext
{
    GLuint glProgram = 0;
    blib::graphics::RenderApi* pRenderApi = nullptr;

    // Прикреплённые шейдеры (не владеет: Shader живёт в вызывающем
    // объекте, например в Mesh). Нужны для reload()
    std::vector<blib::graphics::Shader*> shaders;

    // Кеш uniform-локаций: первый запрос — в GL, далее из кеша.
    // Фиксированная ёмкость + линейный поиск — hot path без аллокаций
    static constexpr size_t maxCachedUniforms = 16;
    struct CachedUniform
    {
        std::string name;
        GLint location = -1;
    };
    CachedUniform uniforms[maxCachedUniforms];
    size_t uniformCount = 0;
};

#define __blib_this_shader_context(_this) (static_cast<oglShaderContext*>(_this->ctx))
#define __blib_this_program_context(_this) (static_cast<oglShaderProgramContext*>(_this->ctx))

namespace
{
    // Размер буфера лога компиляции/линковки
    constexpr size_t maxShaderLogLength = 5000;

    // Максимальная длина пути к exe (GetModuleFileNameA)
    constexpr size_t maxExePathLength = 1024;

    // Глубина подъёма по родительским каталогам при поиске шейдеров
    // (dev-раскладка: exe в build\...\Debug, шейдеры в <корне>\src\shaders)
    constexpr buint32 maxPathWalkUpLevels = 8;

    // Реестр живых шейдерных программ: пополняется в create(),
    // чистка — в деструкторе. Потребитель — reloadAllShaderPrograms
    // (консольная команда "hotreload"); обращения только из главного
    // потока, как и вся работа с программами
    std::vector<blib::graphics::ShaderProgram*> livePrograms;

    // Есть ли шейдер в реестре живых программ
    bool isRegistered(blib::graphics::ShaderProgram* pProgram)
    {
        for (const blib::graphics::ShaderProgram* p : livePrograms)
        {
            if (p == pProgram)
            {
                return true;
            }
        }
        return false;
    }

    // Пробует открыть файл — проверка существования
    bool fileExists(const std::string& path)
    {
        std::ifstream fin(path, std::ios::in);
        return fin.is_open();
    }

    // Отрезает последний компонент пути (напр. "M:\Stuff\build\Debug\"
    // → "M:\Stuff\build\"). Ожидает завершающий разделитель; корень
    // ("C:\") вырождается в пустую строку
    void stripLastPathComponent(std::string& path)
    {
        if (path.empty())
        {
            return;
        }

        // Снимаем завершающий разделитель
        size_t pos = path.size() - 1;
        if (path[pos] == '\\' || path[pos] == '/')
        {
            path.erase(pos);
        }
        if (path.empty())
        {
            return;
        }

        pos = path.find_last_of("\\/");
        if (pos == std::string::npos)
        {
            path.clear();
            return;
        }

        // Оставляем всё до разделителя включительно
        path.erase(pos + 1);
    }

    // Резолвит путь к шейдеру: сначала как есть (рабочая директория),
    // затем относительно каталога исполняемого файла, затем подъёмом
    // по родительским каталогам от exe (candidate и src\candidate на
    // каждом уровне). Позволяет запускать приложения из любого cwd:
    // dev-запуск из студии (exe в build\...\Debug, шейдеры в
    // <корне репо>\src\shaders) и деплой (шейдеры рядом с exe)
    std::string resolveShaderPath(const std::string& shaderPath)
    {
        // 1. Рабочая директория
        if (fileExists(shaderPath))
        {
            return shaderPath;
        }

        char exeDirRaw[maxExePathLength] = { 0 };
        const DWORD length = GetModuleFileNameA(nullptr, exeDirRaw, static_cast<DWORD>(maxExePathLength));
        if (length == 0 || length >= maxExePathLength)
        {
            return shaderPath;
        }

        // Отрезаем имя exe, оставляя каталог с завершающим разделителем
        for (DWORD i = length; i > 0; --i)
        {
            const char c = exeDirRaw[i - 1];
            if (c == '\\' || c == '/')
            {
                exeDirRaw[i] = '\0';
                break;
            }
        }

        const std::string exeDir(exeDirRaw);

        // 2. Каталог exe (деплой-раскладка: шейдеры скопированы рядом)
        const std::string exeCandidate = exeDir + shaderPath;
        if (fileExists(exeCandidate))
        {
            return exeCandidate;
        }

        // 3. Подъём по родителям exe (dev-раскладка): на каждом уровне
        // пробуем <ancestor>\<path> и <ancestor>\src\<path>
        std::string ancestor = exeDir;
        for (buint32 level = 0; level < maxPathWalkUpLevels; ++level)
        {
            stripLastPathComponent(ancestor);
            if (ancestor.empty())
            {
                break;
            }

            const std::string plainCandidate = ancestor + shaderPath;
            if (fileExists(plainCandidate))
            {
                return plainCandidate;
            }

            const std::string srcCandidate = ancestor + "src\\" + shaderPath;
            if (fileExists(srcCandidate))
            {
                return srcCandidate;
            }
        }

        // Не найдено: возвращаем exe-кандидатуру — лог покажет,
        // где именно файл не нашёлся
        return exeCandidate;
    }

    // Читает файл, создаёт GL-шейдер и компилирует его.
    // При неудаче созданный объект удаляется (outShader остаётся 0).
    // Логирование — на совести вызывающего (здесь есть только текст
    // ошибки компиляции)
    blib::graphics::ShaderError compileShaderFromFile(
        blib::graphics::RenderApi* pApi,
        const std::string& path,
        GLenum shaderType,
        _Out GLuint& outShader,
        _Out std::string& errorText)
    {
        std::ifstream fin;
        fin.open(path, std::ios::in);
        if (__blib_unlikely(!fin.is_open()))
        {
            return blib::graphics::ShaderError::FileNotFound;
        }

        fin.seekg(0, std::ios::end);
        const std::streamsize size = fin.tellg();
        fin.seekg(0, std::ios::beg);

        std::string shaderText(static_cast<size_t>(size), '\0');
        fin.read(&shaderText[0], size);

        const char* pSource = shaderText.c_str();

        GLuint glShader = pApi->ogl.ext.__blib_gl_glCreateShader(shaderType);
        pApi->ogl.ext.__blib_gl_glShaderSource(glShader, 1, &pSource, nullptr);
        pApi->ogl.ext.__blib_gl_glCompileShader(glShader);

        GLint ok = GL_FALSE;
        pApi->ogl.ext.__blib_gl_glGetShaderiv(glShader, GL_COMPILE_STATUS, &ok);
        if (__blib_unlikely(!ok))
        {
            errorText.resize(maxShaderLogLength);
            pApi->ogl.ext.__blib_gl_glGetShaderInfoLog(glShader, static_cast<GLsizei>(errorText.size()), nullptr, &errorText[0]);
            pApi->ogl.ext.__blib_gl_glDeleteShader(glShader);
            return blib::graphics::ShaderError::CompilationFailed;
        }

        outShader = glShader;
        return blib::graphics::ShaderError::None;
    }
}

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
    : ctx(nullptr)
    , shaderPath()
    , type(Type::None)
{
    // Выделяющая форма new запрещена проектом: контекст аллоцируется
    // через GlobalAllocator + placement new (см. AGENTS.md)
    this->ctx = blib::memory::GlobalAllocator::instance().allocate(sizeof(oglShaderContext));
    new (this->ctx) oglShaderContext();
}

blib::graphics::Shader::Shader(const std::string& path)
    : ctx(nullptr)
    , shaderPath()
    , type(Type::None)
{
    this->ctx = blib::memory::GlobalAllocator::instance().allocate(sizeof(oglShaderContext));
    new (this->ctx) oglShaderContext();
    this->setPath(path);
}

blib::graphics::Shader::~Shader()
{
    // GL-шейдер удаляется, если компилировался и контекст ещё жив.
    // Порядок разрушения гарантирует вызывающий (модели/меши/лайны
    // выгружаются раньше окна — см. GRAPHICS.md «Владение GL»).
    // Шейдер, прикреплённый к программе, удаляется отложенно (GL) —
    // разрушение Mesh'а (шейдеры + программа) безопасно
    if (this->ctx)
    {
        oglShaderContext* shaderCtx = __blib_this_shader_context(this);
        if (shaderCtx->glShader != 0 && shaderCtx->pRenderApi &&
            shaderCtx->pRenderApi->ogl.ext.__blib_gl_glDeleteShader)
        {
            shaderCtx->pRenderApi->ogl.ext.__blib_gl_glDeleteShader(shaderCtx->glShader);
            shaderCtx->glShader = 0;
        }

        shaderCtx->~oglShaderContext();
        blib::memory::GlobalAllocator::instance().deallocate(this->ctx, sizeof(oglShaderContext));
        this->ctx = nullptr;
    }
}

blib::graphics::Shader::Shader(Shader&& other) noexcept
    : ctx(other.ctx)
    , shaderPath(std::move(other.shaderPath))
    , type(other.type)
{
    // Источник обнуляется: его деструктор не тронет переданный ctx
    other.ctx = nullptr;
}

void blib::graphics::Shader::setPath(const std::string& path)
{
    this->shaderPath = path;
}

const std::string& blib::graphics::Shader::getPath() const
{
    return this->shaderPath;
}

void blib::graphics::Shader::setType(const Type type)
{
    this->type = type;
}

void blib::graphics::Shader::setRenderApi(RenderApi* pRenderApi)
{
    __blib_this_shader_context(this)->pRenderApi = pRenderApi;
}

blib::graphics::ShaderError blib::graphics::Shader::compile()
{
    oglShaderContext* shaderCtx = __blib_this_shader_context(this);
    shaderCtx->shaderType = blibShaderTypeToOGL(this->type);

    if (__blib_unlikely(shaderCtx->shaderType == GL_NONE_SHADER))
    {
        __blib_return_error(blib::graphics::ShaderError::InvalidType, "invalid shader type");
    }
    if (__blib_unlikely(shaderCtx->pRenderApi == nullptr))
    {
        __blib_return_error(blib::graphics::ShaderError::InvalidType, "shader render api not set");
    }

    const std::string resolvedPath = resolveShaderPath(this->shaderPath);

    GLuint freshShader = 0;
    std::string errorText;
    blib::graphics::ShaderError errCode = compileShaderFromFile(
        shaderCtx->pRenderApi, resolvedPath, shaderCtx->shaderType, freshShader, errorText);
    if (__blib_unlikely(errCode != blib::graphics::ShaderError::None))
    {
        if (errCode == blib::graphics::ShaderError::FileNotFound)
        {
            __blib_return_error(errCode, "can not open shader file '%s'", resolvedPath.c_str());
        }
        __blib_return_error(errCode, "error on shader compilation: %s", errorText.c_str());
    }

    // Повторная компиляция после прикрепления к программе запрещена:
    // старая программа продолжила бы ссылаться на пересозданный объект
    // (GL может переиспользовать ID). Для перезагрузки — ShaderProgram::reload()
    if (shaderCtx->glShader != 0)
    {
        shaderCtx->pRenderApi->ogl.ext.__blib_gl_glDeleteShader(shaderCtx->glShader);
    }
    shaderCtx->glShader = freshShader;

    return blib::graphics::ShaderError::None;
}

blib::graphics::ShaderProgram::ShaderProgram()
    : ctx(nullptr)
{
    this->ctx = blib::memory::GlobalAllocator::instance().allocate(sizeof(oglShaderProgramContext));
    new (this->ctx) oglShaderProgramContext();
}

blib::graphics::ShaderProgram::~ShaderProgram()
{
    if (this->ctx)
    {
        oglShaderProgramContext* programCtx = __blib_this_program_context(this);

        // Снять из реестра живых программ (hotreload больше не трогает)
        for (size_t i = 0; i < livePrograms.size(); ++i)
        {
            if (livePrograms[i] == this)
            {
                livePrograms[i] = livePrograms.back();
                livePrograms.pop_back();
                break;
            }
        }

        if (programCtx->glProgram != 0 && programCtx->pRenderApi &&
            programCtx->pRenderApi->ogl.ext.__blib_gl_glDeleteProgram)
        {
            programCtx->pRenderApi->ogl.ext.__blib_gl_glDeleteProgram(programCtx->glProgram);
            programCtx->glProgram = 0;
        }

        programCtx->~oglShaderProgramContext();
        blib::memory::GlobalAllocator::instance().deallocate(this->ctx, sizeof(oglShaderProgramContext));
        this->ctx = nullptr;
    }
}

blib::graphics::ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : ctx(other.ctx)
{
    // Передача владения + перерегистрация в реестре hotreload:
    // живой адрес программы сменился (this вместо &other).
    // Источник обнуляется — его деструктор ничего не сделает
    for (size_t i = 0; i < livePrograms.size(); ++i)
    {
        if (livePrograms[i] == &other)
        {
            livePrograms[i] = this;
            break;
        }
    }

    other.ctx = nullptr;
}

void blib::graphics::ShaderProgram::AttachShader(Shader& shader)
{
    oglShaderProgramContext* programCtx = __blib_this_program_context(this);

    programCtx->shaders.push_back(&shader);
    // Прямой доступ к private ctx (ShaderProgram — friend Shader);
    // макро-форма `&shader->ctx` не подходит — MSVC разбирает её
    // как `(&shader)->ctx`
    programCtx->pRenderApi->ogl.ext.__blib_gl_glAttachShader(
        programCtx->glProgram, static_cast<oglShaderContext*>(shader.ctx)->glShader);
}

int blib::graphics::ShaderProgram::create()
{
    oglShaderProgramContext* programCtx = __blib_this_program_context(this);

    // Повторный create: старая программа отбрасывается (обычно create
    // вызывается один раз при bake — защита от утечек на граблях)
    if (programCtx->glProgram != 0 && programCtx->pRenderApi)
    {
        programCtx->pRenderApi->ogl.ext.__blib_gl_glDeleteProgram(programCtx->glProgram);
    }

    programCtx->glProgram = programCtx->pRenderApi->ogl.ext.__blib_gl_glCreateProgram();
    programCtx->uniformCount = 0;

    if (!isRegistered(this))
    {
        livePrograms.push_back(this);
    }

    return static_cast<int>(programCtx->glProgram);
}

blib::graphics::ShaderError blib::graphics::ShaderProgram::compile()
{
    oglShaderProgramContext* programCtx = __blib_this_program_context(this);

    programCtx->pRenderApi->ogl.ext.__blib_gl_glLinkProgram(programCtx->glProgram);

    GLint ok = GL_FALSE;
    programCtx->pRenderApi->ogl.ext.__blib_gl_glGetProgramiv(programCtx->glProgram, GL_LINK_STATUS, &ok);
    if (__blib_unlikely(!ok))
    {
        std::string errstr;
        errstr.resize(maxShaderLogLength);
        programCtx->pRenderApi->ogl.ext.__blib_gl_glGetProgramInfoLog(
            programCtx->glProgram, static_cast<GLsizei>(errstr.size()), nullptr, &errstr[0]);
        __blib_return_error(blib::graphics::ShaderError::LinkFailed, "error on shader program linking: %s", errstr.c_str());
    }

    return blib::graphics::ShaderError::None;
}

GLuint blib::graphics::ShaderProgram::getContext()
{
    return __blib_this_program_context(this)->glProgram;
}

void blib::graphics::ShaderProgram::setRenderApi(RenderApi* pRenderApi)
{
    __blib_this_program_context(this)->pRenderApi = pRenderApi;
}

void blib::graphics::ShaderProgram::use()
{
    __blib_this_program_context(this)->pRenderApi->ogl.ext.__blib_gl_glUseProgram(__blib_this_program_context(this)->glProgram);
}

void blib::graphics::ShaderProgram::unuse()
{
    __blib_this_program_context(this)->pRenderApi->ogl.ext.__blib_gl_glUseProgram(0);
}

blib::graphics::ShaderError blib::graphics::ShaderProgram::reload()
{
    oglShaderProgramContext* programCtx = __blib_this_program_context(this);
    blib::graphics::RenderApi* pApi = programCtx->pRenderApi;

    if (__blib_unlikely(pApi == nullptr))
    {
        __blib_return_error(blib::graphics::ShaderError::InvalidType, "hotreload: program render api not set");
    }
    if (programCtx->glProgram == 0 || programCtx->shaders.empty())
    {
        // Нечего перезагружать — не ошибка
        return blib::graphics::ShaderError::None;
    }

    // Временные объекты: старые шейдеры и программа не трогаются,
    // пока свежая сборка не готова (при неудаче рендер продолжает
    // работать на старой программе)
    std::vector<GLuint> freshShaders;
    freshShaders.reserve(programCtx->shaders.size());

    for (const blib::graphics::Shader* pShader : programCtx->shaders)
    {
        const oglShaderContext* shaderCtx = __blib_this_shader_context(pShader);
        const std::string resolvedPath = resolveShaderPath(pShader->shaderPath);

        GLuint freshShader = 0;
        std::string errorText;
        blib::graphics::ShaderError errCode = compileShaderFromFile(
            pApi, resolvedPath, shaderCtx->shaderType, freshShader, errorText);
        if (__blib_unlikely(errCode != blib::graphics::ShaderError::None))
        {
            for (GLuint shader : freshShaders)
            {
                pApi->ogl.ext.__blib_gl_glDeleteShader(shader);
            }

            if (errCode == blib::graphics::ShaderError::FileNotFound)
            {
                __blib_log_error("hotreload: can not open '%s'", resolvedPath.c_str());
            }
            else
            {
                __blib_log_error("hotreload: compilation failed for '%s': %s", resolvedPath.c_str(), errorText.c_str());
            }
            return errCode;
        }
        freshShaders.push_back(freshShader);
    }

    GLuint freshProgram = pApi->ogl.ext.__blib_gl_glCreateProgram();
    for (GLuint shader : freshShaders)
    {
        pApi->ogl.ext.__blib_gl_glAttachShader(freshProgram, shader);
    }
    pApi->ogl.ext.__blib_gl_glLinkProgram(freshProgram);

    GLint ok = GL_FALSE;
    pApi->ogl.ext.__blib_gl_glGetProgramiv(freshProgram, GL_LINK_STATUS, &ok);
    if (__blib_unlikely(!ok))
    {
        std::string errorText;
        errorText.resize(maxShaderLogLength);
        pApi->ogl.ext.__blib_gl_glGetProgramInfoLog(freshProgram, static_cast<GLsizei>(errorText.size()), nullptr, &errorText[0]);

        for (GLuint shader : freshShaders)
        {
            pApi->ogl.ext.__blib_gl_glDeleteShader(shader);
        }
        pApi->ogl.ext.__blib_gl_glDeleteProgram(freshProgram);

        __blib_log_error("hotreload: link failed: %s", errorText.c_str());
        return blib::graphics::ShaderError::LinkFailed;
    }

    // Успех: старые объекты удаляются (порядок важен — старую программу
    // удаляем первой, чтобы освободить старые шейдеры без ID-конфликтов),
    // на их место — свежие; кеш uniform-локаций сбрасывается
    pApi->ogl.ext.__blib_gl_glDeleteProgram(programCtx->glProgram);

    for (size_t i = 0; i < programCtx->shaders.size(); ++i)
    {
        oglShaderContext* shaderCtx = __blib_this_shader_context(programCtx->shaders[i]);
        if (shaderCtx->glShader != 0)
        {
            pApi->ogl.ext.__blib_gl_glDeleteShader(shaderCtx->glShader);
        }
        shaderCtx->glShader = freshShaders[i];
    }

    programCtx->glProgram = freshProgram;
    programCtx->uniformCount = 0;

    __blib_log_info("hotreload: %zu shader(s) recompiled, program relinked", programCtx->shaders.size());
    return blib::graphics::ShaderError::None;
}

GLint blib::graphics::ShaderProgram::getUniformLocation(_In const char* name)
{
    oglShaderProgramContext* programCtx = __blib_this_program_context(this);

    for (size_t i = 0; i < programCtx->uniformCount; ++i)
    {
        if (programCtx->uniforms[i].name == name)
        {
            return programCtx->uniforms[i].location;
        }
    }

    const GLint location = programCtx->pRenderApi->ogl.ext.__blib_gl_glGetUniformLocation(programCtx->glProgram, name);

    // Кеш ограничен ёмкостью: не влезло — честно ходим в GL каждый раз
    if (programCtx->uniformCount < oglShaderProgramContext::maxCachedUniforms)
    {
        programCtx->uniforms[programCtx->uniformCount].name = name;
        programCtx->uniforms[programCtx->uniformCount].location = location;
        ++programCtx->uniformCount;
    }

    return location;
}

void blib::graphics::reloadAllShaderPrograms()
{
    if (livePrograms.empty())
    {
        __blib_log_info("hotreload: no live shader programs");
        return;
    }

    buint32 okCount = 0;
    for (blib::graphics::ShaderProgram* pProgram : livePrograms)
    {
        if (pProgram->reload() == blib::graphics::ShaderError::None)
        {
            ++okCount;
        }
    }

    __blib_log_info("hotreload: done (%u/%zu programs reloaded)", static_cast<unsigned int>(okCount), livePrograms.size());
}

void blib::graphics::registerGraphicsConsoleCommands()
{
    // "hotreload" — основное имя; "reload_shaders" — читаемый алиас.
    // Вызывается приложением при инициализации (реестр Console
    // не thread-safe — строго из главного потока)
    blib::console::Console::instance().registerCommand(
        "hotreload",
        "recompiles all live shader programs from disk (style iteration)",
        [](const std::vector<std::string>&)
        {
            blib::graphics::reloadAllShaderPrograms();
        });

    blib::console::Console::instance().registerCommand(
        "reload_shaders",
        "alias of hotreload",
        [](const std::vector<std::string>&)
        {
            blib::graphics::reloadAllShaderPrograms();
        });
}

#undef __blib_this_program_context
#undef __blib_this_shader_context
