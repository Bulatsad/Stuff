#include <blib/graphics/material.h>

#include <blib/core/console/console.h>
#include <blib/graphics/shader.h>

#include <assimp/scene.h>

#include <cctype>
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace
{
    // Размер синтезируемой 1x1 текстуры плоского цвета (см. bake)
    constexpr buint16 flatColorTextureDimension = 1;
    // Диапазон и округление при переводе компонента цвета 0..1 в байт
    constexpr float colorByteRange = 255.0f;
    constexpr float colorByteRounding = 0.5f;
    // Непрозрачность для текстур без альфа-канала (Image всегда RGBA)
    constexpr buint8 opaqueAlphaComponent = 255;
    // Признак встроенной текстуры Assimp: путь вида "*<индекс>"
    constexpr char embeddedTexturePrefix = '*';
    // Разделители пути (Windows и POSIX)
    constexpr const char* pathSeparatorChars = "\\/";

    // Компонент цвета (0..1) в байт (0..255) с клампом и округлением
    buint8 colorComponentToByte(float component)
    {
        const float clamped = component < 0.0f ? 0.0f : (component > 1.0f ? 1.0f : component);
        return static_cast<buint8>(clamped * colorByteRange + colorByteRounding);
    }

    // Имя файла без пути (для сопоставления встроенных текстур)
    std::string extractFileName(const std::string& path)
    {
        const size_t pos = path.find_last_of(pathSeparatorChars);
        return (pos == std::string::npos) ? path : path.substr(pos + 1);
    }

    // Сравнение имён файлов без учёта регистра
    bool fileNamesEqual(const std::string& lhs, const std::string& rhs)
    {
        if (lhs.size() != rhs.size())
        {
            return false;
        }

        for (size_t i = 0; i < lhs.size(); ++i)
        {
            const char l = static_cast<char>(std::tolower(static_cast<unsigned char>(lhs[i])));
            const char r = static_cast<char>(std::tolower(static_cast<unsigned char>(rhs[i])));
            if (l != r)
            {
                return false;
            }
        }

        return true;
    }

    // Встроенная текстура, на которую ссылается материал:
    //  - "*<индекс>" — прямая ссылка на aiScene::mTextures;
    //  - иначе (материал ссылается на внешний файл, которого нет рядом,
    //    но текстура встроена в модель) — поиск по имени файла
    const aiTexture* findEmbeddedTexture(const aiScene* scene, const std::string& texturePath)
    {
        if (!scene || texturePath.empty())
        {
            return nullptr;
        }

        if (texturePath[0] == embeddedTexturePrefix)
        {
            const size_t index = static_cast<size_t>(std::atoi(texturePath.c_str() + 1));
            return (index < scene->mNumTextures) ? scene->mTextures[index] : nullptr;
        }

        const std::string requestedName = extractFileName(texturePath);
        for (unsigned int i = 0; i < scene->mNumTextures; ++i)
        {
            if (fileNamesEqual(extractFileName(scene->mTextures[i]->mFilename.C_Str()), requestedName))
            {
                return scene->mTextures[i];
            }
        }

        return nullptr;
    }

    // Пиксели stbi (row-major, 3 или 4 канала) в Image (RGBA)
    blib::graphics::MaterialError loadImageFromPixels(
        _In const stbi_uc* pixels,
        int width,
        int height,
        int channels,
        _Out blib::graphics::Image& outImage)
    {
        switch (channels)
        {
        case 3:
            outImage = blib::graphics::Image(width, height);
            // stbi отдаёт пиксели построчно (row-major):
            // индекс = (row * width + col). Alpha не используется
            // (Image всегда RGBA), ставим непрозрачность
            for (int j = 0; j < height; ++j)
            {
                for (int i = 0; i < width; ++i)
                {
                    const int srcIndex = (j * width + i) * 3;
                    outImage[i][j].red = pixels[srcIndex + 0];
                    outImage[i][j].green = pixels[srcIndex + 1];
                    outImage[i][j].blue = pixels[srcIndex + 2];
                    outImage[i][j].alpha = opaqueAlphaComponent;
                }
            }
            return blib::graphics::MaterialError::None;
        case 4:
            outImage = blib::graphics::Image(width, height, reinterpret_cast<const blib::graphics::Color*>(pixels));
            return blib::graphics::MaterialError::None;
        default:
            return blib::graphics::MaterialError::UnsupportedFormat;
        }
    }

    // Декодирование встроенной текстуры Assimp: сжатая (mHeight == 0,
    // mWidth — размер в байтах) через stbi_load_from_memory, несжатая —
    // копированием aiTexel (порядок каналов BGRA)
    blib::graphics::MaterialError loadEmbeddedTexture(
        _In const aiTexture& texture,
        _Out blib::graphics::Image& outImage)
    {
        if (texture.mHeight == 0)
        {
            int width = 0;
            int height = 0;
            int channels = 0;

            stbi_set_flip_vertically_on_load(1);
            stbi_uc* pPixelData = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(texture.pcData),
                static_cast<int>(texture.mWidth),
                &width,
                &height,
                &channels,
                0);
            if (__blib_unlikely(!pPixelData))
            {
                return blib::graphics::MaterialError::TextureLoadFailed;
            }

            const blib::graphics::MaterialError err =
                loadImageFromPixels(pPixelData, width, height, channels, outImage);
            stbi_image_free(pPixelData);
            return err;
        }

        if (__blib_unlikely(texture.mWidth == 0 || !texture.pcData))
        {
            return blib::graphics::MaterialError::UnsupportedFormat;
        }

        outImage = blib::graphics::Image(texture.mWidth, texture.mHeight);
        for (unsigned int j = 0; j < texture.mHeight; ++j)
        {
            for (unsigned int i = 0; i < texture.mWidth; ++i)
            {
                const aiTexel& texel = texture.pcData[j * texture.mWidth + i];
                outImage[i][j].red = texel.r;
                outImage[i][j].green = texel.g;
                outImage[i][j].blue = texel.b;
                outImage[i][j].alpha = texel.a;
            }
        }

        return blib::graphics::MaterialError::None;
    }
}

