#pragma once

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/graphics/console/consoleWindow.h>

namespace beng
{
    namespace editor
    {
        /**
         * ConsolePanel — панель-обёртка над ядром консоли.
         *
         * Презентация консоли живёт в blib::graphics::console::
         * ConsoleWindow (ImGui-окно поверх blib::console::Console,
         * история/автодополнение/цвета — всё в blib-core).
         * Здесь — только панельный интерфейс: draw() внутри
         * ImGui-кадра + сервисные методы фокуса/очистки.
         *
         * ConsoleWindow является единственным консюмером буфера
         * вывода ядра — двух таких панелей одновременно быть не
         * должно (см. контракт MPSC-очереди в consoleWindow.h).
         */
        class __beng_api ConsolePanel : public beng::editor::IPanel
        {
        private:
            blib::graphics::console::ConsoleWindow consoleWindow;

        public:
            void draw() override;

            /**
             * Сфокусировать поле ввода (например, при открытии
             * консоли горячей клавишей).
             */
            void requestFocus();

            /**
             * Очистить вывод: скроллбэк окна + буфер ядра.
             */
            void clearDisplay();

            const char* getName() const override { return "Console"; }
        };

    } // namespace editor
} // namespace beng
