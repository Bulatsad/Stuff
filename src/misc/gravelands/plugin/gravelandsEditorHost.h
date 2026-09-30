#pragma once

#include <beng/editor/editorApplication.h>

// Макрос экспорта фабрик плагина: dllexport при сборке gravelands.dll
// (CMake задаёт GRAVELANDS_PLUGIN_EXPORTS), пустой при статической
// линковке (этап 1 плагин-модели, см. ARCHITECTURE.md)
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
     * фабрика `gravelandsCreateEditorHost()` превратится в
     * экспортируемый символ плагина, код хоста не изменится.
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
        // верхняя полоса эдитора, выбор кликом во вьюпорте
        void onInitialize(_In beng::Scene& scene) __blib_override;
        void onInput() __blib_override;
        void onSceneWillUpdate(float deltaTime) __blib_override;
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
         * Инициализация: impl хоста (мир) + каркас EditorApplication
         * (окно, FBO, камера, ImGui, сцена эдитора).
         * @return true при успехе
         */
        bool initialize();

        /**
         * Корректное гашение (идемпотентно): мир разрушается ДО
         * гашения каркаса (меши должны умереть раньше GL-контекста).
         * Скрывает базовый EditorApplication::shutdown() — см. .cpp.
         */
        void shutdown();

        // tick()/isRunning() — наследуются от EditorApplication
    };

    /**
     * Фабрика игрового модуля эдитора — единственная точка входа
     * плагина: на DLL-этапе это экспортируемый символ gravelands.dll
     * (extern "C" — стабильное недекорированное имя для GetProcAddress;
     * см. ARCHITECTURE.md, «Сложности плагин-модели»).
     *
     * Память — через GlobalAllocator (shared blib — один на процесс);
     * владелец гасит хост ПАРНОЙ функцией gravelandsDestroyEditorHost
     * (она знает конкретный тип и возвращает память аллокатору) —
     * самому вызывать деструктор нельзя: в DLL-режиме вызывающий не
     * знает конкретного типа хоста.
     */
    extern "C" GRAVELANDS_PLUGIN_API GravelandsEditorHost* gravelandsCreateEditorHost();

    /**
     * Парная фабрике функция уничтожения хоста: гасит и возвращает
     * память GlobalAllocator'у (конкретный тип известен только плагину).
     */
    extern "C" GRAVELANDS_PLUGIN_API void gravelandsDestroyEditorHost(_In beng::editor::EditorApplication* host);

} // namespace gravelands
