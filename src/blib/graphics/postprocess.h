#pragma once

#include <blib/config.h>
#include <blib/utilmacro.h>

#include <blib/graphics/rendercontext.h>
#include <blib/graphics/shader.h>
#include <blib/graphics/texture.h>
#include <blib/graphics/vector.h>

namespace blib
{
    namespace graphics
    {
        // ---------------------------------------------------------------
        // PostProcessSettings — параметры пост-процессинга (фаза 6,
        // NPR-пайплайн, «хейдсовская» картинка):
        //
        //   fogColor/vignetteStrength — тёмная дымка по глубине и
        //       затемнение углов (главные приёмы Hades-стайла);
        //   fogStart/fogEnd — дистанции начала и полного поглощения
        //       дымкой (мировые единицы);
        //   lift/gamma/gain/saturation — color grading;
        //   nearPlane/farPlane — проекция для линеаризации глубины
        //       (копия параметров камеры);
        //   gammaOutput — выходная гамма. Дефолт 1.0 (без коррекции):
        //       конвейер пока НЕ линейный (текстуры авторятся в sRGB),
        //       2.2 даст двойное осветление; полноценный sRGB-конвейер
        //       — TODO (GL_SRGB8_ALPHA8 + линейная работа)
        //
        // Цвета — линейный RGB [0, 1].
        // ---------------------------------------------------------------
        struct PostProcessSettings
        {
            blib::graphics::Vector3f fogColor;
            float fogStart;
            float fogEnd;

            blib::graphics::Vector3f lift;
            blib::graphics::Vector3f gamma;
            blib::graphics::Vector3f gain;
            float saturation;

            float vignetteStrength;

            float nearPlane;
            float farPlane;

            float gammaOutput;
        };

        // ---------------------------------------------------------------
        // PostProcess — полноэкранный пост-пасс поверх цветовой и
        // depth-текстур сцены (FBO из IRenderTarget): дымка по глубине,
        // color grading, виньетка, гамма. Рисует в ТЕКУЩИЙ framebuffer
        // (обычно 0 — back-буфер окна), сцена при этом лежит в FBO.
        //
        // Полноэкранный треугольник генерируется в вершинном шейдере
        // из gl_VertexID — VBO не нужен, только пустой VAO.
        //
        // Шейдеры компилируются лениво при первом apply() и участвуют
        // в hotreload (реестр ShaderProgram) — консольная команда
        // перекомпилирует и пост-пасс.
        // ---------------------------------------------------------------
        class __blib_graphics_api PostProcess
        {
        private:
            void* ctx;
            mutable bool baked = false;

            mutable blib::graphics::Shader vertexShader;
            mutable blib::graphics::Shader fragmentShader;
            mutable blib::graphics::ShaderProgram program;

            PostProcessSettings settings;

            void bake(blib::graphics::RenderContext& ctx) const;

        public:
            PostProcess();
            ~PostProcess();

            // Владение GL-ресурсами: копирование дало бы двойное
            // освобождение; перемещение не поддерживается
            PostProcess(const PostProcess&) = delete;
            PostProcess& operator=(const PostProcess&) = delete;

            void setSettings(_In const PostProcessSettings& newSettings);
            const PostProcessSettings& getSettings() const;

            // Отрисовать пост-пасс в текущий framebuffer:
            // colorTexture/depthTexture — текстуры сцены из
            // IRenderTarget::getColorTexture()/getDepthTexture()
            void apply(
                _In blib::graphics::RenderContext& ctx,
                _In const blib::graphics::Texture& colorTexture,
                _In const blib::graphics::Texture& depthTexture);
        };
    }
}