blib::graphics::MaterialError blib::graphics::Material::loadDiffuseTextureFromAssimp(const aiMaterial* pmaterial, const blib::core::Folder& folder, const aiScene* scene)
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
            const std::string texturePath(path.data);

            // Встроенная текстура ("*<индекс>") — декодируем из памяти
            // модели, не обращаясь к файловой системе
            if (!texturePath.empty() && texturePath[0] == embeddedTexturePrefix)
            {
                const aiTexture* embedded = findEmbeddedTexture(scene, texturePath);
                if (__blib_unlikely(!embedded))
                {
                    __blib_return_error(blib::graphics::MaterialError::TextureLoadFailed,
                        "embedded texture '%s' not found in scene", texturePath.c_str());
                }

                const blib::graphics::MaterialError err = loadEmbeddedTexture(*embedded, this->diffuseImage);
                if (__blib_unlikely(err != blib::graphics::MaterialError::None))
                {
                    __blib_return_error(err, "failed to decode embedded texture '%s'", texturePath.c_str());
                }

                return blib::graphics::MaterialError::None;
            }

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

                const blib::graphics::MaterialError err =
                    loadImageFromPixels(pPixelData, width, height, channels, this->diffuseImage);
                stbi_image_free(pPixelData);
                if (__blib_unlikely(err != blib::graphics::MaterialError::None))
                {
                    __blib_return_error(err, "unsupported texture channel count: %d", channels);
                }
            }
            else
            {
                // Файла рядом нет — возможно, текстура встроена в модель
                // (например, Mixamo FBX): ищем встроенную по имени файла
                const aiTexture* embedded = findEmbeddedTexture(scene, texturePath);
                if (embedded)
                {
                    const blib::graphics::MaterialError err = loadEmbeddedTexture(*embedded, this->diffuseImage);
                    if (__blib_unlikely(err != blib::graphics::MaterialError::None))
                    {
                        __blib_return_error(err, "failed to decode embedded texture '%s'", texturePath.c_str());
                    }

                    return blib::graphics::MaterialError::None;
                }

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

void blib::graphics::Material::apply(blib::graphics::RenderContext& ctx, blib::graphics::ShaderProgram& program) const
{
    // Диффузная текстура (или плоская белая заглушка при выключенных
    // текстурах / отсутствии текстуры) на TEXTURE0 + sampler-uniform
    ctx.api.ogl.ext.__blib_gl_glActiveTexture(GL_TEXTURE0);

    GLuint boundTexture = 0;
    if (!ctx.useDiffuseTextures)
    {
        boundTexture = ctx.getFlatWhiteTexture();
    }
    else
    {
        boundTexture = this->diffuse.getContext().textureID;
        if (boundTexture == 0)
        {
            boundTexture = ctx.getFlatWhiteTexture();
        }
    }

    ctx.api.ogl.ext.__blib_gl_glBindTexture(GL_TEXTURE_2D, boundTexture);
    const GLint samplerLocation = program.getUniformLocation("textureSampler");
    ctx.api.ogl.ext.__blib_gl_glUniform1i(samplerLocation, 0);

    // NPR-униформы (фаза 4): шейдеры без них (line-шейдеры и т.п.)
    // просто игнорируются — location == -1
    const GLint shadingModeLocation = program.getUniformLocation("gShadingMode");
    if (shadingModeLocation != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1i(shadingModeLocation, static_cast<GLint>(this->shadingMode));
    }

    const GLint rampSoftnessLocation = program.getUniformLocation("gRampSoftness");
    if (rampSoftnessLocation != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(rampSoftnessLocation, this->rampSoftness);
    }

    const GLint rimColorLocation = program.getUniformLocation("gRimColor");
    if (rimColorLocation != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform3f(rimColorLocation, this->rimColor.x, this->rimColor.y, this->rimColor.z);
    }

    const GLint rimPowerLocation = program.getUniformLocation("gRimPower");
    if (rimPowerLocation != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(rimPowerLocation, this->rimPower);
    }

    const GLint emissionLocation = program.getUniformLocation("gEmission");
    if (emissionLocation != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform3f(emissionLocation, this->emission.x, this->emission.y, this->emission.z);
    }

    const GLint alphaTestLocation = program.getUniformLocation("gAlphaTest");
    if (alphaTestLocation != -1)
    {
        ctx.api.ogl.ext.__blib_gl_glUniform1f(alphaTestLocation, this->m_alphaTest);
    }
}

void blib::graphics::Material::loadFromAssimpMaterial(const aiMaterial* pmaterial, const blib::core::Folder& folder, const aiScene* scene)
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
    this->loadDiffuseTextureFromAssimp(pmaterial, folder, scene);
}
