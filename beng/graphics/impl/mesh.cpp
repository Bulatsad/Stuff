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

static inline void loadFacesFromAssimp(beng::graphics::Mesh& bengmesh, const aiMesh* paimesh)
{
    bengmesh.faces.resize(paimesh->mNumFaces);
    for (size_t i = 0; i < paimesh->mNumFaces; ++i)
    {

    }
}

void beng::graphics::Mesh::loadFromAssimpMesh(const aiMesh* paimesh)
{
    loadVertexFromAssimp(*this, paimesh);

    this->faces.resize(paimesh->mNumFaces);
    for (size_t i = 0; i < this->faces.size(); ++i)
    {
        auto& bengface = this->faces[i];
        const auto& aiface = paimesh->mFaces[i];

        bengface.loadFromAssimpFace(&aiface);
    }

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

void beng::graphics::Mesh::draw(blib::graphics::RenderWindow& wnd)
{
    glPushMatrix();
    {

        glBegin(GL_TRIANGLES);
        {
            for (const auto& face : this->faces)
            {
                for (const auto& index : face.indices)
                {
                    const auto& vertex = this->vertices[index];
                    const auto& normal = this->normals[index];

                    glColor3f(1.f, 1.f, 0.9f);

                    glNormal3f(normal.x, normal.y, normal.z);

                    glVertex3f(vertex.x, vertex.y, vertex.z);
                }
            }

        }
        glEnd();

    }
    glPopMatrix();
}
