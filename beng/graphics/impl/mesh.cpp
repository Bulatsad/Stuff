#include <beng/graphics/mesh.h>

#include <Windows.h>
#include <gl/GL.h>

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

void beng::graphics::Mesh::draw(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const
{
    glPushMatrix();
    {
        target.applyTransform(*this);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, *((GLuint*)this->material.diffuse.getContext()));

        switch (this->primitiveType)
        {
        case beng::graphics::PrimitiveType::Point:
            this->ngonencoding ? glBegin(GL_POINTS) : glBegin(GL_POINT);
            break;
        case beng::graphics::PrimitiveType::Line:
            this->ngonencoding ? glBegin(GL_LINE) : glBegin(GL_LINES);
            break;
       case beng::graphics::PrimitiveType::Triangle:
           this->ngonencoding ? glBegin(GL_TRIANGLE_FAN) : glBegin(GL_TRIANGLES);
           break;
        case beng::graphics::PrimitiveType::Polygon:
            glBegin(GL_POLYGON);
            break;

        default:
            glBegin(GL_TRIANGLES);
            break;
        }
        {
            for (const auto& face : this->faces)
            {
                for (const auto& index : face.indices)
                {
                    const auto& vertex = this->vertices[index];

                    //glColor3f(1.f, 1.f, 0.9f);

                    if (this->normals.size() > 0)
                    {
                        const auto& normal = this->normals[index];
                        glNormal3f(normal.x, normal.y, normal.z);
                    }
                    if (this->textureCoords.size() > 0)
                    {
                        const auto& texturecoord = this->textureCoords[index];
                        glTexCoord3f(texturecoord.x, texturecoord.y, texturecoord.z);
                    }

                    glVertex3f(vertex.x, vertex.y, vertex.z);
                }
            }

        }
        glEnd();

        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glPopMatrix();
}
