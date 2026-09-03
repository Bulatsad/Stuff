#pragma once

#include <string>
#include <vector>

#include <imgui/imgui.h>

#include <blib/config.h>
#include <blib/core/console/consoleOutput.h>

namespace blib
{
    namespace graphics
    {
        namespace console
        {
            // ImGui-окно консоли — презентационный слой над blib::console::Console.
            // Вся логика (парсинг, история, дополнение, буфер вывода) живёт
            // в blib-core; здесь только отрисовка и интерактив:
            //   - цветной вывод по уровню сообщения (info/warning/error/command);
            //   - Enter — исполнение строки, Escape — очистка поля ввода;
            //   - Up/Down — навигация по истории ядра;
            //   - Tab — дополнение команд/переменных (кандидаты считает ядро);
            //   - автоскролл вниз при новых сообщениях.
            //
            // Потоковая модель: draw() дренирует ConsoleOutput ядра в локальный
            // скроллбэк displayLines — окно является единственным консюмером
            // буфера (контракт MPSC-очереди). Вызывающий управляет видимостью
            // окна и зовёт draw() внутри ImGui-кадра.
            class __blib_graphics_api ConsoleWindow
            {
            private:
                static const size_t inputBufferSize = 256;
                static const size_t defaultMaxDisplayLines = 4096;

                char inputBuffer[inputBufferSize];
                std::vector<blib::console::ConsoleLine> displayLines;
                size_t maxDisplayLines;
                bool autoScroll;
                bool scrollToBottom;       // принудительный скролл после Enter
                bool needsFocus;           // выставить фокус на поле ввода в этом кадре
                bool inHistoryNavigation;  // идёт навигация по истории (Up/Down)
                std::string pendingInput;  // недописанный ввод пользователя (для возврата из истории)

            public:
                explicit ConsoleWindow(size_t maxDisplayLines = defaultMaxDisplayLines);

                // Отрисовать окно консоли (внутри ImGui-кадра)
                void draw();

                // Очистить вывод: локальный скроллбэк + буфер ядра
                void clearDisplay();

                // Сфокусировать поле ввода (например, при открытии консоли)
                void requestFocus();

            private:
                // Прокладка для ImGui: коллбэк — статическая функция
                static int textEditCallback(ImGuiInputTextCallbackData* data);
                int handleTextEditCallback(ImGuiInputTextCallbackData* data);
            };
        }
    }
}
