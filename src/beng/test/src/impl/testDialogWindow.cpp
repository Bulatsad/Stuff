#include <blib/test/src/test.h>

#include <beng/editor/panels/dialogWindow.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace
{
    // Заголовки/сообщения диалогов в тестах (ID попапа = заголовок,
    // поэтому в одном тесте используются уникальные заголовки)
    constexpr const char* dialogTitleA = "Test Dialog A";
    constexpr const char* dialogTitleB = "Test Dialog B";
    constexpr const char* dialogMessage = "test message";
    constexpr const char* confirmLabel = "Force Apply";
    constexpr const char* cancelLabel = "Cancel";

    // Ширина кнопок DialogWindow (копия dialogButtonWidth из
    // dialogWindow.cpp) — нужна для вычисления позиции клика
    constexpr float testDialogButtonWidth = 120.0f;

    // Размер виртуального дисплея (для главного вьюпорта ImGui)
    constexpr float testDisplayWidth = 800.0f;
    constexpr float testDisplayHeight = 600.0f;

    /**
     * RAII-контекст ImGui для headless-тестов DialogWindow.
     *
     * Ядро ImGui работает без платформенного бэкенда: не нужны окно,
     * GL-контекст и imgui_impl_* — достаточно живого контекста и
     * пар NewFrame/EndFrame. События ввода подаются напрямую через
     * io.AddKeyEvent/AddMousePosEvent/AddMouseButtonEvent.
     */
    struct ImGuiTestContext
    {
        ImGuiTestContext()
        {
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            // Не писать imgui.ini в рабочую директорию тестов
            io.IniFilename = nullptr;
            io.DisplaySize = ImVec2(testDisplayWidth, testDisplayHeight);

            // Сборка шрифтового атласа вручную: в headless-режиме
            // (без рендер-бэкенда) ядро само его не строит, а
            // NewFrame требует готовый атлас
            unsigned char* texPixels = nullptr;
            int texWidth = 0;
            int texHeight = 0;
            io.Fonts->GetTexDataAsRGBA32(&texPixels, &texWidth, &texHeight);
        }

        ~ImGuiTestContext()
        {
            ImGui::DestroyContext();
        }

        // Один кадр: NewFrame -> draw() диалога -> EndFrame.
        // io-события добавляются ДО вызова (до NewFrame)
        void frame(_In beng::editor::DialogWindow& dialog)
        {
            ImGui::NewFrame();
            dialog.draw();
            ImGui::EndFrame();
        }
    };

    /**
     * Пробник состояния попапа: открыт ли (начат ли в последнем кадре)
     * попап-окно с заданным заголовком.
     *
     * ImGui::IsPopupOpen(name) читает g.CurrentWindow (ID-стек) и вне
     * окон неприменим, поэтому смотрим internals: окно попапа живо в
     * g.Windows, а флаг Active сбрасывается в начале каждого кадра и
     * выставляется Begin()'ом — true ровно для окон, отрисованных в
     * последнем кадре.
     */
    bool isPopupWindowActive(_In const char* title)
    {
        ImGuiWindow* window = ImGui::FindWindowByName(title);
        return window != nullptr && window->Active;
    }
}

BLIB_TEST_CASE("dialogWindow: open sets state and shows popup")
{
    ImGuiTestContext ctx;
    beng::editor::DialogWindow dialog;

    dialog.open(dialogTitleA, dialogMessage, confirmLabel, nullptr, nullptr, cancelLabel);

    // Состояние выставляется сразу; попап-окно появляется после draw()
    BLIB_TEST_CHECK(dialog.isOpen());
    BLIB_TEST_CHECK(std::string(dialog.getName()) == dialogTitleA);
    BLIB_TEST_CHECK(ImGui::FindWindowByName(dialogTitleA) == nullptr);

    ctx.frame(dialog);
    BLIB_TEST_CHECK(isPopupWindowActive(dialogTitleA));
}

BLIB_TEST_CASE("dialogWindow: escape cancels dialog and closes popup")
{
    ImGuiTestContext ctx;
    beng::editor::DialogWindow dialog;
    int cancelCount = 0;

    dialog.open(dialogTitleA, dialogMessage, confirmLabel, nullptr,
        [&cancelCount]() { ++cancelCount; }, cancelLabel);

    ctx.frame(dialog);
    BLIB_TEST_REQUIRE(isPopupWindowActive(dialogTitleA));

    // Escape в следующем кадре = отмена. РЕГРЕССИЯ: до фикса после
    // finishWithCancel кадр продолжался отрисовкой TextWrapped("") и
    // Button("") в корне окна — пустой label даёт id == window->ID и
    // IM_ASSERT в Debug-сборке ImGui (аварийное завершение процесса)
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    ctx.frame(dialog);

    BLIB_TEST_CHECK(cancelCount == 1);
    BLIB_TEST_CHECK(!dialog.isOpen());

    // Кадр после закрытия: попап больше не начинается
    ctx.frame(dialog);
    BLIB_TEST_CHECK(!isPopupWindowActive(dialogTitleA));
}

