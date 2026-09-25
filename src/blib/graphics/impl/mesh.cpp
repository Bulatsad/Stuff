#include <blib/graphics/mesh.h>

#include <blib/core/console/console.h>
#include <blib/core/verifyHelper.h>
#include <blib/graphics/shaderPaths.h>

#include <Windows.h>
#include <gl/GL.h>

enum class MeshAttributeNames : GLuint
{
    position = 0,
    color = 1,
    textureCoords = 1,
    boneIds = 2,
    boneWeights = 3,
    indices = 4,
    normals = 5,
    vbCount = 6
};

struct oglMeshContext
{
    GLuint vao;
    GLuint vbos[(GLuint)MeshAttributeNames::vbCount];
    GLuint faces; //ebo
    // Контекст рендера, в котором созданы GL-ресурсы (запоминается в
    // bake). Нужен деструктору для их освобождения — по аналогии с
    // Material::pRenderContext
    blib::graphics::RenderContext* renderContext = nullptr;
};

#define __blib_this_context(_this) (static_cast<oglMeshContext*>(_this->ctx))

__blib_private_func inline GLenum primitiveTypeToOGL(const blib::graphics::PrimitiveType& pt)
{
    switch (pt)
    {
    case blib::graphics::PrimitiveType::Triangle:
        return GL_TRIANGLES;
    case blib::graphics::PrimitiveType::TriangleStrip:
        return GL_TRIANGLE_STRIP;
    default:
        // Неизвестный тип примитива: 0 — невалидный GL-режим,
        // GL просто ничего не отрисует (это hot path — без логов)
        return 0;
    }
}

__blib_private_func inline void loadVertexFromAssimp(blib::graphics::Mesh& bengmesh, const aiMesh* paimesh)
{
    bengmesh.vertices.resize(paimesh->mNumVertices);
    for (size_t i = 0; i < bengmesh.vertices.size(); ++i)
    {
        blib::graphics::Vertex& bengvertex = bengmesh.vertices[i];
        aiVector3D& aivertex = paimesh->mVertices[i];
        bengvertex.x = aivertex.x;
        bengvertex.y = aivertex.y;
        bengvertex.z = aivertex.z;
    }
}

__blib_private_func inline void loadNormalsFromAssimp(blib::graphics::Mesh& bengmesh, const aiMesh* paimesh)
{

}

__blib_private_func inline void loadTextureCoordinatesFromAssimp(blib::graphics::Mesh& bengmesh, const aiMesh* paimesh)
{
    if (paimesh->HasTextureCoords(0))
    {
        bengmesh.textureCoords.resize(paimesh->mNumVertices);

        for (size_t i = 0; i < bengmesh.textureCoords.size(); ++i)
        {
            blib::graphics::Vertex& bengtexturecoord = bengmesh.textureCoords[i];
            aiVector3D& aitexturecoord = paimesh->mTextureCoords[0][i];
            bengtexturecoord.x = aitexturecoord.x;
            bengtexturecoord.y = aitexturecoord.y;
            bengtexturecoord.z = aitexturecoord.z;
        }
    }
}

void blib::graphics::Mesh::loadFromAssimpMesh(const aiMesh* paimesh)
{
    auto tmptype = paimesh->mPrimitiveTypes;
    if (tmptype & aiPrimitiveType_NGONEncodingFlag)
    {
        tmptype &= ~aiPrimitiveType_NGONEncodingFlag;
        this->ngonencoding = true;
    }

    switch (tmptype)
    {
    case aiPrimitiveType::aiPrimitiveType_POINT:
        this->primitiveType = PrimitiveType::Point;
        break;
    case aiPrimitiveType::aiPrimitiveType_LINE:
        this->primitiveType = PrimitiveType::Line;
        break;
    case aiPrimitiveType::aiPrimitiveType_TRIANGLE:
        this->primitiveType = PrimitiveType::Triangle;
        break;
    case aiPrimitiveType::aiPrimitiveType_POLYGON:
        this->primitiveType = PrimitiveType::Polygon;
        break;

    default:
        __blib_log_error("unknown assimp primitive type: 0x%X", tmptype);
        return;
    }

    loadVertexFromAssimp(*this, paimesh);
    loadTextureCoordinatesFromAssimp(*this, paimesh);

    // load faces
    this->faces.resize(paimesh->mNumFaces);
    for (size_t i = 0; i < this->faces.size(); ++i)
    {
        auto& bengface = this->faces[i];
        const auto& aiface = paimesh->mFaces[i];

        bengface.loadFromAssimpFace(&aiface);
    }

    if (this->primitiveType != PrimitiveType::Point && this->primitiveType != PrimitiveType::Line && paimesh->mNormals)
    {
        this->normals.resize(paimesh->mNumVertices);
        for (size_t i = 0; i < this->normals.size(); ++i)
        {
            auto& bengnormal = this->normals[i];
            const auto& ainormal = paimesh->mNormals[i];

            bengnormal.x = ainormal.x;
            bengnormal.y = ainormal.y;
            bengnormal.z = ainormal.z;
        }
    }

}

