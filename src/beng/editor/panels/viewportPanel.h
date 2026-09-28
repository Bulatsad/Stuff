#pragma once

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/graphics/orbitCamera.h>
#include <blib/graphics/rendertarget.h>

#include <functional>

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

            // Курсор над изображением вьюпорта в последнем кадре
            // (обратная связь для gizmo-манипулятора каркаса)
            bool cursorOverViewport;

            // Клик ЛКМ по изображению в последнем кадре + NDC курсора
            // внутри изображения ([-1,1], Y вверх) — для ray-picking
            // каркаса (см. takeViewportClick)
            bool clickedThisFrame;
            float clickNdcX;
            float clickNdcY;

            // NDC текущего курсора внутри изображения ([-1,1], Y вверх),
            // обновляется каждый кадр — для gizmo-осей каркаса
            float cursorNdcX;
            float cursorNdcY;

            // Разрешено ли вращение камеры ЛКМ-драгом. Выключается
            // каркасом в gizmo-режиме (зажата G, есть выбор): ЛКМ-драг
            // двигает выбранную сущность, а не камеру
            bool cameraRotationEnabled;

            // Предикат блокировки вращения камеры (каркас): true —
            // клик по изображению НЕ захватывает вращение (курсор над
            // стрелкой gizmo / идёт драг манипулятора)
            std::function<bool()> rotationBlockPredicate;

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

            /**
             * Курсор находился над изображением вьюпорта в последнем
             * кадре? (для gizmo-манипулятора каркаса)
             */
            bool isCursorOverViewport() const { return this->cursorOverViewport; }

            /**
             * Разрешить/запретить вращение камеры ЛКМ-драгом.
             * В gizmo-режиме (зажата G, есть выбор) каркас отключает
             * вращение: драг двигает выбранную сущность.
             */
            void setCameraRotationEnabled(bool enabled) { this->cameraRotationEnabled = enabled; }

            /**
             * Предикат блокировки вращения камеры: если возвращает true,
             * клик по изображению НЕ захватывает вращение (каркас отдаёт
             * «курсор над стрелкой gizmo или идёт драг манипулятора»).
             * Вызывается в момент клика — обязан быть дёшев.
             */
            void setRotationBlockPredicate(_In_opt std::function<bool()> predicate)
            {
                this->rotationBlockPredicate = std::move(predicate);
            }

            /**
             * Забрать клик по изображению (если был в последнем кадре)
             * и NDC-координаты курсора внутри изображения ([-1,1], Y
             * вверх — левый нижний угол = (-1,-1)). Клик — не драг:
             * каркас использует его для ray-picking сущностей.
             *
             * @return true если клик был; флаг сбрасывается
             */
            bool takeViewportClick(_Out float& outNdcX, _Out float& outNdcY);

            /**
             * NDC текущего курсора внутри изображения ([-1,1], Y вверх),
             * вычисленный в последнем кадре — для gizmo-осей каркаса.
             */
            void getCursorNdc(_Out float& outNdcX, _Out float& outNdcY) const;

            void draw() override;
            const char* getName() const override { return "Viewport"; }
        };

    } // namespace editor
} // namespace beng
