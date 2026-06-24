#include <blib/graphics/material.h>

#include<stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

int blib::graphics::Material::loadDiffuseTextureFromAssimp(const aiMaterial* pmaterial, const blib::core::Folder& folder)
{
    aiString path(folder.getCurrentPath());

    unsigned int textureCount = pmaterial->GetTextureCount(aiTextureType::aiTextureType_DIFFUSE) != 0;

    if (textureCount > 1)
    {
        throw new std::runtime_error("not implemented");
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
                    throw new std::runtime_error(stbi_failure_reason());
                }

                //this->diffuse.create(reinterpret_cast<void*>(pPixelData), width, height, channels);
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
                    //ctx.api.ogl.ext.__blib_gl_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, this->width, this->height, 0, GL_RGB, GL_UNSIGNED_BYTE, pdata);
                    break;
                case 4:
                    this->diffuseImage = blib::graphics::Image(width, height, reinterpret_cast<blib::graphics::Color*>(pPixelData));
                    //ctx.api.ogl.ext.__blib_gl_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pdata);
                    break;
                default:
                    throw new std::runtime_error("not implemented");
                    break;
                }
                


                stbi_image_free(pPixelData);
            }
            else
            {
                throw new std::runtime_error("error texture");
            }
        }
        else
        {
            throw new std::runtime_error("error texture");

        }

    }

}

bool blib::graphics::Material::bake(blib::graphics::RenderContext& ctx)
{
    this->diffuse.create(this->diffuseImage, ctx);
    return true;
}

void blib::graphics::Material::loadFromAssimpMaterial(const aiMaterial* pmaterial, const blib::core::Folder& folder)
{
    this->loadDiffuseTextureFromAssimp(pmaterial, folder);
}