void blib::graphics::Mesh::bake(blib::graphics::RenderContext& ctx) const
{
    // Ленивая аллокация GL-контекста: конструктор не аллоцирует
    // (контракт IAllocatorAware — кеш ресурсов ставит аллокатор после
    // конструирования). Память — через аллокатор интерфейса.
    // allocate() не-const (Allocator мутирует SBO/impl), а bake() —
    // const-метод, только кеширующий GL-состояние (как mutable baked):
    // const_cast — локализованное снятие const для ленивой аллокации
    if (this->ctx == nullptr)
    {
        this->ctx = const_cast<blib::graphics::Mesh*>(this)->allocate(sizeof(oglMeshContext));
        if (__blib_unlikely(this->ctx == nullptr))
        {
            __blib_log_error("mesh bake: failed to allocate GL context");
            return;
        }
        memset(this->ctx, 0, sizeof(oglMeshContext));
    }

    // create VAO
    ctx.api.ogl.ext.__blib_glGenVertexArrays(1, &(__blib_this_context(this)->vao));
    ctx.api.ogl.ext.__blib_glBindVertexArray(__blib_this_context(this)->vao);

    // create vbo's
    ctx.api.ogl.ext.__blib_glGenBuffers((GLuint)MeshAttributeNames::vbCount, (__blib_this_context(this)->vbos));
    // create ebo (faces)
    ctx.api.ogl.ext.__blib_glGenBuffers(1, &(__blib_this_context(this)->faces));


    // populate position vbo and enable attribute
    ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::position]);
    ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(this->vertices[0]) * this->vertices.size(), this->vertices.data(), GL_STATIC_DRAW);
    ctx.api.ogl.ext.__blib_glEnableVertexAttribArray((GLuint)MeshAttributeNames::position);
    ctx.api.ogl.ext.__blib_glVertexAttribPointer((GLuint)MeshAttributeNames::position, 3, GL_FLOAT, GL_FALSE, 0, 0);

    ////// populate position vbo and enable attribute color
    //{
    //    if (this->colors.size() != this->vertices.size())
    //    {
    //        throw std::runtime_error("Colors size must be equal to vertices size");
    //    }
    //    auto tmpFloatColors = blib::graphics::makeFloatData(this->colors);
    //    ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::color]);
    //    ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(tmpFloatColors[0]) * tmpFloatColors.size(), tmpFloatColors.data(), GL_STATIC_DRAW);
    //    ctx.api.ogl.ext.__blib_glEnableVertexAttribArray((GLuint)MeshAttributeNames::color);
    //    ctx.api.ogl.ext.__blib_glVertexAttribPointer((GLuint)MeshAttributeNames::color, 4, GL_FLOAT, GL_FALSE, 0, 0);
    //}

    // populate texturecoords vbo and enable attribute
    ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::textureCoords]);
    ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(this->textureCoords[0]) * this->textureCoords.size(), this->textureCoords.data(), GL_STATIC_DRAW);
    ctx.api.ogl.ext.__blib_glEnableVertexAttribArray((GLuint)MeshAttributeNames::textureCoords);
    ctx.api.ogl.ext.__blib_glVertexAttribPointer((GLuint)MeshAttributeNames::textureCoords, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // populate bone ids and weights vbo and enable attributes
    if (this->boneIds.size() == this->vertices.size() && this->boneWeights.size() == this->vertices.size() && !(this->boneIds.empty()))
    {
        ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::boneIds]);
        ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(this->boneIds[0]) * this->boneIds.size(), this->boneIds.data(), GL_STATIC_DRAW);
        ctx.api.ogl.ext.__blib_glEnableVertexAttribArray((GLuint)MeshAttributeNames::boneIds);
        ctx.api.ogl.ext.__blib_glVertexAttribIPointer((GLuint)MeshAttributeNames::boneIds, 4, GL_INT, 0, 0);

        ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::boneWeights]);
        ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(this->boneWeights[0]) * this->boneWeights.size(), this->boneWeights.data(), GL_STATIC_DRAW);
        ctx.api.ogl.ext.__blib_glEnableVertexAttribArray((GLuint)MeshAttributeNames::boneWeights);
        ctx.api.ogl.ext.__blib_glVertexAttribPointer((GLuint)MeshAttributeNames::boneWeights, 4, GL_FLOAT, GL_FALSE, 0, 0);
    }

    // populate normals vbo and enable attribute (нужны свету/контурам
    // NPR-пайплайна; ранее грузились только в CPU — см. GRAPHICS.md)
    if (this->normals.size() == this->vertices.size())
    {
        ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::normals]);
        ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(this->normals[0]) * this->normals.size(), this->normals.data(), GL_STATIC_DRAW);
        ctx.api.ogl.ext.__blib_glEnableVertexAttribArray((GLuint)MeshAttributeNames::normals);
        ctx.api.ogl.ext.__blib_glVertexAttribPointer((GLuint)MeshAttributeNames::normals, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    // TODO : Triangulate and rewrite renderer
    ctx.api.ogl.ext.__blib_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, __blib_this_context(this)->faces);
        auto tmp = blib::graphics::compileFaces(this->faces);
    ctx.api.ogl.ext.__blib_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(this->faces[0].indices[0]) * this->faces[0].indices.size() * this->faces.size(), &(tmp[0]), GL_STATIC_DRAW);


    // Выбор вершинного шейдера: скиннинговая ветка включается,
    // если у меша есть веса костей (см. MeshAttributeNames::boneIds)
    const std::string vertexShaderPath = std::string(blib::graphics::meshShaderBasePath) +
        (this->boneIds.empty() ? blib::graphics::meshVertexShaderName : blib::graphics::skinMeshVertexShaderName);
    this->vertexShader.setPath(vertexShaderPath);
    this->vertexShader.setType(blib::graphics::Shader::Type::vertex);
    this->vertexShader.setRenderApi(&(ctx.api));
    // Ошибка компиляции шейдера не фатальна для процесса: логируем
    // предупреждение и продолжаем — меш "запечётся" с битым шейдером
    // и просто не отрисуется (baked выставится, повторных попыток не будет)
    {
        blib::graphics::ShaderError err = this->vertexShader.compile();
        if (__blib_unlikely(err != blib::graphics::ShaderError::None))
        {
            __blib_log_warning("mesh bake: vertex shader compile failed (error %u)", static_cast<buint32>(err));
        }
    }

    this->fragmentShader.setPath(std::string(blib::graphics::meshShaderBasePath) + blib::graphics::meshFragmentShaderName);
    this->fragmentShader.setType(blib::graphics::Shader::Type::fragment);
    this->fragmentShader.setRenderApi(&(ctx.api));
    {
        blib::graphics::ShaderError err = this->fragmentShader.compile();
        if (__blib_unlikely(err != blib::graphics::ShaderError::None))
        {
            __blib_log_warning("mesh bake: fragment shader compile failed (error %u)", static_cast<buint32>(err));
        }
    }

    this->drawer.setRenderApi(&(ctx.api));
    this->drawer.create();
    this->drawer.AttachShader(this->vertexShader);
    this->drawer.AttachShader(this->fragmentShader);
    {
        blib::graphics::ShaderError err = this->drawer.compile();
        if (__blib_unlikely(err != blib::graphics::ShaderError::None))
        {
            __blib_log_warning("mesh bake: shader program link failed (error %u)", static_cast<buint32>(err));
        }
    }

    // Контурная программа (inverted hull, фаза 8): компилируется всегда —
    // включение контура решает материал в draw() (outlineEnabled).
    // Выбор вершинного шейдера — как у основной программы (скиннинг)
    const std::string outlineVertexShaderPath = std::string(blib::graphics::meshShaderBasePath) +
        (this->boneIds.empty() ? blib::graphics::outlineVertexShaderName : blib::graphics::skinOutlineVertexShaderName);
    this->outlineVertexShader.setPath(outlineVertexShaderPath);
    this->outlineVertexShader.setType(blib::graphics::Shader::Type::vertex);
    this->outlineVertexShader.setRenderApi(&(ctx.api));
    {
        blib::graphics::ShaderError err = this->outlineVertexShader.compile();
        if (__blib_unlikely(err != blib::graphics::ShaderError::None))
        {
            __blib_log_warning("mesh bake: outline vertex shader compile failed (error %u)", static_cast<buint32>(err));
        }
    }

    this->outlineFragmentShader.setPath(std::string(blib::graphics::meshShaderBasePath) + blib::graphics::outlineFragmentShaderName);
    this->outlineFragmentShader.setType(blib::graphics::Shader::Type::fragment);
    this->outlineFragmentShader.setRenderApi(&(ctx.api));
    {
        blib::graphics::ShaderError err = this->outlineFragmentShader.compile();
        if (__blib_unlikely(err != blib::graphics::ShaderError::None))
        {
            __blib_log_warning("mesh bake: outline fragment shader compile failed (error %u)", static_cast<buint32>(err));
        }
    }

    this->outlineDrawer.setRenderApi(&(ctx.api));
    this->outlineDrawer.create();
    this->outlineDrawer.AttachShader(this->outlineVertexShader);
    this->outlineDrawer.AttachShader(this->outlineFragmentShader);
    {
        blib::graphics::ShaderError err = this->outlineDrawer.compile();
        if (__blib_unlikely(err != blib::graphics::ShaderError::None))
        {
            __blib_log_warning("mesh bake: outline program link failed (error %u)", static_cast<buint32>(err));
        }
    }

    this->material.bake(ctx);

    // Контекст, в котором созданы GL-ресурсы: нужен деструктору для
    // их освобождения (см. ~Mesh)
    __blib_this_context(this)->renderContext = &ctx;

    this->baked = true;
}

