#pragma once

#include <string>

#include <assimp/material.h>

#include <blib/config.h>
#include <blib/utilmacro.h>

#include <blib/core/folder.h>
#include <blib/graphics/vector.h>
#include <blib/graphics/texture.h>
#include <blib/graphics/image.h>
#include <blib/graphics/rendercontext.h>

struct aiScene;

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api ShaderProgram;
        // Коды ошибок загрузки материалов (см. AGENTS.md,
        // раздел "Обработка ошибок": None = 0 — всегда успех)
        enum class MaterialError : buint32
        {
            None = 0,
            TextureLoadFailed,
            UnsupportedFormat,
            NotImplemented
        };

        // Режим затенения материала (NPR-пайплайн, фаза 4):
        // Unlit — чистый альбедо-сэмплинг (тайлы, фоны);
        // Toon — мягкий ramp-свет + rim + emission
        enum class ShadingMode : buint8
        {
            Unlit = 0,
            Toon = 1
        };

        class __blib_graphics_api Material 
        {
        public:

            std::string m_name;

            blib::graphics::Vector4f AmbientColor = blib::graphics::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            blib::graphics::Vector4f DiffuseColor = blib::graphics::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            blib::graphics::Vector4f SpecularColor = blib::graphics::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);

            // Диффузный цвет загружен из материала (AI_MATKEY_COLOR_DIFFUSE):
            // при отсутствии текстуры bake() синтезирует из него 1x1 текстуру
            bool hasDiffuseColor = false;

            //PBRMaterial PBRmaterial;

            blib::graphics::Texture diffuse; // base color of the material
            blib::graphics::Texture pSpecularExponent;

            blib::graphics::Image diffuseImage; // base color of the material

            float m_transparencyFactor = 1.0f;
            float m_alphaTest = 0.0f;

            // -----------------------------------------------------------
            // NPR-параметры (фаза 4): отправляются в шейдер в apply().
            // Цвета — линейный RGB [0, 1], как у света (см. light.h)
            // -----------------------------------------------------------

            // Режим затенения: Unlit — без света, Toon — ramp-шейдинг
            blib::graphics::ShadingMode shadingMode = blib::graphics::ShadingMode::Unlit;

            // Мягкость ступени ramp'а: полный диапазон [0, 1] вокруг
            // NdotL = 0.5. 0.5 — вырождение в линейный градиент,
            // 0.05 — почти жёсткий cel-переход
            float rampSoftness = 0.35f;

            // Контровый свет по силуэту: цвет и степень (больше —
            // уже и ярче кайма)
            blib::graphics::Vector3f rimColor = blib::graphics::Vector3f(0.4f, 0.35f, 0.3f);
            float rimPower = 2.5f;

            // Собственное свечение (не зависит от света)
            blib::graphics::Vector3f emission = blib::graphics::Vector3f(0.0f, 0.0f, 0.0f);

            // -----------------------------------------------------------
            // Контур (inverted hull, фаза 8): второй проход рисует
            // раздутые задние грани плоским цветом (см. Mesh::draw)
            // -----------------------------------------------------------

            // Контур включён (по умолчанию выключен — статика/фоны
            // обводятся рисунком в текстуре, а не геометрией)
            bool outlineEnabled = false;

            // Толщина контура в мировых единицах (расширение вдоль
            // нормали в OutlineVertexShader)
            float outlineWidth = 1.0f;

            // Цвет контура (линейный RGB [0, 1]); дефолт — почти чёрный
            blib::graphics::Vector3f outlineColor = blib::graphics::Vector3f(0.05f, 0.05f, 0.06f);

            void loadFromAssimpMaterial(_In const aiMaterial* pmaterial, _In const blib::core::Folder& folder, _In_opt const aiScene* scene);
            MaterialError loadDiffuseTextureFromAssimp(_In const aiMaterial* pmaterial, _In const blib::core::Folder& folder, _In_opt const aiScene* scene);
            bool bake(blib::graphics::RenderContext& ctx);

            // Бинд текстур и отправка material-униформ в текущую программу
            // (sampler-локации — из кеша ShaderProgram). Вызывается из
            // Mesh::draw после установки программы. Unlit-униформы
            // (rim/ramp) уходят, только если программа их имеет
            void apply(_In blib::graphics::RenderContext& ctx, _In blib::graphics::ShaderProgram& program) const;

            // Возвращает GL-текстуру диффуза, если она была создана
            // (см. реализацию в material.cpp: контекст рендера обязан
            // быть жив на момент уничтожения материала)
            ~Material();

        private:
            // Контекст рендера, которым была создана GL-текстура
            // диффуза (запоминается в bake). Нужен деструктору для
            // возврата ресурса через Texture::free
            blib::graphics::RenderContext* pRenderContext = nullptr;
        };
    }
}
