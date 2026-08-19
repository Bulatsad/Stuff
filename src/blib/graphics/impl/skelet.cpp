#include <blib/graphics/skelet.h>

#include <stack>
#include <stdexcept>

blib::graphics::Bone* blib::graphics::Skelet::find(const std::string& name)
{
    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        if (this->boneStorage[i].name == name)
            return &(this->boneStorage[i]);
    }
    return nullptr;
}

const blib::graphics::Bone* blib::graphics::Skelet::find(const std::string& name) const
{
    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        if (this->boneStorage[i].name == name)
            return &(this->boneStorage[i]);
    }
    return nullptr;
}

size_t blib::graphics::Skelet::findBoneIndex(const std::string& name) const
{
    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        if (this->boneStorage[i].name == name)
            return i;
    }
    return this->boneStorage.size();
}

bool blib::graphics::Skelet::makeBoneTree(const aiNode* pbone)
{
    struct boneTreeMakerCtx
    {
        const aiNode* pnode = nullptr;
        blib::graphics::Bone* bone = nullptr;
        blib::graphics::Bone* prev = nullptr;
    };

    std::stack<boneTreeMakerCtx> st;
    {
        boneTreeMakerCtx newCtx;
        newCtx.bone = this->root;
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
            blib::graphics::Bone* storedBone = this->find(ctx.pnode->mChildren[i]->mName.C_Str());
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

        if (ctx.bone->getParent() != ctx.prev)
        {
            throw std::runtime_error("Reset bone parent");
            return false;
        }
    }

    return true;
}

bool blib::graphics::Skelet::loadDefaultPoseFromArmature(const aiNode* pbone)
{
    struct poseLoaderCtx
    {
        const aiNode* pnode = nullptr;
    };

    std::stack<poseLoaderCtx> st;
    {
        poseLoaderCtx newCtx;
        newCtx.pnode = pbone;
        st.push(newCtx);
    }

    while (!(st.empty()))
    {
        poseLoaderCtx ctx = st.top();
        st.pop();

        blib::graphics::Bone* bone = this->find(ctx.pnode->mName.C_Str());
        if (bone)
        {
            const aiMatrix4x4& m = ctx.pnode->mTransformation;
            bone->localTransform = blib::graphics::TransformMatrix(
                {
                    m.a1, m.b1, m.c1, m.d1,
                    m.a2, m.b2, m.c2, m.d2,
                    m.a3, m.b3, m.c3, m.d3,
                    m.a4, m.b4, m.c4, m.d4
                });
            bone->globalTransform = bone->localTransform;
        }

        for (unsigned int i = 0; i < ctx.pnode->mNumChildren; ++i)
        {
            poseLoaderCtx newCtx;
            newCtx.pnode = ctx.pnode->mChildren[i];
            st.push(newCtx);
        }
    }

    return true;
}

bool blib::graphics::Skelet::finishFromArmature(const aiNode* armature)
{
    blib::graphics::Bone* rootBone = this->find(armature->mName.C_Str());
    if (!rootBone)
    {
        this->boneStorage.emplace_back();
        rootBone = &(this->boneStorage.back());
        rootBone->name = armature->mName.C_Str();
        rootBone->offsetMatrix.loadIdentity();
        rootBone->localTransform.loadIdentity();
        rootBone->globalTransform.loadIdentity();
    }
    this->root = rootBone;

    this->finalMatrices.resize(this->boneStorage.size());

    if (!(this->makeBoneTree(armature)))
    {
        return false;
    }
    if (!(this->loadDefaultPoseFromArmature(armature)))
    {
        return false;
    }

    return true;
}

bool blib::graphics::Skelet::loadFromAssimp(const aiMesh* paimesh)
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

    return this->finishFromArmature(paimesh->mBones[0]->mArmature);
}

bool blib::graphics::Skelet::loadFromAssimp(const aiScene* paiscene)
{
    const aiNode* armature = nullptr;

    for (unsigned int m = 0; m < paiscene->mNumMeshes; ++m)
    {
        const aiMesh* paimesh = paiscene->mMeshes[m];
        for (unsigned int b = 0; b < paimesh->mNumBones; ++b)
        {
            if (!armature)
            {
                armature = paimesh->mBones[b]->mArmature;
            }

            if (!(this->find(paimesh->mBones[b]->mName.C_Str())))
            {
                this->boneStorage.emplace_back();
                if (!(this->boneStorage.back().loadFromAssimp(paimesh->mBones[b])))
                {
                    throw std::runtime_error("Error on loading bone");
                    return false;
                }
            }
        }
    }

    if (this->boneStorage.empty())
    {
        throw std::runtime_error("No bones to load");
        return false;
    }

    if (!armature)
    {
        throw std::runtime_error("Armature field not populated");
        return false;
    }

    return this->finishFromArmature(armature);
}

void blib::graphics::Skelet::updateTransforms(blib::graphics::Bone* pbone)
{
    const blib::graphics::IHierarchal* pParent = pbone->getParent();
    if (pParent)
    {
        pbone->globalTransform = blib::graphics::mul(static_cast<const blib::graphics::Bone*>(pParent)->globalTransform, pbone->localTransform);
    }
    else
    {
        pbone->globalTransform = pbone->localTransform;
    }

    for (blib::graphics::IHierarchal* pchild : pbone->getChilds())
    {
        this->updateTransforms(static_cast<blib::graphics::Bone*>(pchild));
    }
}

void blib::graphics::Skelet::applyClip(const blib::graphics::AnimationClip& clip, double timeTicks)
{
    // restore default (bind pose) globals for bones without animated channels
    if (this->root)
    {
        this->updateTransforms(this->root);
    }

    // MD5/Assimp animation channels store ABSOLUTE joint transforms (not node-local),
    // so sampled values are used directly as bone global transforms
    for (const blib::graphics::AnimationChannel& channel : clip.channels)
    {
        blib::graphics::Bone* pbone = this->find(channel.boneName);
        if (!pbone)
        {
            continue;
        }

        blib::graphics::Vector3f position;
        blib::math::Quaternion<double> rotation;
        blib::graphics::Vector3f scale;

        if (!(channel.sample(timeTicks, position, rotation, scale)))
        {
            continue;
        }

        pbone->globalTransform = blib::graphics::composeMatrix(position, rotation, scale);
    }

    for (size_t i = 0; i < this->boneStorage.size(); ++i)
    {
        this->finalMatrices[i] = blib::graphics::mul(this->boneStorage[i].globalTransform, this->boneStorage[i].offsetMatrix);
    }
}

std::vector<blib::graphics::Bone>& blib::graphics::Skelet::getBoneStorage()
{
    return this->boneStorage;
}

const std::vector<blib::graphics::Bone>& blib::graphics::Skelet::getBoneStorage() const
{
    return this->boneStorage;
}

const std::vector<blib::graphics::TransformMatrix>& blib::graphics::Skelet::getFinalMatrices() const
{
    return this->finalMatrices;
}
