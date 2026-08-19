#include <blib/graphics/skinmodel.h>

#include <blib/core/folder.h>

bool blib::graphics::SkinModel::loadFromAssimp(const aiScene* paiscene, const std::string& filename, const aiScene* panimationScene)
{
    if (!(this->skelet.loadFromAssimp(paiscene)))
    {
        return false;
    }

    if (!(this->animator.loadFromAssimp(panimationScene ? panimationScene : paiscene)))
    {
        return false;
    }

    this->meshes.resize(paiscene->mNumMeshes);
    for (size_t i = 0; i < this->meshes.size(); ++i)
    {
        if (!(this->meshes[i].loadFromAssimpMesh(paiscene->mMeshes[i], this->skelet)))
        {
            return false;
        }

        if (!(filename.empty()) && (paiscene->mNumMaterials > 0) && (paiscene->mMeshes[i]->mMaterialIndex < paiscene->mNumMaterials))
        {
            blib::core::Folder folder(filename);
            this->meshes[i].mesh.material.loadFromAssimpMaterial(paiscene->mMaterials[paiscene->mMeshes[i]->mMaterialIndex], folder);
        }
    }

    return true;
}

void blib::graphics::SkinModel::update(float deltaTimeMs)
{
    this->animator.update(deltaTimeMs);

    const blib::graphics::AnimationClip* pclip = this->animator.getCurrentAnimation();
    if (!pclip || pclip->tickPerSecond <= 0.0)
    {
        return;
    }

    double timeTicks = (this->animator.getCurrentTimeMs() / 1000.0) * pclip->tickPerSecond;
    this->skelet.applyClip(*pclip, timeTicks);
}

bool blib::graphics::SkinModel::selectAnimation(const std::string& animationName)
{
    return this->animator.selectAnimation(animationName);
}

bool blib::graphics::SkinModel::playAnimation()
{
    return this->animator.play();
}

blib::graphics::Skelet& blib::graphics::SkinModel::getSkelet()
{
    return this->skelet;
}

const blib::graphics::Skelet& blib::graphics::SkinModel::getSkelet() const
{
    return this->skelet;
}

blib::graphics::Animator& blib::graphics::SkinModel::getAnimator()
{
    return this->animator;
}

const blib::graphics::Animator& blib::graphics::SkinModel::getAnimator() const
{
    return this->animator;
}

void blib::graphics::SkinModel::draw(blib::graphics::RenderContext& ctx) const
{
    const std::vector<blib::graphics::TransformMatrix>& finalMatrices = this->skelet.getFinalMatrices();
    const blib::graphics::TransformMatrix transform = this->getTransform();

    for (const blib::graphics::SkinMesh& skinMesh : this->meshes)
    {
        const blib::graphics::TransformMatrix meshTransformCopy = skinMesh.mesh.getTransform();
        const blib::graphics::TransformMatrix meshTransform = transform * meshTransformCopy;
        skinMesh.mesh.setTransform(meshTransform);
        skinMesh.mesh.draw(ctx, &finalMatrices);
        skinMesh.mesh.setTransform(meshTransformCopy);
    }
}