blib::graphics::Mesh::Mesh()
{
    // GL-контекст аллоцируется ЛЕНИВО в bake() через аллокатор
    // IAllocatorAware: конструктор не аллоцирует динамическую память
    // (требование кеша ресурсов — см. GRAPHICS.md, «Владение GL»)
    this->ctx = nullptr;
}

blib::graphics::Mesh::~Mesh()
{
    // Симметрично ленивой аллокации в bake(): возвращаем память
    // контекста через аллокатор IAllocatorAware. GL-ресурсы
    // (VAO/VBO/EBO) освобождаются, если они создавались (bake) и
    // контекст рендера ещё жив — порядок разрушения гарантирует
    // вызывающий (модель выгружается до окна/таргета)
    if (this->ctx)
    {
        oglMeshContext* meshCtx = __blib_this_context(this);
        if (meshCtx->renderContext &&
            meshCtx->renderContext->api.ogl.ext.__blib_glDeleteVertexArrays &&
            meshCtx->renderContext->api.ogl.ext.__blib_glDeleteBuffers)
        {
            meshCtx->renderContext->api.ogl.ext.__blib_glDeleteVertexArrays(1, &meshCtx->vao);
            meshCtx->renderContext->api.ogl.ext.__blib_glDeleteBuffers(static_cast<GLsizei>(MeshAttributeNames::vbCount), meshCtx->vbos);
            meshCtx->renderContext->api.ogl.ext.__blib_glDeleteBuffers(1, &meshCtx->faces);
            meshCtx->renderContext = nullptr;
        }

        this->deallocate(this->ctx, sizeof(oglMeshContext));
        this->ctx = nullptr;
    }
}

