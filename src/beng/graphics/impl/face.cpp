#include <beng/graphics/face.h>

#include <stdexcept>

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

std::vector<buint32> beng::graphics::compileFaces(const beng::graphics::Faces& faces)
{
    std::vector<buint32> res;
    
    if (faces.size() == 0)
        throw std::runtime_error("Incorrect faces size");

    size_t fsz = faces[0].indices.size();

    if(fsz == 0)
        throw std::runtime_error("Incorrect faces size");

    // Check that all faces have same sizes
    for (const beng::graphics::Face& f : faces)
    {
        if(f.indices.size() != fsz)
            throw std::runtime_error("Incorrect faces size");
    }

    res.resize(faces.size() * fsz);
    for (size_t i = 0; i < res.size(); ++i)
    {
        res[i] = faces[i / fsz].indices[i % fsz];
    }

    return res;
}
