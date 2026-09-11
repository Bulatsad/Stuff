#include <beng/editor/panels/consolePanel.h>

namespace beng
{
    namespace editor
    {
        void ConsolePanel::draw()
        {
            this->consoleWindow.draw();
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
