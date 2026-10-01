#pragma once

#include <beng/editor/editorApplication.h>

#include <blib/utilmacro.h>

#include <string>

namespace sc2img
{
    /**
     * Sc2imgApp — GUI-утилита конвертации текстур Stronghold
     * (TGX/GM1 → PNG/BMP/JPG), интерфейс в духе старого Stronghold
     * Image Toolbox: открыть файл → сетка-контактник кадров с выбором
     * → превью с зумом → экспорт выделенных/всех.
     *
     * Хост поверх каркаса beng::editor::EditorApplication (паттерн
     * model_viewer): вкладка «Images» (ICenterTabView) подменяет
     * содержимое вкладки «Scene» (setSceneTabView — 3D-вьюпорт
     * утилите не нужен), верхняя панель опций (3 строки —
     * setHostBarRows), консоль каркаса — лог операций.
     *
     * Frame-API (паттерн «lib + тонкий exe»): ядро НЕ владеет главным
     * циклом — его крутит тонкий exe (main/main.cpp). Headless-режим
     * CLI живёт в том же exe и в core (sc2img-core).
     */
    class Sc2imgApp : public beng::editor::EditorApplication
    {
    private:
        struct Sc2imgAppImpl;
        Sc2imgAppImpl* impl;

        // Вкладка «Images» (контракт ICenterTabView): подменяет
        // содержимое вкладки «Scene»; определение скрыто в .cpp
        class ImagesTab;

        // Верхняя панель: путь к файлу, опции экспорта, кнопки
        void drawToolBar();

        // Загрузка документа из пути (со сменой playerColor —
        // перезагрузка) и пересборка GL-миниатюр
        void loadDocument(_In const std::string& path);
        void rebuildThumbnails();

        // Экспорт с текущими опциями (по маске выбора или всех)
        void exportImages(_In_opt const buint8* selectedMask);

        // Диалог выбора файла (Win32) — паттерн model_viewer
        bool browseFile(_In const char* title, _In const char* filter, _Out char* outPath, size_t outSize);
        void browseImageFile();

    protected:
        void onInput() __blib_override;
        void onUi() __blib_override;

    public:
        Sc2imgApp();
        ~Sc2imgApp();

        Sc2imgApp(const Sc2imgApp&) = delete;
        Sc2imgApp& operator=(const Sc2imgApp&) = delete;

        /**
         * Инициализация (override каркаса): impl утилиты + каркас
         * EditorApplication (окно, FBO, ECS, ImGui). Параметры окна —
         * собственные константы утилиты (см. .cpp).
         * @return true при успехе
         */
        bool initialize(
            _In uint16_t windowWidth = beng::editor::EditorApplication::editorDefaultWindowWidth,
            _In uint16_t windowHeight = beng::editor::EditorApplication::editorDefaultWindowHeight,
            _In const char* windowTitle = beng::editor::EditorApplication::editorDefaultWindowTitle) __blib_override;

        /**
         * Корректное гашение (идемпотентно, override каркаса): GL-текстуры
         * миниатюр освобождаются ДО гашения каркаса (контекст жив).
         */
        void shutdown() __blib_override;

        /**
         * Открыть файл .tgx/.gm1 (CLI-аргумент main при GUI-запуске).
         * Требует выполненного initialize (создание GL-миниатюр).
         */
        void openFile(_In const std::string& path);
    };

} // namespace sc2img