blib::graphics::Mesh::Mesh(Mesh&& other) noexcept
    : IDrawable()
    , ITransformable(std::move(other))
    , blib::memory::IAllocatorAware(std::move(other))
    , ctx(other.ctx)
    , baked(other.baked)
    , fragmentShader(std::move(other.fragmentShader))
    , vertexShader(std::move(other.vertexShader))
    , drawer(std::move(other.drawer))
    , outlineFragmentShader(std::move(other.outlineFragmentShader))
    , outlineVertexShader(std::move(other.outlineVertexShader))
    , outlineDrawer(std::move(other.outlineDrawer))
    , ngonencoding(other.ngonencoding)
    , primitiveType(other.primitiveType)
    , vertices(std::move(other.vertices))
    , normals(std::move(other.normals))
    , textureCoords(std::move(other.textureCoords))
    , colors(std::move(other.colors))
    , boneIds(std::move(other.boneIds))
    , boneWeights(std::move(other.boneWeights))
    , faces(std::move(other.faces))
    , material(std::move(other.material))
{
    // Источник обнуляется: его деструктор не тронет переданный ctx.
    // Аллокатор: у IAllocatorAware пользовательский dtor подавляет
    // implicit move-ctor, поэтому std::move(other) связывается с
    // copy-ctor базы — share(): обе стороны делят impl (ref-counting),
    // deallocate пойдёт через тот же аллокатор, что и allocate.
    // У источника ctx == nullptr — его деструктор ничего не деаллоцирует
    other.ctx = nullptr;
}

void blib::graphics::Mesh::draw(blib::graphics::RenderContext& ctx) const
{
    this->draw(ctx, nullptr);
}

