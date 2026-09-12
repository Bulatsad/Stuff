#pragma once

#include <vector>

#include <blib/config.h>
#include <blib/graphics/light.h>
#include <blib/graphics/opengl.h>

#include <blib/core/math/matrix.h>
#include <blib/graphics/transformable.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api ICamera;
        class __blib_graphics_api ShaderProgram;
        class __blib_graphics_api RenderContext
        {
        public:
            TransformMatrix transform;

            blib::graphics::ICamera* pCamera = nullptr; // TODO : Make it array to split wnd // REDO : Spliting by viewports

            blib::math::Matrix<float, 4, 4>vievMatrix;
            blib::math::Matrix<float, 4, 4>projectionMatrix;

            blib::graphics::ShaderProgram* lastShader;

            RenderApi api;

            RenderContext();

            // Отладка: при false меши отрисовываются плоской белой
            // заливкой вместо диффузной текстуры (для вьюверов —
            // переключатель «Diffuse Texture»). Влияет только на
            // отрисовку: материалы не модифицируются
            bool useDiffuseTextures = true;

            // Отладка: при true меши раскрашиваются по нормалям
            // (мировые нормали → RGB), альбедо игнорируется. Влияет
            // только на отрисовку (uniform gShowNormals)
            bool showNormals = false;

            // Свет сцены (NPR-пайплайн, фаза 4): дефолты — мягкий
            // тёплый направленный свет + приглушённый холодный эмбиент.
            // Отправка в шейдер — sendLightsToShaderProgram()
            blib::graphics::DirectionalLight directionalLight;
            blib::graphics::AmbientLight ambientLight;

            // Лениво создаёт и возвращает белую текстуру 1x1,
            // которой заменяется диффузная текстура при
            // useDiffuseTextures == false
            GLuint getFlatWhiteTexture() const;

            void setShaderProgram(blib::graphics::ShaderProgram* pShaderProgram);
            void sendVievMatrixToShaderProgram();
            void sendProjectionMatrixToShaderProgram();
            void sendModelMatrixToShaderProgram(const blib::graphics::TransformMatrix& modelMatix);
            void sendBoneMatricesToShaderProgram(const std::vector<blib::graphics::TransformMatrix>& boneMatrices);
            void setCamera(blib::graphics::ICamera* a_pCamera);

            // Свет сцены в текущую программу: gLightDir, gLightColor,
            // gAmbientColor (uniform'ы без них игнорируются — location -1)
            void sendLightsToShaderProgram();

            // Позиция камеры (gCameraPosition) — для rim-light/тумана
            void sendCameraPositionToShaderProgram();

            void applyTransform(const Transform& transform);

        };
    }
}
