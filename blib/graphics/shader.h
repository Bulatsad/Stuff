#pragma once

#include <string>

#include <blib/config.h>
#include <beng/graphics/material.h>
#include <blib/graphics/opengl.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Shader;
        class __blib_api ShaderProgram;

        class __blib_api Shader
        {
        private:
            void* ctx;
            std::string shaderPath;
        public:
            friend class __blib_api ShaderProgram;

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
            int compile();
        };

        class __blib_api ShaderProgram
        {
            void* ctx;
        public:
            ShaderProgram();
            void AttachShader(Shader& shader);
            int create();
            int compile();
            GLuint getContext();
            void setRenderApi(RenderApi* pRenderApi);
            void use();
            void unuse();
        };

    }
}
