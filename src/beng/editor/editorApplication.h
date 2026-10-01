#pragma once

#include <beng/config.h>
#include <beng/core/commandHistory.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/blibint.h>
#include <blib/core/math/vector.h>
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

// Шрифты ImGui: геттеры возвращают указатели (полное определение —
// у потребителей, включающих <imgui/imgui.h>)
struct ImFont;

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
         * Режим gizmo-манипулятора (переключается клавишами W/E/R,
         * как в Unity/Unreal).
         */
        enum class GizmoMode : buint8
        {
            Translate = 0,  // стрелки: перемещение вдоль осей
            Rotate,         // окружности: вращение вокруг осей
            Scale           // стрелки: масштабирование вдоль осей
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
         * - ImGui: контекст, WndProc-хук, кадр, панели консоли,
         *   центральные вкладки (Scene с вьюпортом + вкладки хоста
         *   ICenterTabView), меню-бар (File/Edit/Help), Unity-подобная
         *   тема, UI-шрифты (иконки MDPI + моно JetBrains Mono),
         *   горячие клавиши (тильда, Escape);
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

        // Gizmo-манипулятор: режимы W/E/R; начало/ход/конец драга за
        // стрелку/окружность (вызывается после отрисовки вьюпорт-панели,
        // внутри ImGui-кадра). Трансформации пишутся в историю команд
        void updateGizmoManipulator();

        // Меню-бар каркаса (File/Edit/Help): выход, загрузка/сброс
        // UI-шрифта (TTF через Win32-диалог), модальный диалог About.
        // Рисуется в конце UI-кадра (tick) — попап About ложится
        // поверх всех панелей и консоли
        void drawMainMenuBar();

        // Центральная область: контейнер с таб-баром — вкладка «Scene»
        // (вьюпорт каркаса) + вкладки хоста (ICenterTabView). Ставит
        // флаг видимости вьюпорта (beginFrame/isContentsDrawn) — гейт
        // gizmo-манипулятора и ray-picking'а
        void drawCenterTabs();

        // Перезагрузка UI-шрифтов ImGui: UI-шрифт + иконочный (отдельный
        // ImFont, Material Design Icons) + моно (JetBrains Mono, консоль).
        // ttfPath — пользовательский UI-шрифт; nullptr/"" — дефолтный
        // (Segoe UI → встроенный). true — запрошенный TTF загружен
        bool reloadEditorFonts(_In_opt const char* ttfPath);

        // Hit-тест манипулятора: курсор над осью/окружностью gizmo
        // выбранной сущности? outAxis: 0=X, 1=Y, 2=Z
        bool hitTestGizmo(
            _In const blib::math::Vector<float, 3>& rayOrigin,
            _In const blib::math::Vector<float, 3>& rayDirection,
            _In const blib::math::Vector<float, 3>& gizmoPosition,
            _Out buint8& outAxis) const;

        // Отрисовка gizmo выбранной сущности (в FBO вьюпорта, после
        // сцены и слоёв хоста): стрелки/окружности + подсветка hot-оси
        void drawGizmo();

        // Луч из орбитальной камеры через NDC-точку клика вьюпорта
        // ([-1,1], Y вверх). false — невалидная проекция (не должно
        // случаться при штатной камере)
        bool computeViewportRay(
            float ndcX, float ndcY,
            _Out blib::math::Vector<float, 3>& outOrigin,
            _Out blib::math::Vector<float, 3>& outDirection) const;

        // Расстояние между двумя лучами (луч мыши и ось gizmo) —
        // выбор активной оси манипулятора по близости курсора
        float gizmoRayAxisDistance(
            _In const blib::math::Vector<float, 3>& rayOrigin,
            _In const blib::math::Vector<float, 3>& rayDirection,
            _In const blib::math::Vector<float, 3>& axisOrigin,
            _In const blib::math::Vector<float, 3>& axisDirection) const;

        // Расстояние от луча до окружности (центр + нормаль + радиус) —
        // hit-тест окружностей режима Rotate
        float gizmoRayCircleDistance(
            _In const blib::math::Vector<float, 3>& rayOrigin,
            _In const blib::math::Vector<float, 3>& rayDirection,
            _In const blib::math::Vector<float, 3>& circleCenter,
            _In const blib::math::Vector<float, 3>& circleNormal,
            float circleRadius) const;

        // Сценные панели каркаса (Hierarchy/Inspector) включены по
        // умолчанию; выключается инструментами со своими панелями
        // в тех же зонах (вьювер) ДО initialize (см. setScenePanelsEnabled)
        bool scenePanelsEnabled;

        // Подмена содержимого вкладки «Scene» (id 0): если задано —
        // центр рисует вкладку хоста вместо 3D-вьюпорта каркаса.
        // Задаётся до initialize (см. setSceneTabView)
        ICenterTabView* sceneTabView;

        // Число строк, резервируемых под верхнюю панель хоста (onUi):
        // высота считается из метрик текущего шрифта ImGui (см.
        // setHostBarRows), а не пиксельной константой
        buint32 hostBarRows;

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

            /**
             * Вызывается при КЛИКЕ ЛКМ по изображению вьюпорта (не
             * драг). Каркас строит луч из камеры через точку клика;
             * хост решает, кого клик задел (ray-picking по своим
             * типам — каркас игровых рендер-типов не знает), и сам
             * вызывает selectEntity(). Клик мимо — selectEntity(invalid).
             *
             * @param rayOrigin Мировые координаты начала луча (камера)
             * @param rayDirection Нормированное направление луча
             */
            virtual void onViewportClick(
                _In const blib::math::Vector<float, 3>& rayOrigin,
                _In const blib::math::Vector<float, 3>& rayDirection);

        public:
            EditorApplication();
            virtual ~EditorApplication();

            EditorApplication(const EditorApplication&) = delete;
            EditorApplication& operator=(const EditorApplication&) = delete;

            // Дефолтные параметры окна единого эдитора (дефолты initialize)
            static constexpr uint16_t editorDefaultWindowWidth = 1280;
            static constexpr uint16_t editorDefaultWindowHeight = 720;
            static constexpr const char* editorDefaultWindowTitle = "beng-editor";

            /**
             * Инициализация каркаса: окно, GL, ImGui, сцена, панели.
             * ВИРТУАЛЬНАЯ: хосты (игра/инструмент) переопределяют её,
             * чтобы подготовить свой impl ДО инициализации каркаса
             * (хуки onInitialize() работают уже внутри неё). Плагин-
             * контракт зовёт initialize() через базовый указатель —
             * без virtual диспетчеризации impl хоста не создался бы
             * (см. ARCHITECTURE.md, «Сложности плагин-модели»).
             * Без аргументов — дефолтные параметры окна единого
             * эдитора (нужно DLL-этапу: вызывающий не знает хоста
             * и зовёт initialize() на базовом типе).
             * @return true при успехе
             */
            virtual bool initialize(
                _In uint16_t windowWidth = editorDefaultWindowWidth,
                _In uint16_t windowHeight = editorDefaultWindowHeight,
                _In const char* windowTitle = editorDefaultWindowTitle);

            /**
             * Один кадр: ввод, симуляция, рендер сцены, UI, презентация.
             */
            void tick();

            /**
             * Корректное гашение (идемпотентно). ВИРТУАЛЬНАЯ: хосты
             * переопределяют её, чтобы разрушить свой impl ДО гашения
             * каркаса (графические ресурсы обязаны умереть раньше
             * GL-контекста) — плагин-контракт зовёт shutdown() через
             * базовый указатель.
             */
            virtual void shutdown();

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
         * Зарегистрировать вкладку ЦЕНТРАЛЬНОЙ области эдитора
         * (таб-бар в центре, первая вкладка — «Scene» с вьюпортом):
         * Game у Gravelands и т.п. Контейнер с таб-баром и раскладкой
         * владеет каркас; вкладка рисует только контент
         * (контракт ICenterTabView). Вызывать из onInitialize (до
         * первого кадра). Вкладка обязана жить, пока зарегистрирована:
         * каркас хранит только указатель.
         *
         * @return id вкладки (>= 1) для setActiveCenterTab;
         *         невалидный id (buint32Max) при ошибке
         */
        buint32 registerCenterTab(_In ICenterTabView* tab);

        /**
         * Сделать активной вкладку центральной области по id
         * (0 — «Scene» вьюпорта каркаса либо вкладка, подменённая
         * через setSceneTabView). Смена применится в ближайшем кадре
         * (таб помечается SetSelected).
         */
        void setActiveCenterTab(buint32 tabId);

        /**
         * Id активной вкладки центральной области (0 — «Scene»).
         * Обновляется по факту выбора (клик пользователя или
         * программная смена в кадре).
         */
        buint32 getActiveCenterTab() const;

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
         * Заменить содержимое вкладки «Scene» (id 0) центральной
         * области: если задано, каркас рисует вкладку хоста
         * (ICenterTabView) вместо 3D-вьюпорта — инструменты без сцены
         * (sc2img) используют центр под свои задачи, заголовок вкладки
         * берётся из view->getTabName(). Вызывать ДО initialize();
         * после — warning + false. nullptr — вернуть вьюпорт каркаса.
         *
         * При подмене gizmo/ray-picking не работают (вьюпорт не
         * рисуется — isContentsDrawn() == false).
         *
         * @return true если флаг применён
         */
        bool setSceneTabView(_In_opt ICenterTabView* view);

        /**
         * Число строк под верхнюю панель хоста (onUi): высота панели =
         * rows строк текущего шрифта ImGui + отступы (без пиксельных
         * констант — при смене шрифта панель масштабируется). Дефолт —
         * 1 строка (одна строка контролов, как у вьювера моделей).
         * Вызывать ДО initialize(); после — warning + false.
         *
         * @return true если значение применено
         */
        bool setHostBarRows(_In buint32 rows);

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
         * Режим gizmo-манипулятора (W/E/R).
         */
        GizmoMode getGizmoMode() const;

        /**
         * Переключить режим gizmo-манипулятора.
         */
        void setGizmoMode(GizmoMode mode);

        /**
         * Включить/выключить РЕДАКТОРСКИЙ ввод (горячие клавиши W/E/R,
         * Ctrl+Z/Ctrl+Shift+Z, Delete). Выключается на время PIE:
         * глобальная клавиатура у клиентского окна общая — W/E/R эдитора
         * конфликтовали бы с вводом игры. Клики/гizmo вьюпорта остаются.
         */
        void setEditorInputEnabled(bool enabled);

        /**
         * История команд эдитора (undo/redo): правки полей, gizmo,
         * создание/удаление сущностей и компонентов. Панели и хосты
         * пишут в неё (см. CommandHistory).
         */
        CommandHistory& getCommandHistory();

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

            /**
             * UI-шрифты ImGui, загруженные каркасом (см. editorIcons.h):
             * иконочный (Material Design Icons — отдельный шрифт, глифы
             * рисуются PushFont'ом с нужным размером) и моно (JetBrains
             * Mono — консоль). nullptr — файл шрифта рядом с exe не
             * найден: хелперы иконок молча ничего не рисуют, консоль
             * использует дефолтный шрифт ImGui.
             */
            ImFont* getIconFont() const;
            ImFont* getMonoFont() const;
        };

    } // namespace editor
} // namespace beng
