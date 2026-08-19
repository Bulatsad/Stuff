#include <blib/graphics/skinmesh.h>

#include <iostream>
#include <stdexcept>

bool blib::graphics::SkinMesh::loadFromAssimpMesh(const aiMesh* paimesh, const blib::graphics::Skelet& skelet)
{
    this->mesh.loadFromAssimpMesh(paimesh);

    if (paimesh->mNumBones == 0)
    {
        return true;
    }

    this->mesh.boneIds.resize(paimesh->mNumVertices, blib::graphics::Vector4i(0, 0, 0, 0));
    this->mesh.boneWeights.resize(paimesh->mNumVertices, blib::graphics::Vector4f(0, 0, 0, 0));
    std::vector<size_t> weightCount(paimesh->mNumVertices, 0);

    for (unsigned int b = 0; b < paimesh->mNumBones; ++b)
    {
        const aiBone* paibone = paimesh->mBones[b];

        size_t boneIndex = skelet.findBoneIndex(paibone->mName.C_Str());
        if (boneIndex >= skelet.getBoneStorage().size())
        {
            throw std::runtime_error("Bone not found in skelet while loading mesh weights");
            return false;
        }
        if (boneIndex >= __blib_max_bones)
        {
            throw std::runtime_error("Too many bones for skinning shader");
            return false;
        }

        for (unsigned int w = 0; w < paibone->mNumWeights; ++w)
        {
            size_t vertexId = paibone->mWeights[w].mVertexId;
            float weight = paibone->mWeights[w].mWeight;

            size_t slot = weightCount[vertexId];
            if (slot >= 4)
            {
                // TODO : Logging
                continue;
            }

            this->mesh.boneIds[vertexId].data[slot] = static_cast<int>(boneIndex);
            this->mesh.boneWeights[vertexId].data[slot] = weight;
            weightCount[vertexId]++;
        }
    }

    // TEMP DEBUG
    if (!(this->mesh.boneIds.empty()))
    {
        int raw = this->mesh.boneIds[0].data[0];
        float asFloat = *reinterpret_cast<float*>(&raw);
        std::cout << "boneIds[0] raw=" << raw << " hex=0x" << std::hex << raw << std::dec << " asFloat=" << asFloat
            << " sizeOfElem=" << sizeof(this->mesh.boneIds[0]) << " vecSize=" << this->mesh.boneIds.size()
            << " vertSize=" << this->mesh.vertices.size() << std::endl;
        std::cout << "boneWeights[0] = " << this->mesh.boneWeights[0].data[0] << " " << this->mesh.boneWeights[0].data[1] << " " << this->mesh.boneWeights[0].data[2] << " " << this->mesh.boneWeights[0].data[3] << std::endl;
    }

    return true;
}
