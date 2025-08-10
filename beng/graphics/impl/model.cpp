#include <beng/graphics/model.h>

#include <blib/core/folder.h>

void beng::graphics::Model::parseFromAssimpScene(const aiScene* pscene, const std::string& filename)
{
    blib::core::Folder folder(filename);

    this->meshes.resize(pscene->mNumMeshes);
    for (size_t i = 0; i < this->meshes.size(); ++i)
    {
        this->meshes[i].loadFromAssimpMesh(pscene->mMeshes[i]);

        if((pscene->mNumMaterials > 0) && (pscene->mMeshes[i]->mMaterialIndex < pscene->mNumMaterials))
            this->meshes[i].material.loadFromAssimpMaterial(pscene->mMaterials[pscene->mMeshes[i]->mMaterialIndex], folder.getCurrentPath());
    }
}

void beng::graphics::Model::draw(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const
{
    ctx.applyTransform(*this);

    for (auto& mesh : this->meshes)
    {
        mesh.draw(target, ctx);
    }
}
