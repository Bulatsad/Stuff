#pragma once

#include <blib/config.h>
#include <blib/graphics/opengl.h>

#include <blib/math/matrix.h>
#include <blib/graphics/transformable.h>
#include <blib/graphics/shader.h>

namespace blib
{
    namespace graphics
    {
        class __blib_api Camera;
        class __blib_api RenderContext
        {
        public:
            Transform transform;

            const blib::graphics::Camera* pCamera = nullptr; // TODO : Make it array to split wnd 

            blib::math::Matrix<float, 4, 4>vievMatrix;
            blib::math::Matrix<float, 4, 4>projectionMatrix;

            blib::graphics::ShaderProgram* lastShader;

            RenderApi api;

            RenderContext();

            void setShaderProgram(blib::graphics::ShaderProgram* pShaderProgram);
            void sendVievMatrixToShaderProgram();
            void sendProjectionMatrixToShaderProgram();
            void setCamera(const blib::graphics::Camera* a_pCamera);

            void applyTransform(const Transformable& transform);

        };
    }
}
