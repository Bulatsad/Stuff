#pragma once

#include <beng/editor/editorApplication.h>

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

    protected:
        // Хуки EditorApplication (см. editorApplication.h):
        // регистрация мира/команд, дебаг-клавиши, правка света,
        // верхняя полоса эдитора, выбор кликом во вьюпорте
        void onInitialize(_In beng::Scene& scene) __blib_override;
        void onInput() __blib_override;
        void onSceneWillUpdate(float deltaTime) __blib_override;
        void onUi() __blib_override;
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
     * плагина: на DLL-этапе станет экспортируемым символом
     * gravelands.dll (см. ARCHITECTURE.md, «Сложности плагин-модели»).
     *
     * Память — через GlobalAllocator; вызывающий владеет хостом и
     * обязан вызвать shutdown(), явный деструктор и вернуть память
     * (паттерн «lib + тонкий exe», см. main/main.cpp эдитора).
     */
    GravelandsEditorHost* gravelandsCreateEditorHost();

} // namespace gravelands
