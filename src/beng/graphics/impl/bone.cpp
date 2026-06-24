#include <beng/graphics/bone.h>

bool beng::graphics::Bone::loadFromAssimp(const aiBone* pbone)
{
    this->name = std::string(pbone->mName.C_Str());

    if (this->name.empty())
    {
        throw std::runtime_error("Empty bone name");
        return false;
    }

    this->offsetMatrix = blib::graphics::TransformMatrix(
        {
            pbone->mOffsetMatrix.a1,pbone->mOffsetMatrix.a2, pbone->mOffsetMatrix.a3, pbone->mOffsetMatrix.a4,
            pbone->mOffsetMatrix.b1,pbone->mOffsetMatrix.b2, pbone->mOffsetMatrix.b3, pbone->mOffsetMatrix.b4,
            pbone->mOffsetMatrix.c1,pbone->mOffsetMatrix.c2, pbone->mOffsetMatrix.c3, pbone->mOffsetMatrix.c4,
            pbone->mOffsetMatrix.d1,pbone->mOffsetMatrix.d2, pbone->mOffsetMatrix.d3, pbone->mOffsetMatrix.d4
        });

    this->weights.resize(pbone->mNumWeights);

    for (size_t i = 0; i < this->weights.size(); ++i)
    {
        this->weights[i] = std::make_pair(pbone->mWeights[i].mVertexId, pbone->mWeights[i].mWeight);
    }

    return true;
}
