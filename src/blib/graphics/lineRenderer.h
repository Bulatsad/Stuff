#pragma once

#include <vector>

#include <blib/config.h>
#include <blib/utilmacro.h>

#include <blib/graphics/color.h>
#include <blib/graphics/drawable.h>
#include <blib/graphics/shader.h>
#include <blib/graphics/transformable.h>
#include <blib/graphics/vector.h>

namespace blib
{
    namespace graphics
    {
        // ---------------------------------------------------------------
        // LineRenderer — отрисовка набора отрезков (GL_LINES) с
        // повершинным цветом. Основа отладочной визуализации: скелеты,
        // гизмо, сетки, рамки.
        //
        // Реализует IDrawable, поэтому рисуется через
        // IRenderTarget::draw() как обычный объект. Модельная матрица —
        // через ITransformable (можно повторить трансформацию меша,
        // чтобы линии жили в его локальном пространстве).
        //
        // Геометрия перезаливается в VBO при каждой отрисовке
        // (GL_DYNAMIC_DRAW): класс рассчитан на данные, которые
        // меняются каждый кадр (например, поза скелета).
        // ---------------------------------------------------------------
        class __blib_graphics_api LineRenderer : public blib::graphics::IDrawable, public blib::graphics::ITransformable
        {
        public:
            // Вершина отрезка: позиция + RGBA8 (16 байт, без паддинга).
            // Публичная только для раскладки VBO в impl/lineRenderer.cpp
            struct LineVertex
            {
                blib::graphics::Vector3f position;
                blib::graphics::Color color;
            };

        private:
            std::vector<LineVertex> vertices;
            void* ctx;
            mutable bool baked = false;

            mutable blib::graphics::Shader vertexShader;
            mutable blib::graphics::Shader fragmentShader;
            mutable blib::graphics::ShaderProgram program;

            // Ленивая инициализация GL-ресурсов при первой отрисовке
            void bake(blib::graphics::RenderContext& ctx) const;

        public:
            LineRenderer();
            ~LineRenderer();

            // Добавить отрезок (одна линия между двумя точками)
            void addLine(_In const blib::graphics::Vector3f& start, _In const blib::graphics::Vector3f& end, _In const blib::graphics::Color& color);

            // Удалить все отрезки
            void clear();

            // Количество добавленных отрезков
            buint32 getSegmentCount() const;

            // IDrawable
            virtual void draw(blib::graphics::RenderContext& ctx) const override;
        };
    }
}
