#pragma once

#include <string>

#include <blib/utilmacro.h>

namespace modelviewer
{
    /**
     * ViewerCore — ядро 3D-вьювера моделей (прототип эдитора).
     *
     * Собирает в одно приложение:
     * - ECS-сцену beng (модель = Entity с Transform/SkinnedMesh/
     *   Animator компонентами; AnimationSystem/RenderSystem);
     * - орбитальную камеру и рендер в FBO вьюпорта;
     * - панели beng-editor на Dear ImGui: иерархия костей, таблица
     *   анимаций с плейбеком, опции визуализации, консоль;
     * - отладочные слои: скелет линиями (LineRenderer), wireframe.
     *
     * Frame-API (паттерн «lib + тонкий exe»): ядро НЕ владеет главным
     * циклом — его крутит тонкий exe (main/main.cpp). В будущем эти же
     * вызовы сможет делать эдитор in-process.
     */
    class ViewerCore
    {
    private:
        struct ViewerCoreImpl;
        ViewerCoreImpl* impl;

        // Загрузка модели в ECS-сцену (см. .cpp)
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

        // Отладочные слои поверх сцены (в тот же FBO)
        void drawSkeleton();
        void drawWireframe();

        // Лейаут панелей и верхняя панель загрузки модели
        void drawPanels();

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
         * Инициализация: окно, FBO, камера, ECS, ImGui.
         * @return true при успехе
         */
        bool initialize();

        /**
         * Один кадр: ввод, симуляция, рендер сцены, UI, презентация.
         */
        void tick();

        /**
         * Корректное гашение (идемпотентно).
         */
        void shutdown();

        /**
         * Условие выхода из главного цикла.
         */
        bool isRunning() const;
    };

} // namespace modelviewer
