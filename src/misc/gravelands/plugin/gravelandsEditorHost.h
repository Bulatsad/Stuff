#pragma once

#include <beng/editor/editorApplication.h>
#include <beng/editor/gameModule.h>

// Макрос экспорта точки входа плагина: dllexport при сборке
// gravelands.dll (CMake задаёт GRAVELANDS_PLUGIN_EXPORTS), пустой
// при статической линковке (этап 1 плагин-модели, см. ARCHITECTURE.md)
#ifndef GRAVELANDS_PLUGIN_API
#ifdef GRAVELANDS_PLUGIN_EXPORTS
#define GRAVELANDS_PLUGIN_API __declspec(dllexport)
#else
#define GRAVELANDS_PLUGIN_API
#endif
#endif

namespace gravelands
{
    /**
     * GravelandsEditorHost — «игровая сторона» единого эдитора
     * (плагин-модель, см. ARCHITECTURE.md).
     *
     * Реализация контракта «эдитор ↔ игра» поверх хуков каркаса
     * `beng::editor::EditorApplication`: в каркасной сцене эдитора
     * хостится мир Gravelands (`gravelands::World` — та же сцена,
     * что у клиента), подключаются консольные команды мира
     * (scene_save/scene_load), отладочное управление светом и
     * hot-reload шейдеров.
     *
     * На DLL-этапе этот класс станет содержимым gravelands.dll:
     * эдитор получает его через контракт GameModuleFunctions
     * (bengGetGameModule — см. ниже), код хоста не изменился.
     * Пока (этап 1) плагин линкуется в `beng-editor.exe` статически
     * (CMake-опция `gravelands_plugin_type`) — это удобный режим
     * отладки, меняется только способ доставки, не код игры.
     */
    class GravelandsEditorHost : public beng::editor::EditorApplication
    {
    private:
        struct GravelandsEditorHostImpl;
        GravelandsEditorHostImpl* impl;

        // Запуск/останов/пауза PIE-сессии (Play/Pause/Stop в верхней
        // полосе; Escape — стоп). Play/Stop переключают центральную
        // вкладку (Game/Scene, как в Unity)
        void startPie();
        void stopPie();

    protected:
        // Хуки EditorApplication (см. editorApplication.h):
        // регистрация мира/команд, дебаг-клавиши, правка света,
        // верхняя полоса эдитора, выбор кликом во вьюпорте,
        // Game-превью из активной камеры
        void onInitialize(_In beng::Scene& scene) __blib_override;
        void onInput() __blib_override;
        void onSceneWillUpdate(float deltaTime) __blib_override;
        void onSceneDidUpdate(float deltaTime) __blib_override;
        void onUi() __blib_override;
        bool onEscapePressed() __blib_override;
        void onViewportClick(
            _In const blib::math::Vector<float, 3>& rayOrigin,
            _In const blib::math::Vector<float, 3>& rayDirection) __blib_override;

    public:
        GravelandsEditorHost();
        ~GravelandsEditorHost();

        GravelandsEditorHost(const GravelandsEditorHost&) = delete;
        GravelandsEditorHost& operator=(const GravelandsEditorHost&) = delete;

        /**
         * Инициализация (override каркаса): impl хоста (мир)
         * создаётся ДО каркасного EditorApplication::initialize —
         * хуки onInitialize() работают уже внутри него. Плагин-
         * контракт зовёт initialize() через базовый указатель —
         * виртуальная диспетчеризация обязательна (см. editorApplication.h).
         * @return true при успехе
         */
        bool initialize(
            _In uint16_t windowWidth = beng::editor::EditorApplication::editorDefaultWindowWidth,
            _In uint16_t windowHeight = beng::editor::EditorApplication::editorDefaultWindowHeight,
            _In const char* windowTitle = beng::editor::EditorApplication::editorDefaultWindowTitle) __blib_override;

        /**
         * Корректное гашение (идемпотентно, override каркаса): мир
         * разрушается ДО гашения каркаса (меши должны умереть раньше
         * GL-контекста) — см. .cpp.
         */
        void shutdown() __blib_override;

        // tick()/isRunning() — наследуются от EditorApplication
    };

    /**
     * Фабрика хоста эдитора игры — внутренняя функция модуля
     * (см. gravelandsEditorHost.cpp): на DLL-этапе наружу торчит
     * только точка входа bengGetGameModule, статический режим зовёт
     * её же напрямую.
     *
     * Память — через GlobalAllocator (shared blib — один на процесс);
     * владелец гасит хост ПАРНОЙ функцией gravelandsDestroyEditorHost
     * (она знает конкретный тип и возвращает память аллокатору) —
     * самому вызывать деструктор нельзя: в DLL-режиме вызывающий не
     * знает конкретного типа хоста.
     */
    beng::editor::EditorApplication* gravelandsCreateEditorHost();

    /**
     * Парная фабрике функция уничтожения хоста: гасит и возвращает
     * память GlobalAllocator'у (конкретный тип известен только плагину).
     */
    void gravelandsDestroyEditorHost(_In beng::editor::EditorApplication* host);

} // namespace gravelands

// Точка входа игрового модуля — ВНЕ namespace: глобальный extern "C"
// символ bengGetGameModule (стабильное имя контракта — см.
// beng::editor::gameModuleEntryName в beng/editor/gameModule.h).
// Эдитор зовёт её БЕЗ квалификации (игровых namespace'ов не знает):
// DLL-режим — GetProcAddress, статический — прямой вызов. Возвращает
// статическую структуру контракта GameModuleFunctions — память не
// требует освобождения.
extern "C" GRAVELANDS_PLUGIN_API const beng::editor::GameModuleFunctions* bengGetGameModule();
