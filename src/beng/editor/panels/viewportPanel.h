#pragma once

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/graphics/orbitCamera.h>
#include <blib/graphics/rendertarget.h>

namespace beng
{
    namespace editor
    {
        /**
         * ViewportPanel — 3D-вьюпорт эдитора.
         *
         * Назначение:
         * - Показывает содержимое FBO рендер-таргета через
         *   ImGui::Image (текстура текущего кадра) с сохранением
         *   пропорций кадра;
         * - Обрабатывает ввод орбитальной камеры, пока курсор
         *   находится над вьюпортом:
         *     ЛКМ-драг  — вращение вокруг цели;
         *     СКМ-драг  — панорама;
         *     колёсико  — зум.
         *
         * Окно вьюпорта зафиксировано (NoMove/NoResize/NoCollapse):
         * перетаскивание заголовка не должно двигать панель — иначе
         * при езде окна под неподвижным курсором ImGui-бэкенд
         * выдаёт ложные MouseDelta (клиентские координаты меняются)
         * и камера крутится от перемещения окна.
         *
         * Вращение/панорама срабатывают только из драга, НАЧАТОГО на
         * изображении (клик на заголовке/другом окне не считается):
         * флаг захвата ставится IsItemClicked, снимается IsMouseReleased.
         *
         * Данные: не владеет ни таргетом, ни камерой.
         */
        class __beng_api ViewportPanel : public beng::editor::IPanel
        {
        private:
            blib::graphics::IRenderTarget* renderTarget;
            blib::graphics::OrbitCamera* camera;

            // Флаги захвата драга, начатого на изображении вьюпорта
            bool rotating;
            bool panning;

            // Доступный размер области изображения, измеренный в
            // последнем кадре — вызывающий может подогнать под него
            // рендер-таргет (1:1 пиксель, без растяжения)
            float lastViewportWidth;
            float lastViewportHeight;

            // Обработка мышиного ввода камеры
            void handleCameraInput();

        public:
            ViewportPanel();

            /**
             * Привязать рендер-таргент и камеру (nullptr допустим —
             * панель рисует заглушку).
             */
            void setRenderTarget(_In_opt blib::graphics::IRenderTarget* target);
            void setCamera(_In_opt blib::graphics::OrbitCamera* cam);

            /**
             * Размер доступной области вьюпорта в последнем кадре
             * (0, пока панель не отрисовалась ни разу).
             */
            float getLastViewportWidth() const;
            float getLastViewportHeight() const;

            void draw() override;
            const char* getName() const override { return "Viewport"; }
        };

    } // namespace editor
} // namespace beng
