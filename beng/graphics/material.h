#pragma once

#include <string>

#include <assimp/material.h>

#include <beng/config.h>

#include <blib/core/folder.h>
#include <blib/graphics/vector.h>
#include <blib/graphics/texture.h>

namespace beng
{
    namespace graphics
    {
        class __beng_api Material {

        public:

            std::string m_name;

            blib::graphics::Vector4f AmbientColor = blib::graphics::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            blib::graphics::Vector4f DiffuseColor = blib::graphics::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            blib::graphics::Vector4f SpecularColor = blib::graphics::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);

            //PBRMaterial PBRmaterial;

            blib::graphics::Texture diffuse; // base color of the material
            blib::graphics::Texture pSpecularExponent;

            float m_transparencyFactor = 1.0f;
            float m_alphaTest = 0.0f;

            void loadFromAssimpMaterial(const aiMaterial* pmaterial, const blib::core::Folder& folder);
            int loadDiffuseTextureFromAssimp(const aiMaterial* pmaterial, const blib::core::Folder& folder);

            ~Material()
            {
            }

        private:

        };
    }
}