void blib::graphics::Mesh::draw(blib::graphics::RenderContext& ctx, const std::vector<blib::graphics::TransformMatrix>* pBoneMatrices) const
{
    if (!(this->baked))
        this->bake(ctx);

    // Face culling включается на время отрисовки: pass 1 рисует
    // лицевые грани (cull back), pass 2 (контур) — задние (cull front).
    // Раньше culling не включался вовсе — меши обязаны иметь
    // корректную обмотку (CCW снаружи)
    ctx.api.ogl.__blib_glEnable(GL_CULL_FACE);
    ctx.api.ogl.__blib_glCullFace(GL_BACK);

    ctx.setShaderProgram(&(this->drawer));
    ctx.sendVievMatrixToShaderProgram();
    ctx.sendProjectionMatrixToShaderProgram();
    ctx.sendModelMatrixToShaderProgram(this->getTransform());

    if (pBoneMatrices)
    {
        ctx.sendBoneMatricesToShaderProgram(*pBoneMatrices);
    }

    // Свет и позиция камеры (NPR-пайплайн, фаза 4): шейдеры без
    // этих uniform'ов просто пропускают отправку
    ctx.sendLightsToShaderProgram();
    ctx.sendCameraPositionToShaderProgram();

    // Текстуры и material-униформы (shadingMode, rim, ramp, emission)
    // биндятся из материала — см. Material::apply
    this->material.apply(ctx, this->drawer);

    // Отладочная раскраска нормалями (RenderContext::showNormals):
    // шейдеры без этого uniform просто игнорируются (location == -1)
    const GLint showNormalsLocation = this->drawer.getUniformLocation("gShowNormals");
    if (showNormalsLocation != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1i(showNormalsLocation, ctx.showNormals ? 1 : 0);
    }

    // count = количество ИНДЕКСОВ (а не байт): раньше сюда передавался
    // размер в байтах, что заставляло читать за пределами EBO
    // и рисовать мусорные треугольники из невалидных индексов
    const GLsizei indexCount = static_cast<GLsizei>(
        this->faces[0].indices.size() * this->faces.size());

    ctx.api.ogl.ext.__blib_glBindVertexArray(__blib_this_context(this)->vao);
    ctx.api.ogl.ext.__blib_gl_glDrawElements(primitiveTypeToOGL(this->primitiveType), indexCount, GL_UNSIGNED_INT, 0);

    // Pass 2 — контур (inverted hull): раздутые задние грани плоским
    // цветом из материала. Без контура у меша с выключенной обмоткой
    // лишнего прохода нет
    if (this->material.outlineEnabled && this->material.outlineWidth > 0.0f)
    {
        ctx.setShaderProgram(&(this->outlineDrawer));
        ctx.sendVievMatrixToShaderProgram();
        ctx.sendProjectionMatrixToShaderProgram();
        ctx.sendModelMatrixToShaderProgram(this->getTransform());

        if (pBoneMatrices)
        {
            ctx.sendBoneMatricesToShaderProgram(*pBoneMatrices);
        }

        const GLint outlineWidthLocation = this->outlineDrawer.getUniformLocation("gOutlineWidth");
        if (outlineWidthLocation != -1)
        {
            ctx.api.ogl.ext.__blib_gl_glUniform1f(outlineWidthLocation, this->material.outlineWidth);
        }

        const GLint outlineColorLocation = this->outlineDrawer.getUniformLocation("gOutlineColor");
        if (outlineColorLocation != -1)
        {
            ctx.api.ogl.ext.__blib_gl_glUniform3f(
                outlineColorLocation,
                this->material.outlineColor.x,
                this->material.outlineColor.y,
                this->material.outlineColor.z);
        }

        // Только задние грани: раздутый силуэт не перекрывает
        // переднюю поверхность объекта
        ctx.api.ogl.__blib_glCullFace(GL_FRONT);
        ctx.api.ogl.ext.__blib_gl_glDrawElements(primitiveTypeToOGL(this->primitiveType), indexCount, GL_UNSIGNED_INT, 0);
        ctx.api.ogl.__blib_glCullFace(GL_BACK);
    }

    ctx.api.ogl.ext.__blib_glBindVertexArray(0);

    // Culling выключается — состояние для прочих проходов
    // (LineRenderer, пост-пасс) как раньше
    ctx.api.ogl.__blib_glDisable(GL_CULL_FACE);
}

//void blib::graphics::Mesh::draw(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const
//{
//    glPushMatrix();
//    {
//        ctx.applyTransform(*this);
//
//        glEnable(GL_TEXTURE_2D);
//        glBindTexture(GL_TEXTURE_2D, *((GLuint*)this->material.diffuse.getContext()));
//
//        switch (this->primitiveType)
//        {
//        case beng::graphics::PrimitiveType::Point:
//            this->ngonencoding ? glBegin(GL_POINTS) : glBegin(GL_POINT);
//            break;
//        case beng::graphics::PrimitiveType::Line:
//            this->ngonencoding ? glBegin(GL_LINE) : glBegin(GL_LINES);
//            break;
//       case beng::graphics::PrimitiveType::Triangle:
//           this->ngonencoding ? glBegin(GL_TRIANGLE_FAN) : glBegin(GL_TRIANGLES);
//           break;
//        case beng::graphics::PrimitiveType::Polygon:
//            glBegin(GL_POLYGON);
//            break;
//
//        default:
//            glBegin(GL_TRIANGLES);
//            break;
//        }
//        {
//            for (const auto& face : this->faces)
//            {
//                for (const auto& index : face.indices)
//                {
//                    const auto& vertex = this->vertices[index];
//
//                    //glColor3f(1.f, 1.f, 0.9f);
//
//                    if (this->normals.size() > 0)
//                    {
//                        const auto& normal = this->normals[index];
//                        ctx.api.ogl.__blib_glNormal3f(normal.x, normal.y, normal.z);
//                        //glNormal3f(normal.x, normal.y, normal.z);
//                    }
//                    if (this->textureCoords.size() > 0)
//                    {
//                        const auto& texturecoord = this->textureCoords[index];
//                        ctx.api.ogl.__blib_glTexCoord3f(texturecoord.x, texturecoord.y, texturecoord.z);
//                        //glTexCoord3f(texturecoord.x, texturecoord.y, texturecoord.z);
//                    }
//
//                    ctx.api.ogl.__blib_glVertex3f(vertex.x, vertex.y, vertex.z);
//                    //glVertex3f(vertex.x, vertex.y, vertex.z);
//                }
//            }
//
//        }
//        glEnd();
//
//        glBindTexture(GL_TEXTURE_2D, 0);
//    }
//    glPopMatrix();
//}

