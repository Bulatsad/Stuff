#include <blib/graphics/console/consoleWindow.h>

#include <blib/core/console/console.h>

// Цвета строк по уровню сообщения (единая палитра для всей консоли)
namespace
{
    ImVec4 consoleColorForType(blib::console::ConsoleMessageType type)
    {
        switch (type)
        {
            case blib::console::ConsoleMessageType::Info:
                return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // белый
            case blib::console::ConsoleMessageType::Warning:
                return ImVec4(1.0f, 0.85f, 0.30f, 1.0f); // жёлтый
            case blib::console::ConsoleMessageType::Error:
                return ImVec4(1.0f, 0.35f, 0.35f, 1.0f); // красный
            case blib::console::ConsoleMessageType::Command:
                return ImVec4(0.60f, 0.85f, 1.0f, 1.0f); // голубой (эхо команд)
        }

        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

blib::graphics::console::ConsoleWindow::ConsoleWindow(size_t maxDisplayLines_)
    : maxDisplayLines(maxDisplayLines_)
    , autoScroll(true)
    , scrollToBottom(false)
    , needsFocus(false)
    , inHistoryNavigation(false)
{
    this->inputBuffer[0] = '\0';
}

void blib::graphics::console::ConsoleWindow::draw()
{
    blib::console::Console& coreConsole = blib::console::Console::instance();

    // Дренируем вывод ядра в локальный скроллбэк. Мы — единственный
    // консюмер буфера (контракт MPSC-очереди), поэтому popLine() можно
    // звать без внешней синхронизации.
    blib::console::ConsoleLine line;
    while (coreConsole.getOutput().popLine(line))
    {
        this->displayLines.push_back(line);
        if (this->displayLines.size() > this->maxDisplayLines)
            this->displayLines.erase(this->displayLines.begin());
    }

    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Console"))
    {
        ImGui::End();
        return;
    }

    // Область вывода: занимает всё окно минус строка ввода
    const ImVec2 outputSize(0, -ImGui::GetFrameHeightWithSpacing());
    ImGui::BeginChild("ConsoleOutput", outputSize, ImGuiChildFlags_None);
    for (const blib::console::ConsoleLine& displayLine : this->displayLines)
        ImGui::TextColored(consoleColorForType(displayLine.type), "%s", displayLine.text.c_str());

    // Автоскролл: держимся низа, только если пользователь сам не ушёл вверх
    if (this->scrollToBottom
        || (this->autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    this->scrollToBottom = false;
    ImGui::EndChild();
    ImGui::Separator();

    // Поле ввода. EnterReturnsTrue — исполняем строку, CallbackCompletion —
    // Tab-дополнение, CallbackHistory — стрелки Up/Down по истории.
    // EscapeClearsAll — как в ImGui-демо консоли: Esc чистит поле ввода.
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
        | ImGuiInputTextFlags_EscapeClearsAll
        | ImGuiInputTextFlags_CallbackCompletion
        | ImGuiInputTextFlags_CallbackHistory;

    if (this->needsFocus)
    {
        ImGui::SetKeyboardFocusHere();
        this->needsFocus = false;
    }

    bool reclaimFocus = false;
    if (ImGui::InputText("Input", this->inputBuffer, inputBufferSize, inputFlags, &ConsoleWindow::textEditCallback, this))
    {
        // Enter: строка непустая — исполняем через ядро
        if (this->inputBuffer[0] != '\0')
        {
            coreConsole.execute(this->inputBuffer);
            this->scrollToBottom = true;
        }

        this->inputBuffer[0] = '\0';
        this->inHistoryNavigation = false;
        reclaimFocus = true;
    }

    // Фокус на поле ввода по умолчанию (при появлении окна) и после Enter
    ImGui::SetItemDefaultFocus();
    if (reclaimFocus)
        ImGui::SetKeyboardFocusHere(-1);

    ImGui::End();
}

void blib::graphics::console::ConsoleWindow::clearDisplay()
{
    this->displayLines.clear();
    blib::console::Console::instance().getOutput().clear();
}

void blib::graphics::console::ConsoleWindow::requestFocus()
{
    this->needsFocus = true;
}

int blib::graphics::console::ConsoleWindow::textEditCallback(ImGuiInputTextCallbackData* data)
{
    ConsoleWindow* window = static_cast<ConsoleWindow*>(data->UserData);
    return window->handleTextEditCallback(data);
}

int blib::graphics::console::ConsoleWindow::handleTextEditCallback(ImGuiInputTextCallbackData* data)
{
    blib::console::Console& coreConsole = blib::console::Console::instance();

    switch (data->EventFlag)
    {
        case ImGuiInputTextFlags_CallbackCompletion:
        {
            // Кандидатов и общий префикс считает ядро консоли
            std::vector<std::string> candidates;
            coreConsole.complete(std::string(data->Buf), candidates);

            if (candidates.empty())
            {
                coreConsole.log(blib::console::ConsoleMessageType::Info, "no matches");
                break;
            }

            // v1: заменяем всю строку ввода на общий префикс кандидатов
            // (курсор предполагается в конце строки)
            std::string prefix = blib::console::commonPrefix(candidates);
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, prefix.c_str());
            data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen;
            break;
        }

        case ImGuiInputTextFlags_CallbackHistory:
        {
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                // Перед уходом в историю запоминаем недописанный ввод,
                // чтобы Down вернул пользователя к нему
                if (!this->inHistoryNavigation)
                {
                    this->pendingInput = std::string(data->Buf);
                    this->inHistoryNavigation = true;
                }

                const std::string* entry = coreConsole.historyUp();
                if (entry)
                {
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, entry->c_str());
                    data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen;
                }
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                const std::string* entry = coreConsole.historyDown();
                if (entry)
                {
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, entry->c_str());
                }
                else
                {
                    // Курсор ушёл "мимо" конца истории — восстанавливаем
                    // пользовательский ввод
                    this->inHistoryNavigation = false;
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, this->pendingInput.c_str());
                }

                data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen;
            }
            break;
        }
    }

    return 0;
}
