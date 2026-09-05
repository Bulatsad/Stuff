#include <blib/graphics/skinmesh.h>

#include <blib/core/console/console.h>

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
        if (__blib_unlikely(boneIndex >= skelet.getBoneStorage().size()))
        {
            __blib_log_error("bone '%s' not found in skelet while loading mesh weights", paibone->mName.C_Str());
            return false;
        }
        if (__blib_unlikely(boneIndex >= __blib_max_bones))
        {
            __blib_log_error("too many bones for skinning shader (bone '%s' at index %zu, max %d)", paibone->mName.C_Str(), boneIndex, __blib_max_bones);
            return false;
        }

        for (unsigned int w = 0; w < paibone->mNumWeights; ++w)
        {
            size_t vertexId = paibone->mWeights[w].mVertexId;
            float weight = paibone->mWeights[w].mWeight;

            size_t slot = weightCount[vertexId];
            if (slot >= 4)
            {
                // Шейдер держит максимум 4 веса на вершину — лишние
                // отбрасываем, но предупреждаем о потере данных
                __blib_log_warning("vertex %zu of mesh has more than 4 bone weights, extra weights skipped (bone '%s')",
                    vertexId, paibone->mName.C_Str());
                continue;
            }

            this->mesh.boneIds[vertexId].data[slot] = static_cast<int>(boneIndex);
            this->mesh.boneWeights[vertexId].data[slot] = weight;
            weightCount[vertexId]++;
        }
    }

    return true;
}
