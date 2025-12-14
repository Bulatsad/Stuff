#pragma once

#include <vector>

#include <assimp/scene.h>

#include <blib/blibint.h>
#include <blib/graphics/vertex.h>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/drawable.h>
#include <blib/graphics/transformable.h>
#include <blib/graphics/shader.h>
#include <blib/graphics/material.h>

#include <beng/config.h>
#include <beng/graphics/face.h>


namespace blib
{
    namespace graphics
    {
        enum class PrimitiveType : buint8
        {
            Unknown = 0x00,
            Point = 0x1,
            Line = 0x2,
            Triangle = 0x4,
            Polygon = 0x8,
            TriangleStrip = 0x10
        };

        class __blib_api Mesh : public blib::graphics::IDrawable, public blib::graphics::ITransformable
        {
        private:
            void* ctx;
            mutable bool baked = false;

            mutable blib::graphics::Shader fragmentShader;
            mutable blib::graphics::Shader vertexShader;
            mutable blib::graphics::ShaderProgram drawer;
            void bake(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const;
        public:
            Mesh();
            bool ngonencoding = false;

            PrimitiveType primitiveType;
            std::vector<blib::graphics::Vector3f>vertices;
            std::vector<blib::graphics::Vector3f>normals;
            std::vector<blib::graphics::Vector3f>textureCoords;
            std::vector<blib::graphics::Color>colors;
            std::vector<beng::graphics::Face>faces;
            mutable blib::graphics::Material material;

            void loadFromAssimpMesh(const aiMesh* paimesh);

            // Унаследовано через IDrawable
            virtual void draw(blib::graphics::RenderTarget& target, blib::graphics::RenderContext& ctx) const override;
        };
    }
}
