#include <beng/graphics/face.h>

void beng::graphics::Face::loadFromAssimpFace(const aiFace* paiface)
{
    this->indices.resize(paiface->mNumIndices);
    for (size_t i = 0; i < paiface->mNumIndices; ++i)
    {
        auto& bengindex = this->indices[i];
        auto& aiindex = paiface->mIndices[i];

        bengindex = aiindex;
    }
}
