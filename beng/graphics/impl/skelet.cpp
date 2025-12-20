#include <beng/graphics/skelet.h>

#include <stack>

beng::graphics::Bone* beng::graphics::Skelet::find(const std::string& name)
{
    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        if (this->boneStorage[i].name == name)
            return &(this->boneStorage[i]);
    }
    return nullptr;
}

bool beng::graphics::Skelet::makeBoneTree(const aiNode* pbone)
{
    struct boneTreeMakerCtx
    {
        const aiNode* pnode = nullptr;
        beng::graphics::Bone* bone = nullptr;
        beng::graphics::Bone* prev = nullptr;
    };

    std::stack<boneTreeMakerCtx> st;
    {
        //for (unsigned int i = 0; i < pbone->mNumChildren; ++i)
        //{
        //    if (!storedBone)
        //    {
        //        throw std::runtime_error("Bone name not found");
        //        return false;
        //    }

        //    boneTreeMakerCtx newCtx;
        //    newCtx.bone = storedBone;
        //    newCtx.pnode = pbone->mChildren[i];
        //    newCtx.prev = nullptr;
        //    st.push(newCtx);
        //}
        beng::graphics::Bone* storedBone = this->find(pbone->mName.C_Str());

        boneTreeMakerCtx newCtx;
        newCtx.bone = storedBone;
        newCtx.pnode = pbone;
        newCtx.prev = nullptr;
        st.push(newCtx);

    }

    while (!(st.empty()))
    {
        boneTreeMakerCtx ctx = st.top();
        st.pop();

        for (unsigned int i = 0; i < ctx.pnode->mNumChildren; ++i)
        {
            beng::graphics::Bone* storedBone = this->find(ctx.pnode->mChildren[i]->mName.C_Str());
            if (!storedBone)
            {
                throw std::runtime_error("Bone name not finded");
                return false;
            }
            
            ctx.bone->addChild(storedBone);

            boneTreeMakerCtx newCtx;
            newCtx.bone = storedBone;
            newCtx.pnode = ctx.pnode->mChildren[i];
            newCtx.prev = ctx.bone;
            st.push(newCtx);
        }

        if (!(ctx.bone->getParent()))
            ctx.bone->setParent(ctx.prev);
        else
        {
            throw std::runtime_error("Reset bone parent");
            return false;
        }
    }

    return true;
}

bool beng::graphics::Skelet::loadFromAssimp(const aiMesh* paimesh)
{
    if (paimesh->mNumBones == 0)
    {
        throw std::runtime_error("No bones to load");
        return false;
    }

    if (!(paimesh->mBones[0]->mArmature))
    {
        throw std::runtime_error("Armature field not populated");
        return false;
    }

    for (unsigned int i = 0; i < paimesh->mNumBones; ++i)
    {
        if (paimesh->mBones[0]->mArmature != paimesh->mBones[i]->mArmature)
        {
            throw std::runtime_error("Meshes have different armatures");
            return false;
        }
    }

    this->boneStorage.resize(paimesh->mNumBones);

    for (unsigned int i = 0; i < paimesh->mNumBones; ++i)
    {
        if(!(this->boneStorage[i].loadFromAssimp(paimesh->mBones[i])))
        {
            throw std::runtime_error("Error on loading bone");
            return false;
        }
    }

    //makeBoneTree(this->boneStorage, paimesh->mBones[0]->mArmature);

    return true;
}

std::vector<beng::graphics::Bone>& beng::graphics::Skelet::getBoneStorage()
{
    return this->boneStorage;
}
