#include <beng/editor/editorApplication.h>
#include <beng/editor/gameModule.h>

#include <blib/core/console/console.h>
#include <blib/utilmacro.h>

// Плагин игры (плагин-модель, см. ARCHITECTURE.md «Эдитор»). Exe НЕ
// знает игровых типов — работает только через контракт
// GameModuleFunctions (beng/editor/gameModule.h). Способ доставки
// выбирает CMake (дефайны по gravelands_plugin_type):
// - BENG_EDITOR_STATIC_GAME_HEADER: статический линк (этап 1) — exe
//   включает заголовок игры и зовёт точку входа bengGetGameModule()
//   напрямую;
// - BENG_EDITOR_GAME_DLL: DLL-этап (этап 2) — exe грузит <game>.dll
//   через LoadLibrary и берёт точку входа по стабильному имени
//   (beng::editor::gameModuleEntryName).
// Код плагина один и тот же — меняется только способ доставки.
#ifdef BENG_EDITOR_STATIC_GAME_HEADER
#include BENG_EDITOR_STATIC_GAME_HEADER
#endif

#ifdef BENG_EDITOR_GAME_DLL
#include <Windows.h>
#endif

#include <cstring>

namespace
{
    // Параметры окна пустого эдитора (ветка без игры; правило
    // проекта: без вшитых литералов)
    constexpr uint16_t editorWindowWidth = 1280;
    constexpr uint16_t editorWindowHeight = 720;
    constexpr const char* editorWindowTitle = "beng-editor";

    // Опция выбора игры: --game=<стабильный id игры>. В DLL-режиме
    // id — имя DLL; в статическом — сверяется со влинкованной игрой
    // (рассинхрон — пустой эдитор с warning)
    constexpr const char* gameOptionPrefix = "--game=";

    // Имя игры по умолчанию (дефайн BENG_EDITOR_DEFAULT_GAME_NAME
    // задаёт игра при подключении к эдитору): beng-editor.exe без
    // опций открывает именно её
#ifdef BENG_EDITOR_DEFAULT_GAME_NAME
    constexpr const char* defaultGameName = BENG_EDITOR_DEFAULT_GAME_NAME;
#endif

    // Пределы длины id игры: имя DLL собирается в стековый буфер
    constexpr buint32 maxGameNameLength = 63;
    constexpr const char* dllExtension = ".dll";

    // Длина префикса опции (вычисляется компилятором)
    constexpr buint32 gameOptionPrefixLength = sizeof(gameOptionPrefix) - 1;

    /**
     * Найти в argv значение опции --game=<id>; nullptr, если опции нет.
     * Без аллокаций: возвращает указатель внутрь argv
     */
    const char* findRequestedGame(_In buint32 argc, _In char* argv[])
    {
        for (buint32 i = 1; i < argc; ++i)
        {
            if (std::strncmp(argv[i], gameOptionPrefix, gameOptionPrefixLength) == 0)
            {
                return argv[i] + gameOptionPrefixLength;
            }
        }
        return nullptr;
    }

    /**
     * Собрать имя DLL игры: "<id>.dll" в стековый буфер вызывающего.
     * @return true при успехе (false — id пустой/слишком длинный)
     */
    bool buildDllFileName(_In const char* gameName, _Out char* buffer, _In buint32 bufferSize)
    {
        const buint32 extensionLength = sizeof(dllExtension) - 1;
        const buint32 nameLength = static_cast<buint32>(std::strlen(gameName));
        if (nameLength == 0 || nameLength > maxGameNameLength ||
            nameLength + extensionLength + 1 > bufferSize)
        {
            return false;
        }

        for (buint32 i = 0; i < nameLength; ++i)
        {
            buffer[i] = gameName[i];
        }
        for (buint32 i = 0; i < extensionLength; ++i)
        {
            buffer[nameLength + i] = dllExtension[i];
        }
        buffer[nameLength + extensionLength] = '\0';
        return true;
    }

    /**
     * Прогнать готовый хост эдитора игры: initialize → главный цикл →
     * shutdown. Уничтожение хоста — за вызывающим (через парную
     * фабрику модуля игры destroyEditorHost)
     */
    void runEditorHost(_In beng::editor::EditorApplication* editor)
    {
        if (!editor->initialize())
        {
            __blib_fatal("Failed to initialize beng-editor");
        }

        // Главный цикл принадлежит тонкому exe (паттерн «lib + тонкий exe»)
        while (editor->isRunning())
        {
            editor->tick();
        }

        editor->shutdown();
    }

    /**
     * Запустить пустой эдитор: каркас без игры (пустая сцена +
     * движковые типы). Используется без плагина и при несовпадении
     * запрошенной игры со влинкованной (статический режим)
     */
    void runEmptyEditor()
    {
        beng::editor::EditorApplication editor;
        if (!editor.initialize(editorWindowWidth, editorWindowHeight, editorWindowTitle))
        {
            __blib_fatal("Failed to initialize beng-editor");
        }

        while (editor.isRunning())
        {
            editor.tick();
        }

        editor.shutdown();
    }

