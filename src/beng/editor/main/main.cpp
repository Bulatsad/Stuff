#include <beng/editor/editorApplication.h>

#include <blib/core/console/console.h>
#include <blib/utilmacro.h>

// Плагин игры (этап 1 плагин-модели: статический линк; этап 2 —
// загрузка gravelands.dll через ту же фабрику). Дефайн ставит
// корневой CMake, когда плагин подключён к beng-editor
#ifdef BENG_EDITOR_GRAVELANDS_STATIC
#include <gravelands/plugin/gravelandsEditorHost.h>
#include <blib/system/memory/globalAllocator.h>
#endif

namespace
{
    // Параметры окна пустого эдитора (ветка без плагина игры;
    // правило проекта: без вшитых литералов)
    constexpr uint16_t editorWindowWidth = 1280;
    constexpr uint16_t editorWindowHeight = 720;
    constexpr const char* editorWindowTitle = "beng-editor";
}

/**
 * beng-editor — единый эдитор на все игры (плагин-модель, см.
 * ARCHITECTURE.md). Тонкая main() над каркасом EditorApplication
 * (beng-editor-core):
 * - с плагином (BENG_EDITOR_GRAVELANDS_STATIC): игра хостится через
 *   фабрику gravelandsCreateEditorHost() — та же точка входа, что
 *   на DLL-этапе станет экспортом gravelands.dll;
 * - без плагина: эдитор запускается с пустой сценой и движковыми
 *   типами.
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Дублировать вывод консоли в stdout (удобно при запуске из
    // командной строки) — паттерн model_viewer/gravelands
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

#ifdef BENG_EDITOR_GRAVELANDS_STATIC
    // Эдитор с игрой Gravelands: хост плагина владеет миром и
    // переопределяет хуки каркаса. Память — GlobalAllocator,
    // владение — у тонкого exe (паттерн «lib + тонкий exe»)
    gravelands::GravelandsEditorHost* editor = gravelands::gravelandsCreateEditorHost();
    if (!editor->initialize())
    {
        __blib_fatal("Failed to initialize beng-editor");
    }

    // Главный цикл принадлежит тонкому exe
    while (editor->isRunning())
    {
        editor->tick();
    }

    editor->shutdown();
    editor->~GravelandsEditorHost();
    blib::memory::GlobalAllocator::instance().deallocate(
        editor, sizeof(gravelands::GravelandsEditorHost));
#else
    // Пустой эдитор: каркас без игры (пустая сцена, движковые типы)
    beng::editor::EditorApplication editor;
    if (!editor.initialize(editorWindowWidth, editorWindowHeight, editorWindowTitle))
    {
        __blib_fatal("Failed to initialize beng-editor");
    }

    // Главный цикл принадлежит тонкому exe (паттерн «lib + тонкий exe»)
    while (editor.isRunning())
    {
        editor.tick();
    }

    editor.shutdown();
#endif
    return 0;
}
