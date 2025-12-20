#include <beng/graphics/skinmesh.h>

void beng::graphics::SkinMesh::loadFromAssimp(const aiScene* paiscene)
{

    this->meshes.resize(paiscene->mNumMeshes);
    for (size_t i = 0; i < this->meshes.size(); ++i)
    {
        this->meshes[i].loadFromAssimpMesh(paiscene->mMeshes[i]);

        beng::graphics::Skelet tempSkelet;
        tempSkelet.loadFromAssimp(paiscene->mMeshes[i]);

        for (auto& bone : tempSkelet.getBoneStorage())
            this->skelet.getBoneStorage().emplace_back(std::move(bone));
    }

    {
        beng::graphics::Bone root;
        root.name = paiscene->mMeshes[0]->mBones[0]->mArmature->mName.C_Str();
        root.offsetMatrix.loadIdentity();
        this->skelet.getBoneStorage().emplace_back(std::move(root));
        this->skelet.root = this->skelet.find(root.name);
    }

    this->skelet.makeBoneTree(paiscene->mMeshes[0]->mBones[0]->mArmature);
    return;
}
