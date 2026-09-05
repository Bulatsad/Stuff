#pragma once

#include <string>

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/graphics/material.h>
#include <blib/graphics/opengl.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Shader;
        class __blib_graphics_api ShaderProgram;

        // Коды ошибок компиляции/линковки шейдеров (см. AGENTS.md,
        // раздел "Обработка ошибок": None = 0 — всегда успех)
        enum class ShaderError : buint32
        {
            None = 0,
            InvalidType,
            FileNotFound,
            CompilationFailed,
            LinkFailed
        };

        class __blib_graphics_api Shader
        {
        private:
            void* ctx;
            std::string shaderPath;
        public:
            friend class __blib_graphics_api ShaderProgram;

            enum class Type
            {
                None,
                vertex,
                fragment
            }type;

            Shader();
            Shader(const std::string& path);
            void setPath(const std::string& path);
            void setType(const Type type);
            void setRenderApi(RenderApi* pRenderApi);
            ShaderError compile();
        };

        class __blib_graphics_api ShaderProgram
        {
            void* ctx;
        public:
            ShaderProgram();
            void AttachShader(Shader& shader);
            int create();
            ShaderError compile();
            GLuint getContext();
            void setRenderApi(RenderApi* pRenderApi);
            void use();
            void unuse();
        };

    }
}
