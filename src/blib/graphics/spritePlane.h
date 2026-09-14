#pragma once

#include <blib/config.h>
#include <blib/utilmacro.h>

#include <blib/graphics/mesh.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/rendercontext.h>
#include <blib/graphics/transformable.h>

namespace blib
{
    namespace graphics
    {
        // ---------------------------------------------------------------
        // SpritePlane — вертикальная рисованная плоскость (NPR-гибрид,
        // фаза 5): декор и фоны рисуются как unlit-квады с alpha-test,
        // персонажи остаются lit-мешами (см. GRAVELANDS.md).
        //
        // Квад лежит в локальной плоскости XY (нормаль +Z), на камеру
        // выравнивается поворотом вокруг Y через setRotation (камера
        // в gravelands-клиенте фиксирована — billboarding не нужен,
        // угол ставится один раз при создании сцены).
        //
        // ВАЖНО — конвенция поворота: rotateY в движке (transformMatrix.cpp)
        // разворачивает локальную нормаль +Z в мировое направление
        // (-sin(yaw), 0, cos(yaw)). Чтобы плоскость смотрела на камеру,
        // yaw = atan2(-dir.x, dir.z), где dir — горизонтальное
        // направление «цель → камера» (пример — gravelands-клиент).
        //
        // Материал: ShadingMode::Unlit + диффузная текстура +
        // m_alphaTest — пиксели с alpha < порога отбрасываются в
        // шейдере (discard): жёсткие края в духе рисованного стиля,
        // сортировка плоскостей не требуется (видимость решает
        // depth-test, полупрозрачный блендинг не используется).
        // ---------------------------------------------------------------
        class __blib_graphics_api SpritePlane : public ITransformable, public IDrawable
        {
        private:
            // mutable: draw() (const, контракт IDrawable) синхронизирует
            // трансформ плоскости в меш — тот же паттерн, что Sphere
            mutable blib::graphics::Mesh mesh;

        public:
            SpritePlane();

            // Строит квад размера width x height вокруг локального центра
            // (ось Y — высота, нуль — «нога» плоскости). image — диффуз:
            // альфа-канал = прозрачность (порог — см. setAlphaTest)
            void create(float width, float height, _In const blib::graphics::Image& image);

            // Порог alpha-test: пиксели с alpha < порога отбрасываются;
            // 0 — плоскость полностью непрозрачна
            void setAlphaTest(float alphaTest);

            const blib::graphics::Mesh& getMesh() const;

            // Передать владение мешем наружу (NPR/ECS: меш уходит в
            // рендер-компонент, см. MeshRenderComponent). После вызова
            // объект пуст и не пригоден для отрисовки
            blib::graphics::Mesh takeMesh();

            // Release IDrawable api
            virtual void draw(RenderContext& ctx) const override;
        };
    }
}
