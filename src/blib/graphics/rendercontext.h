#pragma once

#include <vector>

#include <blib/config.h>
#include <blib/graphics/opengl.h>

#include <blib/core/math/matrix.h>
#include <blib/graphics/transformable.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Camera;
        class __blib_graphics_api ShaderProgram;
        class __blib_graphics_api RenderContext
        {
        public:
            TransformMatrix transform;

            blib::graphics::Camera* pCamera = nullptr; // TODO : Make it array to split wnd // REDO : Spliting by viewports

            blib::math::Matrix<float, 4, 4>vievMatrix;
            blib::math::Matrix<float, 4, 4>projectionMatrix;

            blib::graphics::ShaderProgram* lastShader;

            RenderApi api;

            RenderContext();

            void setShaderProgram(blib::graphics::ShaderProgram* pShaderProgram);
            void sendVievMatrixToShaderProgram();
            void sendProjectionMatrixToShaderProgram();
            void sendModelMatrixToShaderProgram(const blib::graphics::TransformMatrix& modelMatix);
            void sendBoneMatricesToShaderProgram(const std::vector<blib::graphics::TransformMatrix>& boneMatrices);
            void setCamera(blib::graphics::Camera* a_pCamera);

            void applyTransform(const Transform& transform);

        };
    }
}
