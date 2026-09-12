#include <blib/graphics/material.h>

#include <blib/core/console/console.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace
{
    // Размер синтезируемой 1x1 текстуры плоского цвета (см. bake)
    constexpr buint16 flatColorTextureDimension = 1;
    // Диапазон и округление при переводе компонента цвета 0..1 в байт
    constexpr float colorByteRange = 255.0f;
    constexpr float colorByteRounding = 0.5f;

    // Компонент цвета (0..1) в байт (0..255) с клампом и округлением
    buint8 colorComponentToByte(float component)
    {
        const float clamped = component < 0.0f ? 0.0f : (component > 1.0f ? 1.0f : component);
        return static_cast<buint8>(clamped * colorByteRange + colorByteRounding);
    }
}

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
                    // stbi отдаёт пиксели построчно (row-major):
                    // индекс = (row * width + col). Раньше стояла
                    // транспонированная индексация (i * height + j) —
                    // для неквадратных текстур она читала за границей
                    // буфера, для квадратных — зеркалила картинку.
                    // Alpha не используется (Image всегда RGBA), ставим
                    // непрозрачность, а не 0
                    for (int j = 0; j < height; ++j)
                    {
                        for (int i = 0; i < width; ++i)
                        {
                            const int srcIndex = (j * width + i) * 3;
                            this->diffuseImage[i][j].red = pPixelData[srcIndex + 0];
                            this->diffuseImage[i][j].green = pPixelData[srcIndex + 1];
                            this->diffuseImage[i][j].blue = pPixelData[srcIndex + 2];
                            this->diffuseImage[i][j].alpha = 255;
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
    // Запоминаем контекст рендера: он понадобится деструктору для
    // возврата GL-текстуры через Texture::free
    this->pRenderContext = &ctx;

    // Пустое изображение (нет диффузной текстуры у материала):
    //  - если у материала есть диффузный цвет — синтезируем 1x1
    //    текстуру этого цвета, чтобы меш рисовался цветом из файла
    //    вместо плоской белой заглушки;
    //  - иначе GL-текстуру не создаём: textureID остаётся 0, и
    //    Mesh::draw подставит плоскую белую заглушку. Раньше
    //    glTexImage2D с размерами 0x0 создавал «пустую» текстуру,
    //    сэмплинг которой давал чёрный цвет
    if (this->diffuseImage.width == 0 || this->diffuseImage.height == 0)
    {
        if (!(this->hasDiffuseColor))
        {
            return false;
        }

        this->diffuseImage.create(
            flatColorTextureDimension,
            flatColorTextureDimension,
            blib::graphics::Color(
                colorComponentToByte(this->DiffuseColor.x),
                colorComponentToByte(this->DiffuseColor.y),
                colorComponentToByte(this->DiffuseColor.z),
                colorComponentToByte(this->DiffuseColor.w)));
    }

    blib::graphics::TextureError err = this->diffuse.create(this->diffuseImage, ctx);
    if (__blib_unlikely(err != blib::graphics::TextureError::None))
    {
        __blib_log_warning("material bake: failed to create diffuse texture (error %u)", static_cast<buint32>(err));
        return false;
    }
    return true;
}

blib::graphics::Material::~Material()
{
    // Возврат GL-текстуры диффуза, если она была создана в bake.
    // Материалы разрушаются вместе с моделью (или сцены раньше
    // рендер-таргета), поэтому контекст рендера к этому моменту жив
    // и GL-контекст текущий. Ограничение: материал не должен
    // пережить владеющий контекстом RenderTarget
    if (this->pRenderContext && this->diffuse.getContext().textureID != 0)
    {
        this->diffuse.free(*this->pRenderContext);
    }
}

void blib::graphics::Material::loadFromAssimpMaterial(const aiMaterial* pmaterial, const blib::core::Folder& folder)
{
    // Диффузный цвет (RGB) — fallback для материалов без текстуры:
    // bake() синтезирует из него 1x1 текстуру
    aiColor4D diffuseColor;
    if (pmaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == aiReturn_SUCCESS)
    {
        this->DiffuseColor = blib::graphics::Vector4f(
            diffuseColor.r, diffuseColor.g, diffuseColor.b, diffuseColor.a);
        this->hasDiffuseColor = true;
    }

    // Детали ошибки уже залогированы внутри loadDiffuseTextureFromAssimp
    // через __blib_return_error, дублировать тут незачем
    this->loadDiffuseTextureFromAssimp(pmaterial, folder);
}
