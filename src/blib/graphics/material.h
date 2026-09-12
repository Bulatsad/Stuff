#pragma once

#include <string>

#include <assimp/material.h>

#include <blib/config.h>

#include <blib/core/folder.h>
#include <blib/graphics/vector.h>
#include <blib/graphics/texture.h>
#include <blib/graphics/image.h>
#include <blib/graphics/rendercontext.h>

namespace blib
{
    namespace graphics
    {
        // Коды ошибок загрузки материалов (см. AGENTS.md,
        // раздел "Обработка ошибок": None = 0 — всегда успех)
        enum class MaterialError : buint32
        {
            None = 0,
            TextureLoadFailed,
            UnsupportedFormat,
            NotImplemented
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

            void loadFromAssimpMaterial(const aiMaterial* pmaterial, const blib::core::Folder& folder);
            MaterialError loadDiffuseTextureFromAssimp(const aiMaterial* pmaterial, const blib::core::Folder& folder);
            bool bake(blib::graphics::RenderContext& ctx);

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
