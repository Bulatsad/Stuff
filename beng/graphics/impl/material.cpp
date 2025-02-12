#include <beng/graphics/material.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

int beng::graphics::Material::loadDiffuseTextureFromAssimp(const aiMaterial* pmaterial, const blib::core::Folder& folder)
{
    aiString path(folder.getCurrentPath());

    unsigned int textureCount = pmaterial->GetTextureCount(aiTextureType::aiTextureType_DIFFUSE) != 0;

    if (textureCount > 1)
    {
        throw new std::exception("not implemented");
    }

    //for (decltype(textureCount) i = 0; i < textureCount; +i)
    {
        if (pmaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path) == aiReturn_SUCCESS)
        {
            std::string texturePath(path.data);
            blib::core::Folder tmpfolder(folder.getCurrentPath());
            if (tmpfolder.down(texturePath))
            {
                int width;
                int height;
                int channels;

                stbi_set_flip_vertically_on_load(1);
                stbi_uc* pPixelData = stbi_load(tmpfolder.getCurrentPath().c_str(), &width, &height, &channels, 0);
                if (!pPixelData)
                {
                    throw new std::exception(stbi_failure_reason());
                }

                this->diffuse.create(reinterpret_cast<void*>(pPixelData), width, height, channels);
            }
            else
            {
                throw new std::exception("error texture");
            }
        }
        else
        {
            throw new std::exception("error texture");

        }

    }

}

void beng::graphics::Material::loadFromAssimpMaterial(const aiMaterial* pmaterial, const blib::core::Folder& folder)
{
    this->loadDiffuseTextureFromAssimp(pmaterial, folder);
}
