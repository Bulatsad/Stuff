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

blib::graphics::Mesh beng::graphics::Model::bakeMeshes(const std::vector<blib::graphics::Mesh>& meshes)
{
    blib::graphics::Mesh res;
    size_t baseIndex = 0;
    for (size_t iMesh = 0; iMesh < meshes.size(); ++iMesh)
    {
        const blib::graphics::TransformMatrix& currentTransform = meshes[iMesh].getTransform();
        for (size_t i = 0; i < meshes[iMesh].vertices.size(); ++i)
        {
            res.vertices.push_back(meshes[iMesh].transform(meshes[iMesh].vertices[i]));
            if(meshes[iMesh].colors.size() != 0)
                res.colors.push_back(meshes[iMesh].colors[i]);
            if (meshes[iMesh].textureCoords.size() != 0)
                res.textureCoords.push_back(meshes[iMesh].textureCoords[i]);
            if (meshes[iMesh].normals.size() != 0)
                res.normals.push_back(meshes[iMesh].normals[i]);
        }

        for (size_t i = 0; i < meshes[iMesh].faces.size(); ++i)
        {
            res.faces.push_back(meshes[iMesh].faces[i]);
            for (size_t j = 0; j < res.faces.back().indices.size(); ++j)
            {
                res.faces.back().indices[j] += baseIndex;
            }
        }
        baseIndex += meshes[iMesh].vertices.size();
    }

    res.primitiveType = meshes[0].primitiveType;

    return res;
}

void beng::graphics::Model::draw(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const
{
    const blib::graphics::TransformMatrix transform = this->getTransform();
    for (auto& mesh : this->meshes)
    {
        blib::graphics::TransformMatrix meshTransformCopy = mesh.getTransform();
        blib::graphics::TransformMatrix meshTransform = transform * meshTransformCopy;
        mesh.setTransform(meshTransform);
        mesh.draw(target, ctx);
        mesh.setTransform(meshTransformCopy);
    }
    //this->meshes[0].draw(target, ctx);
}
