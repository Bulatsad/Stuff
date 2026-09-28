#pragma once

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

// Форвард-декларации blib-graphics: ссылки возвращаются геттерами,
// полные определения нужны только потребителям
namespace blib
{
    namespace graphics
    {
        class IRenderTarget;
        class OrbitCamera;
        class RenderWindow;
    }
}

namespace beng
{
    class Scene;

    namespace editor
    {
        /**
         * Зона размещения панели в дефолтной раскладке эдитора.
         *
         * Раскладка пока ручная (фиксированные размеры, как у
         * прототипа-вьювера): зоны LeftTop/LeftBottom/Right + вьюпорт
         * по центру + консоль снизу. В будущем заменится докингом
         * (см. TODO в iPanel.h) — зоны останутся контрактом по умолчанию.
         */
        enum class PanelZone : buint8
        {
            LeftTop,    // левая колонка, верхняя часть
            LeftBottom, // левая колонка, нижняя часть
            Right       // правая колонка
        };

        /**
         * EditorApplication — каркас эдитора beng: приложение-хост,
         * единое для всех игр и инструментов (плагин-модель, см.
         * ARCHITECTURE.md).
         *
         * Владеет игра-агностичной частью эдитора:
         * - окно + GL-контекст и рендер-таргет (FBO вьюпорта);
         * - орбитальную камеру вьюпорта;
         * - ECS-сцену с движковыми типами beng-client и системами
         *   (Transform → Animation → Render);
         * - ImGui: контекст, WndProc-хук, кадр, панели вьюпорта и
         *   консоли, горячие клавиши (тильда, Escape);
         * - раскладку панелей по зонам (registerPanel).
         *
         * Игра/инструмент (хост) наследует EditorApplication и
         * переопределяет хуки on*() — регистрация собственных типов,
         * панели, отладочные слои, своя обработка ввода. Это будущий
         * контракт IGameModule: на этапе DLL-плагина хук-интерфейс
         * станет границей эдитор ↔ игра.
         *
         * Frame-API (паттерн «lib + тонкий exe»): каркас НЕ владеет
         * главным циклом — его крутит тонкий exe хоста.
         */
        class __beng_api EditorApplication
        {
    private:
        struct EditorApplicationImpl;
        EditorApplicationImpl* impl;

        // Gizmo-манипулятор: при зажатой G и выбранной сущности ЛКМ-драг
        // двигает её по горизонтальной плоскости взгляда камеры
        // (вызывается после отрисовки вьюпорт-панели, внутри ImGui-кадра)
        void updateGizmoDrag();

        // Сценные панели каркаса (Hierarchy/Inspector) включены по
        // умолчанию; выключается инструментами со своими панелями
        // в тех же зонах (вьювер) ДО initialize (см. setScenePanelsEnabled)
        bool scenePanelsEnabled;

        protected:
            /**
             * Вызывается в initialize() после создания каркаса
             * (окно/сцена/системы/панели каркаса готовы). Хост
             * регистрирует свои типы компонентов, системы и панели
             * (registerPanel). Движковые типы beng-client уже
             * зарегистрированы каркасом.
             */
            virtual void onInitialize(_In Scene& scene);

            /**
             * Вызывается каждый кадр после каркасных горячих клавиш
             * (тильда/Escape), до симуляции. Свои горячие клавиши.
             */
            virtual void onInput();

            /**
             * Вызывается каждый кадр непосредственно ПЕРЕД
             * scene.update() (после очистки FBO). Хост выставляет
             * состояние рендера (флаги RenderContext и т.п.).
             */
            virtual void onSceneWillUpdate(float deltaTime);

            /**
             * Вызывается каждый кадр сразу ПОСЛЕ scene.update(),
             * всё ещё в FBO вьюпорта. Отладочные слои (скелет,
             * wireframe и т.п.) рисуются здесь.
             */
            virtual void onSceneDidUpdate(float deltaTime);

            /**
             * Вызывается внутри ImGui-кадра, между панелями зон и
             * вьюпортом. Свои ImGui-окна: верхняя панель, модальные
             * диалоги.
             */
            virtual void onUi();

            /**
             * Вызывается при нажатии Escape, когда консоль закрыта.
             * @return true — хост обработал нажатие (окно приложения
             *         закрывать НЕ нужно; например, отменён модальный
             *         диалог); false — каркас закроет приложение.
             */
            virtual bool onEscapePressed();

        public:
            EditorApplication();
            virtual ~EditorApplication();

            EditorApplication(const EditorApplication&) = delete;
            EditorApplication& operator=(const EditorApplication&) = delete;

            /**
             * Инициализация каркаса: окно, GL, ImGui, сцена, панели.
             * @return true при успехе
             */
            bool initialize(_In uint16_t windowWidth, _In uint16_t windowHeight, _In const char* windowTitle);

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

        /**
         * Зарегистрировать панель хоста в дефолтной раскладке.
         * Вызывать из onInitialize (до первого кадра). Панель
         * обязана жить, пока зарегистрирована: каркас хранит
         * только указатель (контракт IPanel — панели не владеют
         * данными, каркас не владеет панелями).
         */
        void registerPanel(_In IPanel* panel, _In PanelZone zone);

        /**
         * Включить/выключить СЦЕННЫЕ панели каркаса (Scene Hierarchy
         * слева сверху + Inspector справа). По умолчанию включены —
         * это панели единого эдитора: работают с любой сценой через
         * рефлексию (см. ARCHITECTURE.md, «Эдитор»). Вызывать ДО
         * initialize(); после initialize() — warning + false.
         *
         * Инструменты со СВОИМИ панелями в тех же зонах (вьювер
         * моделей) выключают сценные панели, чтобы не конфликтовать
         * раскладкой.
         *
         * @return true если флаг применён
         */
        bool setScenePanelsEnabled(bool enabled);

        /**
         * Выбранная сущность сцены (selection эдитора). Источник
         * выбора — SceneHierarchyPanel (каркасная панель); читатели —
         * Inspector (поля), gizmo хоста, будущий gizmo-манипулятор.
         * Устаревший выбор (сущность удалена/сцена перезагружена)
         * сбрасывается в invalidEntity панелью иерархии.
         */
        EntityID getSelectedEntity() const;

        /**
         * Установить выбранную сущность (программно; панель иерархии
         * подсветит узел в следующем кадре). invalidEntity — снять выбор.
         */
        void selectEntity(EntityID entity);

        /**
         * Сбросить орбитальную камеру на дефолтный ракурс
         * (цель — начало координат, штатная дистанция).
         */
        void resetCamera();

            /**
             * ECS-сцена каркаса (создание сущностей/компонентов,
             * кеш ресурсов — scene.getResources()).
             */
            Scene& getScene();

            /**
             * Рендер-таргет (FBO) вьюпорта.
             */
            blib::graphics::IRenderTarget& getRenderTarget();

            /**
             * Орбитальная камера вьюпорта.
             */
            blib::graphics::OrbitCamera& getCamera();

            /**
             * Окно приложения (Win32-хуки, диалоги файлов и т.п.).
             */
            blib::graphics::RenderWindow& getWindow();
        };

    } // namespace editor
} // namespace beng
