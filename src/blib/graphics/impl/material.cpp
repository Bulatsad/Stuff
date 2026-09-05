#include <blib/graphics/material.h>

#include <blib/core/console/console.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

blib::graphics::MaterialError blib::graphics::Material::loadDiffuseTextureFromAssimp(const aiMaterial* pmaterial, const blib::core::Folder& folder)
{
    aiString path(folder.getCurrentPath());

    // BUG-FIX: раньше стояло `... != 0`, что превращало количество
    // текстур в bool и проверка `> 1` никогда не срабатывала
    unsigned int textureCount = pmaterial->GetTextureCount(aiTextureType::aiTextureType_DIFFUSE);

    if (__blib_unlikely(textureCount > 1))
    {
        __blib_return_error(blib::graphics::MaterialError::NotImplemented, "multiple diffuse textures are not implemented (%u found)", textureCount);
    }

    if (__blib_likely(textureCount == 1))
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
                if (__blib_unlikely(!pPixelData))
                {
                    __blib_return_error(blib::graphics::MaterialError::TextureLoadFailed, "stbi_load failed: %s", stbi_failure_reason());
                }

                switch (channels)
                {
                case 3:
                    this->diffuseImage = blib::graphics::Image(width, height);
                    for (int i = 0; i < width; ++i)
                    {
                        for (int j = 0; j < height; ++j)
                        {
                            this->diffuseImage[i][j].red = pPixelData[(i * height + j) * 3 + 0];
                            this->diffuseImage[i][j].green = pPixelData[(i * height + j) * 3 + 1];
                            this->diffuseImage[i][j].blue = pPixelData[(i * height + j) * 3 + 2];
                            this->diffuseImage[i][j].alpha = 0;
                        }
                    }
                    break;
                case 4:
                    this->diffuseImage = blib::graphics::Image(width, height, reinterpret_cast<blib::graphics::Color*>(pPixelData));
                    break;
                default:
                    stbi_image_free(pPixelData);
                    __blib_return_error(blib::graphics::MaterialError::UnsupportedFormat, "unsupported texture channel count: %d", channels);
                }

                stbi_image_free(pPixelData);
            }
            else
            {
                __blib_return_error(blib::graphics::MaterialError::TextureLoadFailed, "can not resolve texture path '%s'", texturePath.c_str());
            }
        }
        else
        {
            __blib_return_error(blib::graphics::MaterialError::TextureLoadFailed, "assimp failed to get diffuse texture");
        }
    }

    return blib::graphics::MaterialError::None;
}

bool blib::graphics::Material::bake(blib::graphics::RenderContext& ctx)
{
    blib::graphics::TextureError err = this->diffuse.create(this->diffuseImage, ctx);
    if (__blib_unlikely(err != blib::graphics::TextureError::None))
    {
        __blib_log_warning("material bake: failed to create diffuse texture (error %u)", static_cast<buint32>(err));
        return false;
    }
    return true;
}

void blib::graphics::Material::loadFromAssimpMaterial(const aiMaterial* pmaterial, const blib::core::Folder& folder)
{
    // Детали ошибки уже залогированы внутри loadDiffuseTextureFromAssimp
    // через __blib_return_error, дублировать тут незачем
    this->loadDiffuseTextureFromAssimp(pmaterial, folder);
}