namespace
{
    // Ключи JSON-объекта меша (формат сериализации)
    constexpr const char* meshKeyPrimitiveType = "primitiveType";
    constexpr const char* meshKeyNgonEncoding = "ngonEncoding";
    constexpr const char* meshKeyVertices = "vertices";
    constexpr const char* meshKeyNormals = "normals";
    constexpr const char* meshKeyTextureCoords = "textureCoords";
    constexpr const char* meshKeyColors = "colors";
    constexpr const char* meshKeyBoneIds = "boneIds";
    constexpr const char* meshKeyBoneWeights = "boneWeights";
    constexpr const char* meshKeyFaces = "faces";
    constexpr const char* meshKeyMaterial = "material";

    // Количество каналов цвета (RGBA) и размеры векторов
    constexpr buint32 meshColorChannelCount = 4;
    constexpr buint32 meshVector3Size = 3;
    constexpr buint32 meshVector4Size = 4;

    // -------- Запись примитивов в JSON --------

    // Vector3f → массив [x, y, z]
    blib::core::json::JsonValue meshVector3fToArray(_In const blib::graphics::Vector3f& v)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        arr.pushBack(blib::core::json::JsonValue(v.x));
        arr.pushBack(blib::core::json::JsonValue(v.y));
        arr.pushBack(blib::core::json::JsonValue(v.z));
        return arr;
    }

    // Vector4f → массив [x, y, z, w]
    blib::core::json::JsonValue meshVector4fToArray(_In const blib::graphics::Vector4f& v)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        arr.pushBack(blib::core::json::JsonValue(v.x));
        arr.pushBack(blib::core::json::JsonValue(v.y));
        arr.pushBack(blib::core::json::JsonValue(v.z));
        arr.pushBack(blib::core::json::JsonValue(v.w));
        return arr;
    }

    // Vector4i → массив [x, y, z, w]
    blib::core::json::JsonValue meshVector4iToArray(_In const blib::graphics::Vector4i& v)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        arr.pushBack(blib::core::json::JsonValue(v.x));
        arr.pushBack(blib::core::json::JsonValue(v.y));
        arr.pushBack(blib::core::json::JsonValue(v.z));
        arr.pushBack(blib::core::json::JsonValue(v.w));
        return arr;
    }

    // Color → массив [r, g, b, a]
    blib::core::json::JsonValue meshColorToArray(_In const blib::graphics::Color& c)
    {
        blib::core::json::JsonValue arr = blib::core::json::JsonValue::makeArray();
        arr.pushBack(blib::core::json::JsonValue(c.red));
        arr.pushBack(blib::core::json::JsonValue(c.green));
        arr.pushBack(blib::core::json::JsonValue(c.blue));
        arr.pushBack(blib::core::json::JsonValue(c.alpha));
        return arr;
    }

    // -------- Чтение примитивов из JSON (false — неверная форма) --------

    bool meshReadVector3f(_In const blib::core::json::JsonValue& v, _Out blib::graphics::Vector3f& out)
    {
        if (!v.isArray() || v.size() != meshVector3Size)
        {
            return false;
        }
        out = blib::graphics::Vector3f(v[0].asBfloat(), v[1].asBfloat(), v[2].asBfloat());
        return true;
    }

    bool meshReadVector4f(_In const blib::core::json::JsonValue& v, _Out blib::graphics::Vector4f& out)
    {
        if (!v.isArray() || v.size() != meshVector4Size)
        {
            return false;
        }
        out = blib::graphics::Vector4f(v[0].asBfloat(), v[1].asBfloat(), v[2].asBfloat(), v[3].asBfloat());
        return true;
    }

    bool meshReadVector4i(_In const blib::core::json::JsonValue& v, _Out blib::graphics::Vector4i& out)
    {
        if (!v.isArray() || v.size() != meshVector4Size)
        {
            return false;
        }
        out = blib::graphics::Vector4i(
            static_cast<int>(v[0].asBint64()),
            static_cast<int>(v[1].asBint64()),
            static_cast<int>(v[2].asBint64()),
            static_cast<int>(v[3].asBint64()));
        return true;
    }

    bool meshReadColor(_In const blib::core::json::JsonValue& v, _Out blib::graphics::Color& out)
    {
        if (!v.isArray() || v.size() != meshColorChannelCount)
        {
            return false;
        }
        out = blib::graphics::Color(
            static_cast<buint8>(v[0].asBuint64()),
            static_cast<buint8>(v[1].asBuint64()),
            static_cast<buint8>(v[2].asBuint64()),
            static_cast<buint8>(v[3].asBuint64()));
        return true;
    }

    // Массив элементов одного вида: проверка формы каждого элемента.
    // reader возвращает false → весь результат отбрасывается
    template<typename T>
    bool meshReadElementArray(_In const blib::core::json::JsonValue& arr,
        _In bool (*reader)(_In const blib::core::json::JsonValue&, _Out T&),
        _Out std::vector<T>& out)
    {
        out.clear();
        if (!arr.isArray())
        {
            return false;
        }
        out.reserve(arr.size());
        for (buint32 i = 0; i < arr.size(); ++i)
        {
            T value;
            if (!reader(arr[i], value))
            {
                out.clear();
                return false;
            }
            out.push_back(value);
        }
        return true;
    }

    // -------- Сравнение (поэлементное, без готовых операторов ==) --------

    bool meshColorsEqual(_In const std::vector<blib::graphics::Color>& a,
        _In const std::vector<blib::graphics::Color>& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].red != b[i].red || a[i].green != b[i].green ||
                a[i].blue != b[i].blue || a[i].alpha != b[i].alpha)
            {
                return false;
            }
        }
        return true;
    }

    bool meshFacesEqual(_In const std::vector<blib::graphics::Face>& a,
        _In const std::vector<blib::graphics::Face>& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].indices != b[i].indices)
            {
                return false;
            }
        }
        return true;
    }

    bool meshDataEqual(_In const blib::graphics::Mesh& a, _In const blib::graphics::Mesh& b)
    {
        if (a.ngonencoding != b.ngonencoding || a.primitiveType != b.primitiveType)
        {
            return false;
        }
        // Векторы Vector3f/Vector4i/Vector4f имеют операторы ==
        if (a.vertices != b.vertices || a.normals != b.normals ||
            a.textureCoords != b.textureCoords)
        {
            return false;
        }
        if (!meshColorsEqual(a.colors, b.colors))
        {
            return false;
        }
        if (a.boneIds != b.boneIds || a.boneWeights != b.boneWeights)
        {
            return false;
        }
        return meshFacesEqual(a.faces, b.faces);
    }
}

