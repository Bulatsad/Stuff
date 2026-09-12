#include <blib/graphics/skinmesh.h>

#include <blib/core/console/console.h>

namespace
{
    // Слоты весов вершины, которые занимает шейдер (максимум 4)
    constexpr size_t maxWeightsPerVertex = 4;

    // Порог суммы весов, ниже которого вершина считается
    // неренормализуемой (остаётся с нулевыми весами)
    constexpr float zeroWeightSumEpsilon = 1e-6f;
}

bool blib::graphics::SkinMesh::loadFromAssimpMesh(
    _In const aiMesh* paimesh,
    _In const blib::graphics::Skelet& skelet,
    _In bool force,
    _In_opt const blib::graphics::Skelet* candidateSkelet)
{
    this->mesh.loadFromAssimpMesh(paimesh);

    if (paimesh->mNumBones == 0)
    {
        return true;
    }

    this->mesh.boneIds.resize(paimesh->mNumVertices, blib::graphics::Vector4i(0, 0, 0, 0));
    this->mesh.boneWeights.resize(paimesh->mNumVertices, blib::graphics::Vector4f(0, 0, 0, 0));
    std::vector<size_t> weightCount(paimesh->mNumVertices, 0);

    // Флаг: в force-режиме отбрасывались веса неизвестных костей —
    // после раскладки такие вершины ренормализуются к сумме 1
    bool droppedWeights = false;

    for (unsigned int b = 0; b < paimesh->mNumBones; ++b)
    {
        const aiBone* paibone = paimesh->mBones[b];

        size_t boneIndex = skelet.findBoneIndex(paibone->mName.C_Str());
        if (__blib_unlikely(boneIndex >= skelet.getBoneStorage().size()))
        {
            if (!force)
            {
                __blib_log_error("bone '%s' not found in skelet while loading mesh weights", paibone->mName.C_Str());
                return false;
            }

            // Force-режим: поднимаемся по иерархии скелета-кандидата,
            // пока не найдём предка, существующего в текущем скелете
            // (у Mixamo ленты-волосы Ribbon* висят на Head — их веса
            // переедут туда, и волосы будут следовать за головой
            // вместо схлопывания в начало координат)
            size_t fallbackIndex = skelet.getBoneStorage().size();
            std::string fallbackName;
            if (candidateSkelet)
            {
                const blib::graphics::Bone* candidateBone = candidateSkelet->find(paibone->mName.C_Str());
                const blib::graphics::IHierarchal* parent = candidateBone ? candidateBone->getParent() : nullptr;
                while (parent)
                {
                    const blib::graphics::Bone* parentBone = static_cast<const blib::graphics::Bone*>(parent);
                    fallbackIndex = skelet.findBoneIndex(parentBone->name);
                    if (fallbackIndex < skelet.getBoneStorage().size())
                    {
                        fallbackName = parentBone->name;
                        break;
                    }
                    parent = parent->getParent();
                }
            }

            if (fallbackIndex < skelet.getBoneStorage().size())
            {
                __blib_log_warning("bone '%s' not found in skelet, weights remapped to ancestor '%s' (forced skin)",
                    paibone->mName.C_Str(), fallbackName.c_str());
                boneIndex = fallbackIndex;
            }
            else
            {
                // Предка нет (или скелет-кандидат не передан) — веса
                // отбрасываются, остальные будут ренормализованы ниже
                __blib_log_warning("bone '%s' not found in skelet, weights dropped (forced skin)",
                    paibone->mName.C_Str());
                droppedWeights = true;
                continue;
            }
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
            if (slot >= maxWeightsPerVertex)
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

    if (droppedWeights)
    {
        // Ренормализация вершин, потерявших часть весов в force-режиме:
        // скиннинг-шейдер ожидает сумму 1, иначе вершина «схлопывается»
        // к оставшимся костям. Вершины с нулевой суммой (все кости
        // неизвестны) остаются нулевыми — о них предупреждаем.
        size_t zeroSumVertices = 0;
        for (size_t v = 0; v < this->mesh.boneWeights.size(); ++v)
        {
            blib::graphics::Vector4f& weights = this->mesh.boneWeights[v];

            float sum = 0.0f;
            for (size_t s = 0; s < maxWeightsPerVertex; ++s)
            {
                sum += weights.data[s];
            }

            if (sum <= zeroWeightSumEpsilon)
            {
                ++zeroSumVertices;
                continue;
            }

            for (size_t s = 0; s < maxWeightsPerVertex; ++s)
            {
                weights.data[s] /= sum;
            }
        }

        if (zeroSumVertices > 0)
        {
            __blib_log_warning("forced skin: %zu vertices have no weights for any known bone", zeroSumVertices);
        }
    }

    return true;
}
