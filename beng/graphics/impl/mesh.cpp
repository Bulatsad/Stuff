#include <beng/graphics/mesh.h>

#include <Windows.h>
#include <gl/GL.h>

enum class MeshAttributeNames : GLuint
{
    position = 0,
    color = 1,
    textureCoords = 2,
    normals = 3,
    indices = 4,
    vbCount
};

struct oglMeshContext
{
    GLuint vao;
    GLuint vbos[(GLuint)MeshAttributeNames::vbCount];
};

#define __blib_this_context(_this) (static_cast<oglMeshContext*>(_this->ctx))

static inline void loadVertexFromAssimp(beng::graphics::Mesh& bengmesh, const aiMesh* paimesh)
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

static inline void loadNormalsFromAssimp(beng::graphics::Mesh& bengmesh, const aiMesh* paimesh)
{

}

static inline void loadTextureCoordinatesFromAssimp(beng::graphics::Mesh& bengmesh, const aiMesh* paimesh)
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

static inline void loadFacesFromAssimp(beng::graphics::Mesh& bengmesh, const aiMesh* paimesh)
{
    bengmesh.faces.resize(paimesh->mNumFaces);
    for (size_t i = 0; i < paimesh->mNumFaces; ++i)
    {

    }
}

void beng::graphics::Mesh::loadFromAssimpMesh(const aiMesh* paimesh)
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
        throw std::exception("unknown primitive type");
        break;
    }

    loadVertexFromAssimp(*this, paimesh);
    loadTextureCoordinatesFromAssimp(*this, paimesh);

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

void beng::graphics::Mesh::bake(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const
{
    // create VAO
    ctx.api.ogl.ext.__blib_glGenVertexArrays(1, &(__blib_this_context(this)->vao));
    ctx.api.ogl.ext.__blib_glBindVertexArray(__blib_this_context(this)->vao);

    // create vbo's
    ctx.api.ogl.ext.__blib_glGenBuffers((GLuint)MeshAttributeNames::vbCount, (__blib_this_context(this)->vbos));

    // populate position vbo and enable attribute
    ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::position]);
    ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(this->vertices[0]) * this->vertices.size(), this->vertices.data(), GL_STATIC_DRAW);
    ctx.api.ogl.ext.__blib_glEnableVertexAttribArray((GLuint)MeshAttributeNames::position);
    ctx.api.ogl.ext.__blib_glVertexAttribPointer((GLuint)MeshAttributeNames::position, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // populate position vbo and enable attribute color
    {
        if (this->colors.size() != this->vertices.size())
        {
            throw std::exception("Colors size must be equal to vertices size");
        }
        auto tmpFloatColors = blib::graphics::makeFloatData(this->colors);
        ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::color]);
        ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(tmpFloatColors[0]) * tmpFloatColors.size(), tmpFloatColors.data(), GL_STATIC_DRAW);
        ctx.api.ogl.ext.__blib_glEnableVertexAttribArray((GLuint)MeshAttributeNames::color);
        ctx.api.ogl.ext.__blib_glVertexAttribPointer((GLuint)MeshAttributeNames::color, 4, GL_FLOAT, GL_FALSE, 0, 0);
    }

    // populate texturecoords vbo and enable attribute
    //ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::textureCoords]);
    //ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(this->textureCoords[0]) * this->textureCoords.size(), this->textureCoords.data(), GL_STATIC_DRAW);
    //ctx.api.ogl.ext.__blib_glEnableVertexAttribArray((GLuint)MeshAttributeNames::textureCoords);
    //ctx.api.ogl.ext.__blib_glVertexAttribPointer((GLuint)MeshAttributeNames::textureCoords, 3, GL_FLOAT, GL_FALSE, 0, 0);

    //// populate normals vbo and enable attribute
    //ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[normalAttributeName]);
    //ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(this->normals[0]) * this->normals.size(), this->normals.data(), GL_STATIC_DRAW);
    //ctx.api.ogl.ext.__blib_glEnableVertexAttribArray(normalAttributeName);
    //ctx.api.ogl.ext.__blib_glVertexAttribPointer(normalAttributeName, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // TODO : Triangulate and rewrite renderer
    //// populate indices vbo
    //ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbos[(GLuint)MeshAttributeNames::indices]);
    //ctx.api.ogl.ext.__blib_glBufferData(GL_ARRAY_BUFFER, sizeof(this->faces[0]) * this->faces.size(), this->faces.data(), GL_STATIC_DRAW);


    ctx.api.ogl.ext.__blib_glBindVertexArray(GL_NULL_VERTEX_BUFFER);
    ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, GL_NULL_VERTEX_BUFFER);


    this->vertexShader.setPath("M:\\Stuff\\shaders\\mesh\\MeshVertexShader.glsl");
    this->vertexShader.setType(blib::graphics::Shader::Type::vertex);
    this->vertexShader.setRenderApi(&(ctx.api));
    this->vertexShader.compile();

    this->fragmentShader.setPath("M:\\Stuff\\shaders\\mesh\\MeshFragmentShader.glsl");
    this->fragmentShader.setType(blib::graphics::Shader::Type::fragment);
    this->fragmentShader.setRenderApi(&(ctx.api));
    this->fragmentShader.compile();

    this->drawer.setRenderApi(&(ctx.api));
    this->drawer.create();
    this->drawer.AttachShader(this->vertexShader);
    this->drawer.AttachShader(this->fragmentShader);
    this->drawer.compile();

    this->baked = true;
}

beng::graphics::Mesh::Mesh()
{
    this->ctx = new oglMeshContext();
    memset(this->ctx, 0, sizeof(oglMeshContext));
}

void beng::graphics::Mesh::draw(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const
{
    if (!(this->baked))
        this->bake(target, ctx);

    ctx.setShaderProgram(&(this->drawer));
    ctx.sendVievMatrixToShaderProgram();
    ctx.sendProjectionMatrixToShaderProgram();

    ctx.api.ogl.ext.__blib_gl_glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, *((GLuint*)this->material.diffuse.getContext()));
    GLint samplerPos = ctx.api.ogl.ext.__blib_gl_glGetUniformLocation(this->drawer.getContext(), "textureSampler");
    ctx.api.ogl.ext.__blib_gl_glUniform1i(samplerPos, 0);

    ctx.api.ogl.ext.__blib_glBindVertexArray(__blib_this_context(this)->vao);

    ctx.api.ogl.ext.__blib_gl_glDrawArrays(GL_TRIANGLES, 0, this->vertices.size());

    ctx.api.ogl.ext.__blib_glBindVertexArray(0);
}

//void beng::graphics::Mesh::draw(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const
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