    /**
     * Сверка контракта игрового модуля: рассинхрон версий — fatal
     * (ABI мог разъехаться — продолжать нельзя)
     */
    void verifyGameModule(_In const beng::editor::GameModuleFunctions* module)
    {
        if (module == nullptr)
        {
            __blib_fatal("Game module entry returned null");
        }
        if (module->contractVersion != beng::editor::gameModuleContractVersion)
        {
            __blib_fatal("Game module contract version mismatch: engine %u, game %u",
                beng::editor::gameModuleContractVersion, module->contractVersion);
        }
    }
}

/**
 * beng-editor — единый эдитор на все игры (плагин-модель, см.
 * ARCHITECTURE.md). Тонкая main() над каркасом EditorApplication
 * (beng-editor-core). Игра подключается через контракт
 * GameModuleFunctions (beng/editor/gameModule.h):
 * - статический линк (BENG_EDITOR_STATIC_GAME_HEADER) — exe зовёт
 *   точку входа bengGetGameModule() напрямую;
 * - DLL (BENG_EDITOR_GAME_DLL) — LoadLibrary("<id>.dll") +
 *   GetProcAddress(gameModuleEntryName); id — из --game=<id> или
 *   BENG_EDITOR_DEFAULT_GAME_NAME;
 * - без плагина — пустой эдитор (пустая сцена, движковые типы).
 */
int main(int argc, char* argv[])
{
    // Дублировать вывод консоли в stdout (удобно при запуске из
    // командной строки) — паттерн model_viewer/gravelands
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    const buint32 argCount = static_cast<buint32>(argc);

#if defined(BENG_EDITOR_STATIC_GAME_HEADER)
    // ===== Статический режим: игра влинкована в exe (этап 1) =====
    const char* requestedGame = findRequestedGame(argCount, argv);

    const beng::editor::GameModuleFunctions* module = bengGetGameModule();
    verifyGameModule(module);

    if (requestedGame != nullptr && std::strcmp(requestedGame, module->getGameName()) != 0)
    {
        // Запрошена не та игра, что влинкована: честный отказ вместо
        // запуска чужой игры (выбор игры — на этапе сборки exe)
        __blib_log_warning("Game '%s' is not linked into this editor (built-in game is '%s'); starting with an empty scene",
            requestedGame, module->getGameName());
        runEmptyEditor();
        return 0;
    }

    beng::editor::EditorApplication* editor = module->createEditorHost();
    runEditorHost(editor);
    module->destroyEditorHost(editor);
#elif defined(BENG_EDITOR_GAME_DLL)
    // ===== DLL-режим: exe НЕ знает игру (этап 2) =====
    const char* requestedGame = findRequestedGame(argCount, argv);
    const char* gameName = requestedGame;
#ifdef BENG_EDITOR_DEFAULT_GAME_NAME
    if (gameName == nullptr)
    {
        gameName = defaultGameName;
    }
#endif

    if (gameName == nullptr)
    {
        // Ни --game, ни игры по умолчанию: пустой эдитор
        runEmptyEditor();
        return 0;
    }

    char dllFileName[maxGameNameLength + sizeof(dllExtension)];
    if (!buildDllFileName(gameName, dllFileName, sizeof(dllFileName)))
    {
        __blib_fatal("Invalid game name '%s'", gameName);
    }

    HMODULE pluginModule = LoadLibraryA(dllFileName);
    if (pluginModule == nullptr)
    {
        __blib_fatal("Failed to load game plugin '%s'", dllFileName);
    }

    // Единственный символ плагина: точка входа контракта по
    // стабильному extern "C"-имени (движок игровых имён не знает)
    using GameModuleEntryFn = const beng::editor::GameModuleFunctions* (*)();
    GameModuleEntryFn entry = reinterpret_cast<GameModuleEntryFn>(
        GetProcAddress(pluginModule, beng::editor::gameModuleEntryName));
    if (entry == nullptr)
    {
        FreeLibrary(pluginModule);
        __blib_fatal("Game plugin '%s' does not export the game module entry '%s'",
            dllFileName, beng::editor::gameModuleEntryName);
    }

    const beng::editor::GameModuleFunctions* module = entry();
    verifyGameModule(module);

    beng::editor::EditorApplication* editor = module->createEditorHost();
    runEditorHost(editor);
    module->destroyEditorHost(editor);

    // Выгружаем плагин после уничтожения всех его объектов
    FreeLibrary(pluginModule);
#else
    // ===== Пустой эдитор: каркас без игры =====
    runEmptyEditor();
#endif
    return 0;
}
