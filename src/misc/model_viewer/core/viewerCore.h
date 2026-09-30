#pragma once

#include <beng/editor/editorApplication.h>

#include <blib/utilmacro.h>

#include <string>

namespace modelviewer
{
    /**
     * ViewerCore — ядро 3D-вьювера моделей (инструмент, не эдитор —
     * единый эдитор проекта живёт в beng-editor, см. ARCHITECTURE.md).
     *
     * Хост поверх каркаса beng::editor::EditorApplication: собирает
     * ECS-сцену beng (модель = Entity с Transform/SkinnedMesh/
     * Animator компонентами), свои панели beng-editor (иерархия
     * костей, таблица анимаций с плейбеком, опции визуализации),
     * верхнюю панель загрузки модели и отладочные слои (скелет
     * линиями, wireframe) через хуки каркаса.
     *
     * Frame-API (паттерн «lib + тонкий exe»): ядро НЕ владеет главным
     * циклом — его крутит тонкий exe (main/main.cpp).
     */
    class ViewerCore : public beng::editor::EditorApplication
    {
    private:
        struct ViewerCoreImpl;
        ViewerCoreImpl* impl;

        // Загрузка модели в ECS-сцену каркаса (см. .cpp)
        void loadModel(_In const std::string& path);
        void unloadModel();

        // Диалог выбора файла (Win32) — см. .cpp
        bool browseFile(_In const char* title, _In const char* filter, _Out char* outPath, size_t outSize);
        void browseModelFile();

        // Смена мешей (skin) с сохранением скелета и анимаций (см. .cpp)
        void changeSkin();

        // Форсированная смена скина при несовместимом скелете:
        // вызывается по кнопке Force Apply диалога несовместимости
        void applySkinForced(_In const std::string& path);

        // Добавление внешних анимаций к текущей модели (см. .cpp)
        void addAnimation();

        // Отладочные слои поверх сцены (в тот же FBO вьюпорта)
        void drawSkeleton();
        void drawWireframe();

        // Верхняя панель: путь к модели + кнопки загрузки
        void drawModelBar();

    protected:
        // Хуки EditorApplication: регистрация панелей, горячие клавиши,
        // отладочные слои, верхняя панель, Escape-цепочка
        void onInitialize(_In beng::Scene& scene) __blib_override;
        void onInput() __blib_override;
        void onSceneWillUpdate(float deltaTime) __blib_override;
        void onSceneDidUpdate(float deltaTime) __blib_override;
        void onUi() __blib_override;
        bool onEscapePressed() __blib_override;

    public:
        ViewerCore();
        ~ViewerCore();

        ViewerCore(const ViewerCore&) = delete;
        ViewerCore& operator=(const ViewerCore&) = delete;

        /**
         * Загрузить модель в сцену (см. .cpp). Предыдущая модель
         * выгружается.
         * @return true при успехе
         */
        bool loadModelFromFile(_In const std::string& path);

        /**
         * Инициализация (override каркаса): impl вьювера + каркас
         * EditorApplication (окно, FBO, камера, ECS, ImGui). Параметры
         * окна — собственные константы вьювера (см. .cpp); переданные
         * игнорируются (инструмент, не плагин игры — контракт требует
         * сигнатуру каркаса для виртуальной диспетчеризации).
         * @return true при успехе
         */
        bool initialize(
            _In uint16_t windowWidth = beng::editor::EditorApplication::editorDefaultWindowWidth,
            _In uint16_t windowHeight = beng::editor::EditorApplication::editorDefaultWindowHeight,
            _In const char* windowTitle = beng::editor::EditorApplication::editorDefaultWindowTitle) __blib_override;

        /**
         * Корректное гашение (идемпотентно, override каркаса): выгрузка
         * модели и панелей вьювера ДО гашения каркаса (сцена/GL должны
         * их пережить) — см. .cpp.
         */
        void shutdown() __blib_override;

        // tick()/isRunning() — наследуются от EditorApplication
    };

} // namespace modelviewer