blib::core::json::JsonValue blib::graphics::Mesh::toJson() const
{
    blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

    doc.set(meshKeyPrimitiveType, blib::core::json::JsonValue(static_cast<buint8>(this->primitiveType)));
    doc.set(meshKeyNgonEncoding, blib::core::json::JsonValue(this->ngonencoding));

    blib::core::json::JsonValue& verts = doc.set(meshKeyVertices, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::Vector3f& v : this->vertices)
    {
        verts.pushBack(meshVector3fToArray(v));
    }

    blib::core::json::JsonValue& norms = doc.set(meshKeyNormals, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::Vector3f& v : this->normals)
    {
        norms.pushBack(meshVector3fToArray(v));
    }

    blib::core::json::JsonValue& uvs = doc.set(meshKeyTextureCoords, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::Vector3f& v : this->textureCoords)
    {
        uvs.pushBack(meshVector3fToArray(v));
    }

    blib::core::json::JsonValue& colors = doc.set(meshKeyColors, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::Color& c : this->colors)
    {
        colors.pushBack(meshColorToArray(c));
    }

    blib::core::json::JsonValue& boneIds = doc.set(meshKeyBoneIds, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::Vector4i& v : this->boneIds)
    {
        boneIds.pushBack(meshVector4iToArray(v));
    }

    blib::core::json::JsonValue& boneWeights = doc.set(meshKeyBoneWeights, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::Vector4f& v : this->boneWeights)
    {
        boneWeights.pushBack(meshVector4fToArray(v));
    }

    blib::core::json::JsonValue& faces = doc.set(meshKeyFaces, blib::core::json::JsonValue::makeArray());
    for (const blib::graphics::Face& face : this->faces)
    {
        blib::core::json::JsonValue faceArr = blib::core::json::JsonValue::makeArray();
        for (buint32 index : face.indices)
        {
            faceArr.pushBack(blib::core::json::JsonValue(index));
        }
        faces.pushBack(faceArr);
    }

    // Материал (CPU-поля + битмап диффуза; GL-хендлы не сериализуемы)
    doc.set(meshKeyMaterial, this->material.toJson());

    return doc;
}