BLIB_TEST_CASE("dialogWindow: close resets state without callbacks")
{
    ImGuiTestContext ctx;
    beng::editor::DialogWindow dialog;
    int confirmCount = 0;
    int cancelCount = 0;

    dialog.open(dialogTitleA, dialogMessage, confirmLabel,
        [&confirmCount]() { ++confirmCount; },
        [&cancelCount]() { ++cancelCount; }, cancelLabel);
    ctx.frame(dialog);
    BLIB_TEST_REQUIRE(isPopupWindowActive(dialogTitleA));

    dialog.close();

    // Состояние сброшено сразу, колбэки не вызваны
    BLIB_TEST_CHECK(!dialog.isOpen());
    BLIB_TEST_CHECK(confirmCount == 0);
    BLIB_TEST_CHECK(cancelCount == 0);

    // Кадр после close(): draw() пропускает попап, ImGui закрывает
    // его в конце кадра
    ctx.frame(dialog);
    BLIB_TEST_CHECK(!isPopupWindowActive(dialogTitleA));
}

BLIB_TEST_CASE("dialogWindow: click on confirm button fires onConfirm and closes")
{
    ImGuiTestContext ctx;
    beng::editor::DialogWindow dialog;
    int confirmCount = 0;
    int cancelCount = 0;

    dialog.open(dialogTitleA, dialogMessage, confirmLabel,
        [&confirmCount]() { ++confirmCount; },
        [&cancelCount]() { ++cancelCount; }, cancelLabel);

    // Кадр 1 — появление; кадр 2 — AlwaysAutoResize уже устаканил
    // размер окна (автоподгонка применяется в конце кадра)
    ctx.frame(dialog);
    ctx.frame(dialog);

    ImGuiWindow* window = ImGui::FindWindowByName(dialogTitleA);
    BLIB_TEST_REQUIRE(window != nullptr);

    // Позиция кнопки подтверждения: кнопки выравнены по правому
    // нижнему углу контента, confirm левее cancel (см. draw())
    const ImGuiStyle& style = ImGui::GetStyle();
    const float buttonHeight = style.FramePadding.y * 2.0f + ImGui::GetFontSize();
    const float cancelCenterX = window->Pos.x + window->Size.x - style.WindowPadding.x - testDialogButtonWidth * 0.5f;
    const float confirmCenterX = cancelCenterX - testDialogButtonWidth - style.ItemSpacing.x;
    const float buttonsCenterY = window->Pos.y + window->Size.y - style.WindowPadding.y - buttonHeight * 0.5f;
    const ImVec2 confirmCenter(confirmCenterX, buttonsCenterY);

    // Клик: нажатие в одном кадре, отпускание в следующем
    // (Button активируется на отпускании внутри прямоугольника)
    ImGui::GetIO().AddMousePosEvent(confirmCenter.x, confirmCenter.y);
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    ctx.frame(dialog);
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    ctx.frame(dialog);

    BLIB_TEST_CHECK(confirmCount == 1);
    BLIB_TEST_CHECK(cancelCount == 0);
    BLIB_TEST_CHECK(!dialog.isOpen());
}

BLIB_TEST_CASE("dialogWindow: reopen replaces previous dialog")
{
    ImGuiTestContext ctx;
    beng::editor::DialogWindow dialog;
    int oldConfirmCount = 0;

    dialog.open(dialogTitleA, dialogMessage, confirmLabel,
        [&oldConfirmCount]() { ++oldConfirmCount; }, nullptr, cancelLabel);
    ctx.frame(dialog);
    BLIB_TEST_REQUIRE(isPopupWindowActive(dialogTitleA));

    // Повторный open() поверх открытого диалога: состояние заменяется,
    // колбэки старого диалога не вызываются, старый попап закрывается
    dialog.open(dialogTitleB, dialogMessage, confirmLabel, nullptr, nullptr, cancelLabel);
    BLIB_TEST_CHECK(std::string(dialog.getName()) == dialogTitleB);

    ctx.frame(dialog);
    BLIB_TEST_CHECK(!isPopupWindowActive(dialogTitleA));
    BLIB_TEST_CHECK(isPopupWindowActive(dialogTitleB));
    BLIB_TEST_CHECK(oldConfirmCount == 0);
}
