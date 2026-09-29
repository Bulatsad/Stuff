#include <beng/editor/editorApplication.h>

#include <blib/core/console/console.h>
#include <blib/utilmacro.h>

// Плагин игры (плагин-модель, см. ARCHITECTURE.md «Эдитор»):
// - BENG_EDITOR_GRAVELANDS_STATIC: этап 1 — статический линк (плагин
//   подключается на этапе компоновки, exe знает фабрику напрямую);
// - BENG_EDITOR_GRAVELANDS_DLL: этап 2 — exe НЕ знает игру: грузит
//   gravelands.dll через LoadLibrary и берёт фабрику по стабильному
//   extern "C"-имени (GetProcAddress). Дефайны ставит корневой CMake
//   по gravelands_plugin_type.
#ifdef BENG_EDITOR_GRAVELANDS_STATIC
#include <gravelands/plugin/gravelandsEditorHost.h>
#include <blib/system/memory/globalAllocator.h>
#endif

#ifdef BENG_EDITOR_GRAVELANDS_DLL
#include <Windows.h>
#endif

namespace
{
    // Параметры окна пустого эдитора (ветка без плагина игры;
    // правило проекта: без вшитых литералов)
    constexpr uint16_t editorWindowWidth = 1280;
    constexpr uint16_t editorWindowHeight = 720;
    constexpr const char* editorWindowTitle = "beng-editor";

    // DLL-этап: имя плагина (лежит рядом с exe — см. CMake плагина)
    // и стабильное имя фабрики (extern "C" — недекорированное)
    constexpr const char* gravelandsPluginFileName = "gravelands.dll";
    constexpr const char* gravelandsCreateFactoryName = "gravelandsCreateEditorHost";
    constexpr const char* gravelandsDestroyFactoryName = "gravelandsDestroyEditorHost";
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
    // Эдитор с игрой Gravelands (этап 1: статический линк). Хост
    // плагина владеет миром и переопределяет хуки каркаса. Память —
    // GlobalAllocator, владение — у тонкого exe
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
#elif defined(BENG_EDITOR_GRAVELANDS_DLL)
    // Эдитор с игрой Gravelands (этап 2: DLL-плагин). Exe НЕ знает
    // игру: грузит gravelands.dll, берёт фабрику по стабильному
    // extern "C"-имени. Конкретный тип хоста известен только плагину —
    // гашение через парную фабрику (gravelandsDestroyEditorHost)
    HMODULE pluginModule = LoadLibraryA(gravelandsPluginFileName);
    if (pluginModule == nullptr)
    {
        __blib_fatal("Failed to load game plugin '%s'", gravelandsPluginFileName);
    }

    using PluginCreateFn = beng::editor::EditorApplication* (*)();
    using PluginDestroyFn = void (*)(beng::editor::EditorApplication*);

    PluginCreateFn createFactory = reinterpret_cast<PluginCreateFn>(
        GetProcAddress(pluginModule, gravelandsCreateFactoryName));
    PluginDestroyFn destroyFactory = reinterpret_cast<PluginDestroyFn>(
        GetProcAddress(pluginModule, gravelandsDestroyFactoryName));
    if (createFactory == nullptr || destroyFactory == nullptr)
    {
        FreeLibrary(pluginModule);
        __blib_fatal("Game plugin '%s' does not export the factory contract", gravelandsPluginFileName);
    }

    beng::editor::EditorApplication* editor = createFactory();
    if (editor == nullptr || !editor->initialize())
    {
        FreeLibrary(pluginModule);
        __blib_fatal("Failed to initialize beng-editor");
    }

    // Главный цикл принадлежит тонкому exe
    while (editor->isRunning())
    {
        editor->tick();
    }

    editor->shutdown();
    destroyFactory(editor);

    // Выгружаем плагин после уничтожения всех его объектов
    FreeLibrary(pluginModule);
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
