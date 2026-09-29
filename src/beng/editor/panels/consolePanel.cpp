#include <beng/editor/panels/consolePanel.h>

#include <imgui/imgui.h>

namespace beng
{
    namespace editor
    {
        ConsolePanel::ConsolePanel()
            : consoleWindow()
            , monoFont(nullptr)
        {
        }

        void ConsolePanel::setMonoFont(_In_opt ImFont* font)
        {
            this->monoFont = font;
        }

        void ConsolePanel::draw()
        {
            // Вывод/ввод консоли — моно-шрифтом (терминальный вид):
            // оконный виджет рисует себя сам, PushFont вокруг draw()
            // покрывает всё окно
            if (this->monoFont != nullptr)
            {
                ImGui::PushFont(this->monoFont);
            }
            this->consoleWindow.draw();
            if (this->monoFont != nullptr)
            {
                ImGui::PopFont();
            }
        }

        void ConsolePanel::requestFocus()
        {
            this->consoleWindow.requestFocus();
        }

        void ConsolePanel::clearDisplay()
        {
            this->consoleWindow.clearDisplay();
        }

    } // namespace editor
} // namespace beng