blib::core::LoadStatus blib::graphics::Mesh::fromJson(_In const blib::core::json::JsonValue& json)
{
    // Валидация формы ДО применения: при ошибке состояние не меняется
    if (!json.isObject() ||
        !json.has(meshKeyPrimitiveType) || !json.get(meshKeyPrimitiveType).isNumber() ||
        !json.has(meshKeyNgonEncoding) || !json.get(meshKeyNgonEncoding).isBool() ||
        !json.has(meshKeyVertices) || !json.has(meshKeyNormals) ||
        !json.has(meshKeyTextureCoords) || !json.has(meshKeyColors) ||
        !json.has(meshKeyBoneIds) || !json.has(meshKeyBoneWeights) ||
        !json.has(meshKeyFaces) || !json.has(meshKeyMaterial))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    const blib::graphics::PrimitiveType primitiveType = static_cast<blib::graphics::PrimitiveType>(
        json.get(meshKeyPrimitiveType).asBuint64());
    const bool ngonEncoding = json.get(meshKeyNgonEncoding).asBool();

    std::vector<blib::graphics::Vector3f> vertices;
    std::vector<blib::graphics::Vector3f> normals;
    std::vector<blib::graphics::Vector3f> textureCoords;
    std::vector<blib::graphics::Color> colors;
    std::vector<blib::graphics::Vector4i> boneIds;
    std::vector<blib::graphics::Vector4f> boneWeights;
    std::vector<blib::graphics::Face> faces;
    if (!meshReadElementArray<blib::graphics::Vector3f>(
            json.get(meshKeyVertices), meshReadVector3f, vertices) ||
        !meshReadElementArray<blib::graphics::Vector3f>(
            json.get(meshKeyNormals), meshReadVector3f, normals) ||
        !meshReadElementArray<blib::graphics::Vector3f>(
            json.get(meshKeyTextureCoords), meshReadVector3f, textureCoords) ||
        !meshReadElementArray<blib::graphics::Color>(
            json.get(meshKeyColors), meshReadColor, colors) ||
        !meshReadElementArray<blib::graphics::Vector4i>(
            json.get(meshKeyBoneIds), meshReadVector4i, boneIds) ||
        !meshReadElementArray<blib::graphics::Vector4f>(
            json.get(meshKeyBoneWeights), meshReadVector4f, boneWeights))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Грани: массив массивов индексов
    const blib::core::json::JsonValue& facesArr = json.get(meshKeyFaces);
    if (!facesArr.isArray())
    {
        return blib::core::LoadStatus::InvalidData;
    }
    for (buint32 i = 0; i < facesArr.size(); ++i)
    {
        const blib::core::json::JsonValue& faceArr = facesArr[i];
        if (!faceArr.isArray())
        {
            return blib::core::LoadStatus::InvalidData;
        }
        blib::graphics::Face face;
        for (buint32 j = 0; j < faceArr.size(); ++j)
        {
            face.indices.push_back(static_cast<buint32>(faceArr[j].asBuint64()));
        }
        faces.push_back(face);
    }

    // Материал: валидация во временный объект (только CPU-поля)
    blib::graphics::Material material;
    if (__blib_unlikely(material.fromJson(json.get(meshKeyMaterial)) != blib::core::LoadStatus::None))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Все данные валидны — пересоздать меш поверх старого: GL-кэш
    // (VAO/VBO/EBO/шейдеры) старой геометрии невалиден после
    // перезаписи CPU-данных; свежий меш перезапечётся в draw().
    // Перезапись ЗАПЕЧЁННОГО меша требует живого RenderContext
    // (деструктор освобождает GL-ресурсы) — см. GRAPHICS.md
    this->~Mesh();
    new (this) blib::graphics::Mesh();

    this->primitiveType = primitiveType;
    this->ngonencoding = ngonEncoding;
    this->vertices = std::move(vertices);
    this->normals = std::move(normals);
    this->textureCoords = std::move(textureCoords);
    this->colors = std::move(colors);
    this->boneIds = std::move(boneIds);
    this->boneWeights = std::move(boneWeights);
    this->faces = std::move(faces);
    this->material = std::move(material);

    return blib::core::LoadStatus::None;
}

blib::core::SaveStatus blib::graphics::Mesh::save(_In blib::core::IOutputStream& os) const
{
    const blib::core::json::JsonValue doc = this->toJson();
    if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
    {
        return blib::core::SaveStatus::WriteFailed;
    }
    return blib::core::SaveStatus::None;
}

blib::core::LoadStatus blib::graphics::Mesh::load(_In blib::core::IInputStream& is)
{
    blib::core::json::JsonParser parser;
    blib::core::json::JsonValue doc;
    if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
    {
        return blib::core::LoadStatus::InvalidData;
    }
    return this->fromJson(doc);
}

bool blib::graphics::Mesh::strongCompare(_In const blib::core::IStrongComparable& other,
    _In blib::core::CompareSession& session) const
{
    if (!session.enter(this, &other))
    {
        return true;
    }

    const blib::graphics::Mesh& o = static_cast<const blib::graphics::Mesh&>(other);

    // Сравниваются только сериализуемые CPU-данные; GL-состояние
    // (ctx, шейдеры, baked) не сериализуется и не сравнивается
    return meshDataEqual(*this, o) &&
        this->material.strongCompare(o.material, session);
}

bool blib::graphics::Mesh::verify() const
{
    // Round-trip без RTTI (см. blib::core::verifyRoundTrip).
    // ПЕРЕЗАПИСЬ НЕЗАПЕЧЁННОГО меша безопасна без RenderContext
    return blib::core::verifyRoundTrip(*this);
}
