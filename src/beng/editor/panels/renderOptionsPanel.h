#pragma once

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

namespace beng
{
    namespace editor
    {
        /**
         * RenderOptionsPanel — переключатели слоёв визуализации.
         *
         * Галочки:
         * - Show Skeleton     — отрисовка всего скелета линиями;
         * - Diffuse Texture   — диффузная текстура (выкл = плоский
         *                       серый цвет вместо текстуры);
         * - Wireframe         — проволочная отрисовка рёбер поверх
         *                       заливки (независимо от текстуры).
         *
         * Панель хранит только состояние; кто именно применяет
         * переключатели — ответственность вызывающего (вьювер читает
         * геттеры при отрисовке кадра).
         */
        class __beng_api RenderOptionsPanel : public beng::editor::IPanel
        {
        private:
            bool showSkeleton;
            bool showDiffuseTexture;
            bool showWireframe;

        public:
            RenderOptionsPanel();

            void draw() override;
            const char* getName() const override { return "Render Options"; }

            bool isSkeletonVisible() const;
            bool isDiffuseTextureVisible() const;
            bool isWireframeVisible() const;
        };

    } // namespace editor
} // namespace beng
