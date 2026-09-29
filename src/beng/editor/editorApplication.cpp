#include <beng/editor/editorApplication.h>

#include <beng/client/components/ambientLightComponent.h>
#include <beng/client/components/animatorComponent.h>
#include <beng/client/components/blobShadowComponent.h>
#include <beng/client/components/directionalLightComponent.h>
#include <beng/client/components/meshRenderComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/client/systems/animationSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/components/transform.h>
#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/editor/panels/consolePanel.h>
#include <beng/editor/panels/inspectorPanel.h>
#include <beng/editor/panels/sceneHierarchyPanel.h>
#include <beng/editor/panels/viewportPanel.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/core/math/angle.h>
#include <blib/core/math/trigonometry.h>
#include <blib/core/math/utilfuncs.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/lineRenderer.h>
#include <blib/graphics/orbitCamera.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/renderWindow.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_win32.h>

#include <Windows.h>
#include <commdlg.h>
#include <gl/GL.h>

#include <string.h>
#include <vector>

// ---------------------------------------------------------------
// ImGui-хук в оконную процедуру (паттерн вьювера/gravelands):
// ImGui обрабатывает ввод первым, остальное — движку.
// Объявлен на ГЛОБАЛЬНОМ скоупе: ImGui_ImplWin32_WndProcHandler —
// экспортируемый символ бэкенда (внутри namespace extern
// объявлял бы другую функцию)
// ---------------------------------------------------------------
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    WNDPROC s_engineWndProc = nullptr;

    LRESULT CALLBACK editorImguiWndProc(HWND hwnd, UINT uMsg, WPARAM wparam, LPARAM lparam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wparam, lparam))
        {
            return 1;
        }

        return CallWindowProc(s_engineWndProc, hwnd, uMsg, wparam, lparam);
    }
}

namespace beng
{
    namespace editor
    {
        namespace
        {
            // ---------------------------------------------------------------
            // Константы каркаса (правило проекта: без вшитых литералов)
            // ---------------------------------------------------------------

            // Камера вьюпорта
            constexpr float cameraFovDegrees = 60.0f;
            constexpr float cameraNearDistance = 0.1f;
            constexpr float cameraFarDistance = 1000.0f;
            constexpr float cameraInitialDistance = 100.0f;
            constexpr float cameraTargetX = 0.0f;
            constexpr float cameraTargetY = 0.0f;
            constexpr float cameraTargetZ = 0.0f;

            // Дефолтная раскладка панелей (ручная, до докинга)
            constexpr float leftPanelWidth = 280.0f;
            constexpr float rightPanelWidth = 320.0f;
            // Верхняя полоса приложения: резервируется каркасом,
            // заполняет хост через onUi (панель модели/меню)
            constexpr float topBarHeight = 44.0f;
            // Доля окна под верхнюю зону левой колонки (остальное —
            // нижняя зона)
            constexpr float hierarchyHeightFraction = 0.65f;
            constexpr float consoleWidth = 900.0f;
            constexpr float consoleHeight = 340.0f;

            // Минимальная измеряемая размерность вьюпорта (защита от
            // схлопнутого окна/панели)
            constexpr float minViewportDimension = 1.0f;

            // Обёртка консоли: команда clear чистит вывод
            constexpr const char* clearCommandName = "clear";
            constexpr const char* clearCommandHelp = "clears the console output";

            // Gizmo-манипулятор: длина осей/радиус окружностей (мир. ед.)
            constexpr float gizmoAxisLength = 25.0f;

            // Масштаб экранных пикселей в мировые единицы на дистанции
            // камеры (как панорама в ViewportPanel)
            constexpr float gizmoPanScalePerPixel = 0.0015f;

            // Порог близости курсора к оси/окружности (в пикселях) —
            // ось становится активной (подсветка/драг)
            constexpr float gizmoAxisHitPixelThreshold = 10.0f;

            // Скорость вращения в режиме Rotate (градус/пиксель)
            constexpr float gizmoRotateSpeedDegPerPixel = 0.3f;

            // Сегменты окружностей gizmo (режим Rotate)
            constexpr buint32 gizmoCircleSegments = 48;
            constexpr float twoPi = 6.2831853f;

            // Минимальный компонент масштаба (защита от вырождения)
            constexpr float gizmoMinScaleComponent = 0.01f;

            // Цвета осей X/Y/Z и подсветки активной оси
            constexpr buint8 gizmoAxisXColorR = 255;
            constexpr buint8 gizmoAxisXColorG = 90;
            constexpr buint8 gizmoAxisXColorB = 90;
            constexpr buint8 gizmoAxisYColorR = 90;
            constexpr buint8 gizmoAxisYColorG = 255;
            constexpr buint8 gizmoAxisYColorB = 90;
            constexpr buint8 gizmoAxisZColorR = 90;
            constexpr buint8 gizmoAxisZColorG = 90;
            constexpr buint8 gizmoAxisZColorB = 255;
            constexpr buint8 gizmoHotColorR = 255;
            constexpr buint8 gizmoHotColorG = 255;
            constexpr buint8 gizmoHotColorB = 80;
            constexpr buint8 gizmoAxisAlpha = 255;

            // «Оси нет» (горячая ось отсутствует)
            constexpr buint8 gizmoNoAxis = 0xFF;

            // «Бесконечность» для hit-тестов (луч промахнулся)
            constexpr float gizmoNoHitDistance = 100000.0f;

            // Клавиши режимов gizmo (как в Unity/Unreal)
            constexpr blib::graphics::Keyboard::Key gizmoTranslateKey = blib::graphics::Keyboard::Key::W;
            constexpr blib::graphics::Keyboard::Key gizmoRotateKey = blib::graphics::Keyboard::Key::E;
            constexpr blib::graphics::Keyboard::Key gizmoScaleKey = blib::graphics::Keyboard::Key::R;

            // ---------------------------------------------------------------
            // Тема эдитора (ImGui): Unity-подобная палитра тёмно-серых
            // тонов + синий акцент, скругления, плотность. Числа здесь —
            // таблица стиля, а не логика: константы ради читаемости
            // таблицы и правила «без вшитых литералов»
            // ---------------------------------------------------------------

            // Скругления (Unity-подобные: окна чуть, скроллбары — пилюли)
            constexpr float themeWindowRounding = 4.0f;
            constexpr float themeChildRounding = 3.0f;
            constexpr float themeFrameRounding = 3.0f;
            constexpr float themePopupRounding = 4.0f;
            constexpr float themeGrabRounding = 2.0f;
            constexpr float themeScrollbarRounding = 8.0f;
            constexpr float themeTabRounding = 4.0f;

            // Плотность: «воздух» между элементами, компактные фреймы
            constexpr ImVec2 themeWindowPadding(8.0f, 8.0f);
            constexpr ImVec2 themeFramePadding(5.0f, 4.0f);
            constexpr ImVec2 themeItemSpacing(6.0f, 6.0f);
            constexpr ImVec2 themeItemInnerSpacing(6.0f, 4.0f);
            constexpr float themeIndentSpacing = 18.0f;
            constexpr float themeScrollbarSize = 12.0f;
            constexpr float themeGrabMinSize = 10.0f;
            constexpr ImVec2 themeTitleAlign(0.0f, 0.5f);

            // Бордеры: тонкие тёмные линии на границах панелей
            constexpr float themeWindowBorderSize = 1.0f;
            constexpr float themeChildBorderSize = 1.0f;
            constexpr float themePopupBorderSize = 1.0f;
            constexpr float themeTabBorderSize = 1.0f;

            // Палитра (0..1): тёплый тёмно-серый разных тонов —
            // многотонность помогает считывать иерархию UI
            constexpr ImVec4 themeWindowBg(0.18f, 0.18f, 0.18f, 1.00f);
            constexpr ImVec4 themeChildBg(0.16f, 0.16f, 0.16f, 1.00f);
            constexpr ImVec4 themePopupBg(0.21f, 0.21f, 0.21f, 0.98f);
            constexpr ImVec4 themeTitleBg(0.15f, 0.15f, 0.15f, 1.00f);
            constexpr ImVec4 themeTitleBgActive(0.20f, 0.20f, 0.20f, 1.00f);
            constexpr ImVec4 themeMenuBarBg(0.20f, 0.20f, 0.20f, 1.00f);
            constexpr ImVec4 themeText(0.90f, 0.90f, 0.90f, 1.00f);
            constexpr ImVec4 themeTextDisabled(0.50f, 0.50f, 0.50f, 1.00f);
            constexpr ImVec4 themeBorder(0.11f, 0.11f, 0.11f, 1.00f);
            constexpr ImVec4 themeFrameBg(0.24f, 0.24f, 0.24f, 1.00f);
            constexpr ImVec4 themeFrameHovered(0.31f, 0.31f, 0.31f, 1.00f);
            constexpr ImVec4 themeFrameActive(0.20f, 0.20f, 0.20f, 1.00f);
            constexpr ImVec4 themeElementBg(0.24f, 0.24f, 0.24f, 1.00f);
            constexpr ImVec4 themeElementHovered(0.31f, 0.31f, 0.31f, 1.00f);
            constexpr ImVec4 themeElementActive(0.20f, 0.20f, 0.20f, 1.00f);
            constexpr ImVec4 themeScrollbarBg(0.13f, 0.13f, 0.13f, 1.00f);
            constexpr ImVec4 themeScrollbarGrab(0.32f, 0.32f, 0.32f, 1.00f);
            constexpr ImVec4 themeScrollbarGrabHovered(0.40f, 0.40f, 0.40f, 1.00f);
            constexpr ImVec4 themeScrollbarGrabActive(0.48f, 0.48f, 0.48f, 1.00f);
            constexpr ImVec4 themeSliderGrab(0.45f, 0.45f, 0.45f, 1.00f);
            constexpr ImVec4 themeResizeGrip(0.30f, 0.30f, 0.30f, 1.00f);
            constexpr ImVec4 themeResizeGripHovered(0.40f, 0.40f, 0.40f, 1.00f);
            constexpr ImVec4 themeTransparent(0.00f, 0.00f, 0.00f, 0.00f);

            // Акцент Unity-синего: выделение, чекбоксы, активные элементы
            constexpr ImVec4 themeAccent(0.13f, 0.59f, 0.95f, 1.00f);
            constexpr ImVec4 themeAccentDark(0.13f, 0.47f, 0.75f, 1.00f);
            constexpr ImVec4 themeSelectionBg(0.13f, 0.47f, 0.75f, 0.55f);
            constexpr ImVec4 themeSeparatorHovered(0.13f, 0.59f, 0.95f, 0.78f);

            // ---------------------------------------------------------------
            // UI-шрифт: размер, системный дефолт, диалог выбора файла
            // ---------------------------------------------------------------
            constexpr float editorFontSizePx = 15.0f;
#ifdef _WIN32
            constexpr const char* defaultEditorFontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
#endif
            constexpr buint32 uiFontPathBufferSize = 512;
            // Фильтр OpenFileName: пары «описание\0маска\0», финальный \0\0
            constexpr const char fontFileFilter[] =
                "TrueType fonts (*.ttf)\0*.ttf\0All files (*.*)\0*.*\0";
            constexpr const char* loadFontDialogTitle = "Select UI Font";

            // ---------------------------------------------------------------
            // Меню-бар каркаса (File/Edit/Help) + диалог About
            // ---------------------------------------------------------------
            constexpr const char* menuFileLabel = "File";
            constexpr const char* menuFileExit = "Exit";
            constexpr const char* menuEditLabel = "Edit";
            constexpr const char* menuEditLoadFont = "Load Font...";
            constexpr const char* menuEditResetFont = "Reset Font";
            constexpr const char* menuHelpLabel = "Help";
            constexpr const char* menuHelpAbout = "About";
            constexpr const char* aboutPopupName = "About##EditorAboutPopup";
            constexpr const char* aboutText = "Stuff - game engine\nbeng editor framework, ImGui UI";
            constexpr const char* okButtonLabel = "OK";
            constexpr float aboutOkButtonWidth = 120.0f;

            // Окружность gizmo (режим Rotate): аппроксимация отрезками в
            // плоскости, натянутой на базисные векторы u/v (нормаль — ось
            // вращения). Сегменты — gizmoCircleSegments
            void addGizmoCircle(
                _In blib::graphics::LineRenderer& gizmo,
                _In const blib::math::Vector<float, 3>& center,
                _In const blib::math::Vector<float, 3>& basisU,
                _In const blib::math::Vector<float, 3>& basisV,
                _In const blib::graphics::Color& color)
            {
                const float angleStep = twoPi / static_cast<float>(gizmoCircleSegments);
                blib::math::Vector<float, 3> previous = center + basisU * gizmoAxisLength;
                for (buint32 i = 1; i <= gizmoCircleSegments; ++i)
                {
                    const float angle = angleStep * static_cast<float>(i);
                    const blib::math::Vector<float, 3> point = center +
                        basisU * (blib::math::cos(angle) * gizmoAxisLength) +
                        basisV * (blib::math::sin(angle) * gizmoAxisLength);
                    gizmo.addLine(previous, point, color);
                    previous = point;
                }
            }

            // Применение темы эдитора: Unity-подобная палитра, скругления,
            // плотность, hover/active-состояния. Вызывается один раз после
            // создания ImGui-контекста
            void applyEditorTheme()
            {
                ImGuiStyle& style = ImGui::GetStyle();

                // Скругления: окна/фреймы слегка, скроллбары — пилюли
                style.WindowRounding = themeWindowRounding;
                style.ChildRounding = themeChildRounding;
                style.FrameRounding = themeFrameRounding;
                style.PopupRounding = themePopupRounding;
                style.GrabRounding = themeGrabRounding;
                style.ScrollbarRounding = themeScrollbarRounding;
                style.TabRounding = themeTabRounding;

                // Плотность: «воздух» между элементами, компактные фреймы,
                // заголовки слева, меню-кнопка слева (как в Unity)
                style.WindowPadding = themeWindowPadding;
                style.FramePadding = themeFramePadding;
                style.ItemSpacing = themeItemSpacing;
                style.ItemInnerSpacing = themeItemInnerSpacing;
                style.IndentSpacing = themeIndentSpacing;
                style.ScrollbarSize = themeScrollbarSize;
                style.GrabMinSize = themeGrabMinSize;
                style.WindowTitleAlign = themeTitleAlign;
                style.WindowMenuButtonPosition = ImGuiDir_Left;

                // Бордеры: тонкие тёмные границы панелей
                style.WindowBorderSize = themeWindowBorderSize;
                style.ChildBorderSize = themeChildBorderSize;
                style.PopupBorderSize = themePopupBorderSize;
                style.FrameBorderSize = 0.0f;
                style.TabBorderSize = themeTabBorderSize;
                style.SeparatorTextBorderSize = 0.0f;

                // Тексты и фоны (многотонность серого: окно → панель →
                // заголовок → элемент читаются отдельными тонами)
                style.Colors[ImGuiCol_Text] = themeText;
                style.Colors[ImGuiCol_TextDisabled] = themeTextDisabled;
                style.Colors[ImGuiCol_WindowBg] = themeWindowBg;
                style.Colors[ImGuiCol_ChildBg] = themeChildBg;
                style.Colors[ImGuiCol_PopupBg] = themePopupBg;
                style.Colors[ImGuiCol_Border] = themeBorder;
                style.Colors[ImGuiCol_BorderShadow] = themeTransparent;
                style.Colors[ImGuiCol_FrameBg] = themeFrameBg;
                style.Colors[ImGuiCol_FrameBgHovered] = themeFrameHovered;
                style.Colors[ImGuiCol_FrameBgActive] = themeFrameActive;
                style.Colors[ImGuiCol_TitleBg] = themeTitleBg;
                style.Colors[ImGuiCol_TitleBgActive] = themeTitleBgActive;
                style.Colors[ImGuiCol_TitleBgCollapsed] = themeTitleBg;
                style.Colors[ImGuiCol_MenuBarBg] = themeMenuBarBg;

                // Скроллбары
                style.Colors[ImGuiCol_ScrollbarBg] = themeScrollbarBg;
                style.Colors[ImGuiCol_ScrollbarGrab] = themeScrollbarGrab;
                style.Colors[ImGuiCol_ScrollbarGrabHovered] = themeScrollbarGrabHovered;
                style.Colors[ImGuiCol_ScrollbarGrabActive] = themeScrollbarGrabActive;

                // Интерактивные элементы: серый в покое, светлее на hover,
                // темнее/синий в активном состоянии (обратная связь)
                style.Colors[ImGuiCol_CheckMark] = themeAccent;
                style.Colors[ImGuiCol_SliderGrab] = themeSliderGrab;
                style.Colors[ImGuiCol_SliderGrabActive] = themeAccent;
                style.Colors[ImGuiCol_Button] = themeElementBg;
                style.Colors[ImGuiCol_ButtonHovered] = themeElementHovered;
                style.Colors[ImGuiCol_ButtonActive] = themeElementActive;
                style.Colors[ImGuiCol_Header] = themeElementBg;
                style.Colors[ImGuiCol_HeaderHovered] = themeElementHovered;
                style.Colors[ImGuiCol_HeaderActive] = themeAccentDark;
                style.Colors[ImGuiCol_Separator] = themeBorder;
                style.Colors[ImGuiCol_SeparatorHovered] = themeSeparatorHovered;
                style.Colors[ImGuiCol_SeparatorActive] = themeAccent;
                style.Colors[ImGuiCol_ResizeGrip] = themeResizeGrip;
                style.Colors[ImGuiCol_ResizeGripHovered] = themeResizeGripHovered;
                style.Colors[ImGuiCol_ResizeGripActive] = themeAccent;
                style.Colors[ImGuiCol_Tab] = themeChildBg;
                style.Colors[ImGuiCol_TabHovered] = themeElementHovered;
                style.Colors[ImGuiCol_TabSelected] = themeElementBg;
                style.Colors[ImGuiCol_TabDimmed] = themeTitleBg;
                style.Colors[ImGuiCol_TabDimmedSelected] = themeTitleBgActive;
                style.Colors[ImGuiCol_TextSelectedBg] = themeSelectionBg;
                style.Colors[ImGuiCol_NavHighlight] = themeAccent;
            }

            // Перезагрузка UI-шрифта ImGui: ttfPath — путь к TTF-файлу;
            // nullptr/"" — дефолтный шрифт (системный Segoe UI, иначе
            // встроенный ProggyClean). Атлас пересобирается целиком:
            // бэкенд ImGui 1.92 с ImGuiBackendFlags_RendererHasTextures
            // пересоздаёт GL-текстуру сам (на следующем NewFrame).
            // true — запрошенный TTF загружен; false — подставлен дефолт
            bool reloadEditorFont(_In_opt const char* ttfPath)
            {
                ImGuiIO& io = ImGui::GetIO();
                ImFontAtlas* atlas = io.Fonts;

                const bool customRequested = (ttfPath != nullptr && ttfPath[0] != '\0');
                bool loadedRequested = false;

                atlas->Clear();

                ImFont* font = nullptr;
                if (customRequested)
                {
                    font = atlas->AddFontFromFileTTF(
                        ttfPath, editorFontSizePx, nullptr, atlas->GetGlyphRangesCyrillic());
                    loadedRequested = (font != nullptr);
                    if (font == nullptr)
                    {
                        __blib_log_warning("EditorApplication: cannot load UI font '%s'", ttfPath);
                    }
                }

#ifdef _WIN32
                if (font == nullptr)
                {
                    font = atlas->AddFontFromFileTTF(
                        defaultEditorFontPath, editorFontSizePx, nullptr, atlas->GetGlyphRangesCyrillic());
                    if (font == nullptr)
                    {
                        __blib_log_warning("EditorApplication: default UI font not found (%s)", defaultEditorFontPath);
                    }
                }
#endif

                if (font == nullptr)
                {
                    font = atlas->AddFontDefault();
                }

                io.FontDefault = font;
                // Build() вызывать нельзя: ImGui 1.92 с бэкендом
                // RendererHasTextures строит атлас сам (assert в
                // imgui_draw.cpp), текстуру пересоздаёт на NewFrame
                return loadedRequested;
            }
        }

        // ---------------------------------------------------------------
        // Внутренности каркаса: окно, рендер, ECS, панели, ImGui-состояние.
        // Полное определение скрыто в .cpp (pimpl) — заголовок не тянет
        // графические типы blib/beng в потребителей.
        // ---------------------------------------------------------------
        struct EditorApplication::EditorApplicationImpl
        {
            // Окно + рендер (порядок важен: окно создаёт GL-контекст,
            // рендер-таргет инициализирует GL-функции в нём)
            blib::graphics::RenderWindow window;
            blib::graphics::IRenderTarget renderTarget;
            blib::graphics::OrbitCamera camera;

            // ECS: сцена + движковые системы (порядок полей важен:
            // сцена объявлена после окна/таргета — разрушается раньше
            // них, GL-контекст на момент выгрузки ресурсов жив;
            // Scene хранит сырой указатель на системы — системы
            // объявлены после сцены и разрушаются раньше)
            beng::Scene scene;
            beng::Time time;
            beng::TransformSystem transformSystem;
            beng::AnimationSystem animationSystem;
            beng::RenderSystem renderSystem;

            // Панели каркаса
            beng::editor::ViewportPanel viewportPanel;
            beng::editor::ConsolePanel consolePanel;

            // Сценные панели эдитора (Hierarchy + Inspector через
            // рефлексию — см. componentReflection.h). Регистрируются
            // каркасом в зоны (если scenePanelsEnabled): единый эдитор
            // правит сцену любой игры без панелей от хоста
            beng::editor::SceneHierarchyPanel sceneHierarchyPanel;
            beng::editor::InspectorPanel inspectorPanel;

            // Выбранная сущность сцены (selection эдитора): пишет
            // SceneHierarchyPanel, читают Inspector/gizmo хостов.
            // Устаревший выбор сбрасывает панель иерархии
            EntityID selectedEntity;

            // История команд эдитора (undo/redo): правки полей, gizmo,
            // сущности/компоненты. Объявлена ПОСЛЕ сцены — разрушается
            // РАНЬШЕ неё: команды уничтожаются при живой сцене
            CommandHistory commandHistory;

            // Режим gizmo-манипулятора (W/E/R)
            GizmoMode gizmoMode;

            // Отрисовка gizmo (стрелки/окружности). LineRenderer не
            // освобождает GL-ресурсы — живёт до гашения каркаса
            blib::graphics::LineRenderer gizmoRenderer;

            // Состояние активного драга за стрелку/окружность
            struct GizmoDragState
            {
                bool active;
                buint8 axis;        // 0=X, 1=Y, 2=Z
                EntityID entity;
                TransformSnapshot startTrs;
            } gizmoDrag;

            // Горячая ось gizmo (подсветка; gizmoNoAxis — нет)
            buint8 gizmoHotAxis;

            // В этом кадре начался драг за стрелку — клик не пикает
            bool gizmoDragBeganThisFrame;

            // Редакторский ввод включён (выключается на время PIE —
            // см. setEditorInputEnabled)
            bool editorInputEnabled;

            // Панели хоста: регистрируются через registerPanel
            // (указатели — каркас панелями не владеет, см. IPanel)
            struct RegisteredPanel
            {
                IPanel* panel;
                PanelZone zone;
            };
            blib::memory::Allocator panelListAllocator;
            std::vector<RegisteredPanel, blib::memory::StdAllocatorAdapter<RegisteredPanel>> panels;

            // Заголовок окна (НЕ владеет строкой: вызывающий обязан
            // держать её живой всё время жизни приложения — на
            // практике это constexpr-литерал)
            const char* windowTitle;

            // Консоль
            bool showConsole;

            // Меню-бар каркаса: флаг модального диалога About (рисуется
            // в конце UI-кадра, поверх всех окон)
            bool showAboutPopup;

            // Путь активного UI-шрифта (пустая строка — дефолтный).
            // Сессионная настройка: не персистится (io.IniFilename = nullptr)
            char uiFontPath[uiFontPathBufferSize];

            // Отложенный ресайз FBO под размер вьюпорт-панели (0 = нет):
            // размер измеряется в ImGui-кадре, а сцена рендерится раньше —
            // применяем в начале следующего кадра
            buint32 pendingViewportWidth;
            buint32 pendingViewportHeight;

            EditorApplicationImpl(_In uint16_t width, _In uint16_t height, _In const char* title)
                : window(width, height, title)
                , renderTarget(width, height)
                , camera()
                , scene()
                , time()
                , transformSystem()
                , animationSystem()
                , renderSystem()
                , viewportPanel()
                , consolePanel()
                , sceneHierarchyPanel()
                , inspectorPanel()
                , selectedEntity(beng::invalidEntity)
                , commandHistory()
                , gizmoMode(GizmoMode::Translate)
                , gizmoRenderer()
                , gizmoDrag{ false, gizmoNoAxis, invalidEntity, TransformSnapshot() }
                , gizmoHotAxis(gizmoNoAxis)
                , gizmoDragBeganThisFrame(false)
                , editorInputEnabled(true)
                , panelListAllocator()
                , panels(blib::memory::StdAllocatorAdapter<RegisteredPanel>(&this->panelListAllocator))
                , windowTitle(title)
                , showConsole(false)
                , showAboutPopup(false)
                , uiFontPath{}
                , pendingViewportWidth(0)
                , pendingViewportHeight(0)
            {
            }
        };

        // ---------------------------------------------------------------
        // Хуки по умолчанию: пустое приложение (эдитор без игры) —
        // пустая сцена, движковые типы, без специфики
        // ---------------------------------------------------------------
        void EditorApplication::onInitialize(_In Scene& scene)
        {
            (void)scene;
        }

        void EditorApplication::onInput()
        {
        }

        void EditorApplication::onSceneWillUpdate(float deltaTime)
        {
            (void)deltaTime;
        }

        void EditorApplication::onSceneDidUpdate(float deltaTime)
        {
            (void)deltaTime;
        }

        void EditorApplication::onUi()
        {
        }

        bool EditorApplication::onEscapePressed()
        {
            return false;
        }

        void EditorApplication::onViewportClick(
            _In const blib::math::Vector<float, 3>& rayOrigin,
            _In const blib::math::Vector<float, 3>& rayDirection)
        {
            // По умолчанию клик никого не выбирает (эдитор без игры
            // пикать нечего — пустая сцена). Хосты переопределяют:
            // ray-picking по своим типам + selectEntity
            (void)rayOrigin;
            (void)rayDirection;
        }

        EditorApplication::EditorApplication()
            : impl(nullptr)
            , scenePanelsEnabled(true)
        {
        }

        EditorApplication::~EditorApplication()
        {
            // Страховка: если владелец не вызвал shutdown явно
            this->shutdown();
        }

        bool EditorApplication::initialize(_In uint16_t windowWidth, _In uint16_t windowHeight, _In const char* windowTitle)
        {
            if (__blib_unlikely(this->impl != nullptr))
            {
                __blib_log_warning("EditorApplication: initialize() called twice");
                return false;
            }

            auto& globalAllocator = blib::memory::GlobalAllocator::instance();

            // Аллокация через GlobalAllocator + placement new (проектное
            // правило: выделяющие new/delete запрещены)
            this->impl = static_cast<EditorApplicationImpl*>(globalAllocator.allocate(sizeof(EditorApplicationImpl)));
            new (this->impl) EditorApplicationImpl(windowWidth, windowHeight, windowTitle);

            // Камера: орбита вокруг начала координат
            this->impl->camera.setPerspective(
                blib::math::AngleDegreef(cameraFovDegrees),
                static_cast<float>(windowWidth) / static_cast<float>(windowHeight),
                cameraNearDistance,
                cameraFarDistance);
            this->impl->camera.setTarget(blib::graphics::Vector3f(cameraTargetX, cameraTargetY, cameraTargetZ));
            this->impl->camera.setDistance(cameraInitialDistance);
            this->impl->camera.update();
            this->impl->renderTarget.rc.setCamera(&this->impl->camera);

            // ECS: движковые типы компонентов и системы.
            // Системы живут в impl (Scene ими не владеет).
            // TransformComponent регистрируется сценой автоматически
            // (инвариант) — явная регистрация запрещена. Движковые
            // рендер-типы beng-client регистрирует каркас: сцены
            // любой игры в эдиторе опираются на них; игровые типы
            // (gravelands.*) регистрирует хост в onInitialize
            this->impl->scene.registerComponentType<beng::SkinnedMeshComponent>();
            this->impl->scene.registerComponentType<beng::AnimatorComponent>();
            this->impl->scene.registerComponentType<beng::MeshRenderComponent>();
            this->impl->scene.registerComponentType<beng::DirectionalLightComponent>();
            this->impl->scene.registerComponentType<beng::AmbientLightComponent>();
            this->impl->scene.registerComponentType<beng::BlobShadowComponent>();

            this->impl->scene.addSystem(&this->impl->transformSystem);
            this->impl->scene.addSystem(&this->impl->animationSystem);
            this->impl->scene.addSystem(&this->impl->renderSystem);

            this->impl->renderSystem.setRenderTarget(&this->impl->renderTarget);

            // Панели каркаса
            this->impl->viewportPanel.setRenderTarget(&this->impl->renderTarget);
            this->impl->viewportPanel.setCamera(&this->impl->camera);

            // Сценные панели единого эдитора (Hierarchy + Inspector):
            // каркас регистрирует их сам — они игра-агностичны (работают
            // через рефлексию). Выбор живёт в каркасе (selectedEntity):
            // Hierarchy пишет, Inspector читает, хост рисует gizmo по
            // getSelectedEntity(). Инструменты со своими панелями в тех
            // же зонах (вьювер) выключают их setScenePanelsEnabled(false)
            if (this->scenePanelsEnabled)
            {
                this->impl->sceneHierarchyPanel.setScene(&this->impl->scene);
                this->impl->sceneHierarchyPanel.setSelectionRef(&this->impl->selectedEntity);
                this->impl->sceneHierarchyPanel.setCommandHistory(&this->impl->commandHistory);
                this->impl->inspectorPanel.setScene(&this->impl->scene);
                this->impl->inspectorPanel.setSelectionSource(&this->impl->selectedEntity);
                this->impl->inspectorPanel.setCommandHistory(&this->impl->commandHistory);

                this->impl->panels.push_back(EditorApplicationImpl::RegisteredPanel{
                    &this->impl->sceneHierarchyPanel, PanelZone::LeftTop });
                this->impl->panels.push_back(EditorApplicationImpl::RegisteredPanel{
                    &this->impl->inspectorPanel, PanelZone::Right });
            }

            // Камера вьюпорта не должна захватываться кликом по стрелке
            // gizmo: вращение блокируется, пока курсор над осью/окружностью
            // или идёт драг манипулятора
            this->impl->viewportPanel.setRotationBlockPredicate([this]() -> bool {
                return this->impl->gizmoDrag.active || this->impl->gizmoHotAxis != gizmoNoAxis;
            });

            // ImGui + WndProc-хук
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            // Не писать imgui.ini в рабочую директорию (мусорит в корне
            // репозитория при запуске из студии)
            io.IniFilename = nullptr;

            // Тема эдитора (Unity-подобная палитра, скругления,
            // плотность) + UI-шрифт (Segoe UI, встроенный — fallback)
            applyEditorTheme();
            reloadEditorFont(nullptr);

            HWND hwnd = __blib_render_window_context(this->impl->window.__getCtx())->hwnd;
            ImGui_ImplWin32_Init(hwnd);
            ImGui_ImplOpenGL3_Init();
            s_engineWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(editorImguiWndProc)));

            // Команда clear: чистит и буфер ядра, и скроллбэк окна консоли.
            // Коллбэк захватывает каркас по this — вызовы происходят
            // только внутри главного цикла, пока impl жив
            blib::console::Console::instance().registerCommand(
                clearCommandName, clearCommandHelp,
                [this](const std::vector<std::string>&)
                {
                    this->impl->consolePanel.clearDisplay();
                });

            // Хук хоста: свои типы, системы, панели
            this->onInitialize(this->impl->scene);

            __blib_log_info("%s initialized (%ux%u window)", windowTitle, windowWidth, windowHeight);
            return true;
        }

        void EditorApplication::tick()
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                return;
            }

            // Переменный dt кадра (таймстеп клиентской стороны;
            // см. ARCHITECTURE.md)
            this->impl->time.tick();
            const float deltaTime = this->impl->time.getDeltaTime();

            // Оконные сообщения + клавиатура
            this->impl->window.update();
            blib::graphics::Keyboard::update();

            // Горячие клавиши каркаса
            if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Grave))
            {
                // `~` открывает/закрывает консоль (Quake-стиль)
                this->impl->showConsole = !this->impl->showConsole;
                if (this->impl->showConsole)
                {
                    this->impl->consolePanel.requestFocus();
                }
            }
            if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Escape))
            {
                if (this->impl->showConsole)
                {
                    this->impl->showConsole = false;
                }
                else if (!this->onEscapePressed())
                {
                    this->impl->window.close();
                    return;
                }
            }

            // Горячие клавиши эдитора — только при закрытой консоли
            // (печать в консоли не должна «протекать» в правки) и при
            // включённом редакторском вводе (PIE выключает: W/E/R
            // конфликтуют с вводом игры в клиентском окне)
            if (!this->impl->showConsole && this->impl->editorInputEnabled)
            {
                // W/E/R — режимы gizmo (как в Unity/Unreal)
                if (blib::graphics::Keyboard::isKeyJustPressed(gizmoTranslateKey))
                {
                    this->impl->gizmoMode = GizmoMode::Translate;
                }
                if (blib::graphics::Keyboard::isKeyJustPressed(gizmoRotateKey))
                {
                    this->impl->gizmoMode = GizmoMode::Rotate;
                }
                if (blib::graphics::Keyboard::isKeyJustPressed(gizmoScaleKey))
                {
                    this->impl->gizmoMode = GizmoMode::Scale;
                }

                const bool ctrlPressed =
                    blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LControl) ||
                    blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::RControl);
                const bool shiftPressed =
                    blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LShift) ||
                    blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::RShift);

                // Ctrl+Z — отмена, Ctrl+Shift+Z — повтор
                if (ctrlPressed && blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Z))
                {
                    if (shiftPressed)
                    {
                        this->impl->commandHistory.redo();
                    }
                    else
                    {
                        this->impl->commandHistory.undo();
                    }
                }

                // Delete — удалить выбранную сущность (через историю)
                if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Delete) &&
                    this->impl->selectedEntity != invalidEntity)
                {
                    if (this->impl->commandHistory.recordEntityDestroy(
                        this->impl->scene, this->impl->selectedEntity))
                    {
                        this->impl->selectedEntity = invalidEntity;
                    }
                }
            }

            // Свои горячие клавиши хоста (до симуляции)
            this->onInput();

            // Отложенный ресайз FBO под размер вьюпорт-панели (измерен
            // в прошлом ImGui-кадре — сцена рендерится раньше UI).
            // Вместе с FBO пересчитываем перспективу: её аспект должен
            // совпадать с аспектом вьюпорта, иначе картинка растянется
            if (this->impl->pendingViewportWidth > 0 && this->impl->pendingViewportHeight > 0)
            {
                this->impl->renderTarget.resize(this->impl->pendingViewportWidth, this->impl->pendingViewportHeight);
                this->impl->camera.setPerspective(
                    blib::math::AngleDegreef(cameraFovDegrees),
                    static_cast<float>(this->impl->pendingViewportWidth) / static_cast<float>(this->impl->pendingViewportHeight),
                    cameraNearDistance,
                    cameraFarDistance);
                __blib_log_info("viewport resized to %ux%u",
                    this->impl->pendingViewportWidth, this->impl->pendingViewportHeight);
                this->impl->pendingViewportWidth = 0;
                this->impl->pendingViewportHeight = 0;
            }

            // Симуляция + рендер сцены в FBO (RenderSystem внутри update)
            this->onSceneWillUpdate(deltaTime);

            // Хост мог тикнуть ВЛОЖЕННОЕ окно с другим GL-контекстом
            // (PIE: клиентское окно игры в onSceneWillUpdate) — вернуть
            // контекст эдитора до отрисовки (multi-window контракт, см.
            // GRAPHICS.md «Владение GL»)
            this->impl->window.makeCurrent();

            this->impl->renderTarget.clear(blib::graphics::Color::Black);
            this->impl->scene.update(deltaTime);

            // Отладочные слои хоста поверх сцены (в тот же FBO)
            this->onSceneDidUpdate(deltaTime);

            // Gizmo выбранной сущности — поверх сцены и слоёв хоста
            this->drawGizmo();

            // UI: переключаемся на back-буфер (иначе ImGui-бэкенд
            // рисует в FBO, а вьюпорт сэмплит его же — feedback loop).
            // Фон — цвет окна темы: скруглённые углы панелей «впиваются»
            // в подложку без чёрных щелей на стыках
            this->impl->renderTarget.rc.api.ogl.ext.__blib_gl_glBindFramebuffer(GL_FRAMEBUFFER, 0);
            this->impl->renderTarget.rc.api.ogl.__blib_glClearColor(
                themeWindowBg.x, themeWindowBg.y, themeWindowBg.z, themeWindowBg.w);
            this->impl->renderTarget.rc.api.ogl.__blib_gl_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Раскладка: зоны хоста
            {
                const float windowW = static_cast<float>(this->impl->window.getWight());
                const float windowH = static_cast<float>(this->impl->window.getHeight());
                const float hierarchyHeight = windowH * hierarchyHeightFraction;
                // Верх окна: меню-бар каркаса (высота = высоте фрейма
                // текущего шрифта), под ним — верхняя полоса хоста,
                // ещё ниже — панели и вьюпорт
                const float menuBarOffset = ImGui::GetFrameHeight();
                const float topStripOffset = menuBarOffset + topBarHeight;

                for (const EditorApplicationImpl::RegisteredPanel& registered : this->impl->panels)
                {
                    switch (registered.zone)
                    {
                        case PanelZone::LeftTop:
                            ImGui::SetNextWindowPos(ImVec2(0.0f, topStripOffset), ImGuiCond_FirstUseEver);
                            ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, hierarchyHeight - topStripOffset), ImGuiCond_FirstUseEver);
                            break;
                        case PanelZone::LeftBottom:
                            ImGui::SetNextWindowPos(ImVec2(0.0f, hierarchyHeight), ImGuiCond_FirstUseEver);
                            ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, windowH - hierarchyHeight), ImGuiCond_FirstUseEver);
                            break;
                        case PanelZone::Right:
                            ImGui::SetNextWindowPos(ImVec2(windowW - rightPanelWidth, topStripOffset), ImGuiCond_FirstUseEver);
                            ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, windowH - topStripOffset), ImGuiCond_FirstUseEver);
                            break;
                    }
                    registered.panel->draw();
                }

                // Свои ImGui-окна хоста: верхняя полоса (позицию/
                // размер задаёт каркас — полоса зарезервирована за
                // хостом; она плоская: примыкает к меню-бару и краю
                // окна — скругления дали бы щели), модальные диалоги
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::SetNextWindowPos(ImVec2(0.0f, menuBarOffset), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(windowW, topBarHeight), ImGuiCond_FirstUseEver);
                this->onUi();
                ImGui::PopStyleVar();

                // Центр: вьюпорт
                ImGui::SetNextWindowPos(ImVec2(leftPanelWidth, topStripOffset), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(windowW - leftPanelWidth - rightPanelWidth, windowH - topStripOffset), ImGuiCond_FirstUseEver);
                this->impl->viewportPanel.draw();

                // Gizmo-манипулятор: драг за стрелку/окружность (режим
                // W/E/R) — после панели, пока io.MouseDelta свежий
                this->updateGizmoManipulator();

                // Клик ЛКМ по вьюпорту (не драг и не по стрелке gizmo) —
                // ray-picking: каркас строит луч из камеры через точку
                // клика, хост решает, кого задел клик (onViewportClick)
                {
                    float clickNdcX = 0.0f;
                    float clickNdcY = 0.0f;
                    if (this->impl->viewportPanel.takeViewportClick(clickNdcX, clickNdcY) &&
                        !this->impl->gizmoDragBeganThisFrame)
                    {
                        blib::math::Vector<float, 3> rayOrigin;
                        blib::math::Vector<float, 3> rayDirection;
                        if (this->computeViewportRay(clickNdcX, clickNdcY, rayOrigin, rayDirection))
                        {
                            this->onViewportClick(rayOrigin, rayDirection);
                        }
                    }
                }

                // Консоль поверх всего
                if (this->impl->showConsole)
                {
                    ImGui::SetNextWindowPos(ImVec2(0.0f, windowH - consoleHeight), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowSize(ImVec2(consoleWidth, consoleHeight), ImGuiCond_FirstUseEver);
                    this->impl->consolePanel.draw();
                }

                // Меню-бар каркаса (File/Edit/Help) + диалог About —
                // рисуется последним: попап About обязан лечь ПОВЕРХ
                // консоли и всех панелей
                this->drawMainMenuBar();
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            // Вьюпорт-панель измерила доступный размер в этом кадре —
            // если он отличается от текущего FBO, планируем ресайз на
            // следующий кадр (применение — в начале tick, см. выше)
            {
                const float viewportW = this->impl->viewportPanel.getLastViewportWidth();
                const float viewportH = this->impl->viewportPanel.getLastViewportHeight();
                if (viewportW >= minViewportDimension && viewportH >= minViewportDimension)
                {
                    const auto& rtCtx = this->impl->renderTarget.getContext();
                    const buint32 desiredWidth = static_cast<buint32>(viewportW);
                    const buint32 desiredHeight = static_cast<buint32>(viewportH);
                    if (desiredWidth != rtCtx.viewportWidth || desiredHeight != rtCtx.viewportHeight)
                    {
                        this->impl->pendingViewportWidth = desiredWidth;
                        this->impl->pendingViewportHeight = desiredHeight;
                    }
                }
            }

            // Презентация без блита FBO: сцена уже показана во вьюпорте,
            // UI отрисован поверх back-буфера
            this->impl->window.swapBuffers();
        }

        void EditorApplication::drawMainMenuBar()
        {
            // Меню-бар каркаса: File (выход), Edit (UI-шрифт), Help
            // (About). Полоса занимает весь верх окна (позицию/высоту
            // задаёт BeginMainMenuBar; раскладка панелей учитывает
            // высоту фрейма текущего шрифта — см. tick)
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu(menuFileLabel))
                {
                    if (ImGui::MenuItem(menuFileExit))
                    {
                        this->impl->window.close();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu(menuEditLabel))
                {
                    if (ImGui::MenuItem(menuEditLoadFont))
                    {
                        // Win32-диалог выбора TTF. Блокирует кадр до
                        // закрытия — штатно для прототипа (прецедент:
                        // файлдиалог вьювера, см. MODEL_VIEWER.md)
                        char pathBuffer[uiFontPathBufferSize] = "";
                        OPENFILENAMEA ofn;
                        ZeroMemory(&ofn, sizeof(ofn));
                        ofn.lStructSize = sizeof(ofn);
                        ofn.hwndOwner = __blib_render_window_context(this->impl->window.__getCtx())->hwnd;
                        ofn.lpstrFilter = fontFileFilter;
                        ofn.lpstrFile = pathBuffer;
                        ofn.nMaxFile = uiFontPathBufferSize;
                        ofn.lpstrTitle = loadFontDialogTitle;
                        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

                        if (GetOpenFileNameA(&ofn) && reloadEditorFont(pathBuffer))
                        {
                            strncpy_s(this->impl->uiFontPath, pathBuffer, _TRUNCATE);
                            __blib_log_info("UI font loaded: %s", pathBuffer);
                        }
                    }
                    if (ImGui::MenuItem(menuEditResetFont))
                    {
                        this->impl->uiFontPath[0] = '\0';
                        reloadEditorFont(nullptr);
                        __blib_log_info("UI font reset to default");
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu(menuHelpLabel))
                {
                    if (ImGui::MenuItem(menuHelpAbout))
                    {
                        this->impl->showAboutPopup = true;
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            // Диалог About: модальный попап по центру, поверх всех окон
            // (рисуется после консоли — см. tick)
            if (this->impl->showAboutPopup)
            {
                ImGui::OpenPopup(aboutPopupName);
            }
            if (ImGui::BeginPopupModal(aboutPopupName, &this->impl->showAboutPopup, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("%s", aboutText);
                ImGui::Spacing();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - aboutOkButtonWidth);
                if (ImGui::Button(okButtonLabel, ImVec2(aboutOkButtonWidth, 0.0f)))
                {
                    ImGui::CloseCurrentPopup();
                    this->impl->showAboutPopup = false;
                }
                ImGui::EndPopup();
            }
        }

        bool EditorApplication::isRunning() const
        {
            return this->impl != nullptr && this->impl->window.isOpen();
        }

        void EditorApplication::shutdown()
        {
            if (this->impl == nullptr)
            {
                return;
            }

            // Вернуть оригинальную оконную процедуру до гашения ImGui
            HWND hwnd = __blib_render_window_context(this->impl->window.__getCtx())->hwnd;
            if (s_engineWndProc != nullptr)
            {
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_engineWndProc));
                s_engineWndProc = nullptr;
            }

            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            // Заголовок для прощального лога — до разрушения impl
            const char* title = this->impl->windowTitle;

            // Явный деструктор + возврат памяти глобальному аллокатору
            this->impl->~EditorApplicationImpl();
            blib::memory::GlobalAllocator::instance().deallocate(this->impl, sizeof(EditorApplicationImpl));
            this->impl = nullptr;

            __blib_log_info("%s shut down", title);
        }

        void EditorApplication::registerPanel(_In IPanel* panel, _In PanelZone zone)
        {
            if (__blib_unlikely(this->impl == nullptr || panel == nullptr))
            {
                __blib_log_warning("EditorApplication: registerPanel() outside of initialize() or null panel");
                return;
            }

            this->impl->panels.push_back(EditorApplicationImpl::RegisteredPanel{ panel, zone });
        }

        bool EditorApplication::setScenePanelsEnabled(bool enabled)
        {
            // Флаг применяется при инициализации: после initialize()
            // сценные панели уже зарегистрированы в раскладку
            if (__blib_unlikely(this->impl != nullptr))
            {
                __blib_log_warning("EditorApplication: setScenePanelsEnabled() must be called before initialize()");
                return false;
            }

            this->scenePanelsEnabled = enabled;
            return true;
        }

        EntityID EditorApplication::getSelectedEntity() const
        {
            return (this->impl != nullptr) ? this->impl->selectedEntity : invalidEntity;
        }

        void EditorApplication::selectEntity(EntityID entity)
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                return;
            }
            this->impl->selectedEntity = entity;
        }

        bool EditorApplication::computeViewportRay(
            float ndcX, float ndcY,
            _Out blib::math::Vector<float, 3>& outOrigin,
            _Out blib::math::Vector<float, 3>& outDirection) const
        {
            // Луч строится из проекционной матрицы орбитальной камеры
            // (column-major, стандартная перспектива GL):
            // data[1][1] = f = 1/tan(fovY/2), data[0][0] = f/aspect.
            // Направление = forward + right*(u*tanHalf*aspect) +
            // up*(v*tanHalf) в базисе камеры (без обратных матриц —
            // OrbitCamera хранит углы/позицию, forward/up считаются
            // из них; right = cross(forward, worldUp))
            const blib::graphics::TransformMatrix& projection = this->impl->camera.getProjectionMatrix();
            const float f = projection.data[1][1];
            if (__blib_unlikely(f == 0.0f))
            {
                return false;
            }
            const float tanHalfFov = 1.0f / f;
            const float aspect = f / projection.data[0][0];

            blib::math::Vector<float, 3> forward =
                this->impl->camera.getTarget() - this->impl->camera.getPosition();
            forward = blib::math::normalize(forward);

            const blib::math::Vector<float, 3> worldUp(0.0f, 1.0f, 0.0f);
            const blib::math::Vector<float, 3> right =
                blib::math::normalize(blib::math::cross(forward, worldUp));
            const blib::math::Vector<float, 3> up = blib::math::cross(right, forward);

            outOrigin = this->impl->camera.getPosition();
            outDirection = blib::math::normalize(
                forward +
                right * (ndcX * tanHalfFov * aspect) +
                up * (ndcY * tanHalfFov));
            return true;
        }

        GizmoMode EditorApplication::getGizmoMode() const
        {
            return (this->impl != nullptr) ? this->impl->gizmoMode : GizmoMode::Translate;
        }

        void EditorApplication::setGizmoMode(GizmoMode mode)
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                return;
            }
            this->impl->gizmoMode = mode;
        }

        void EditorApplication::setEditorInputEnabled(bool enabled)
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                return;
            }
            this->impl->editorInputEnabled = enabled;
        }

        CommandHistory& EditorApplication::getCommandHistory()
        {
            return this->impl->commandHistory;
        }

        float EditorApplication::gizmoRayCircleDistance(
            _In const blib::math::Vector<float, 3>& rayOrigin,
            _In const blib::math::Vector<float, 3>& rayDirection,
            _In const blib::math::Vector<float, 3>& circleCenter,
            _In const blib::math::Vector<float, 3>& circleNormal,
            float circleRadius) const
        {
            // Пересечение луча с плоскостью окружности; расстояние до
            // окружности — |расстояние от центра до точки - радиус|.
            // Нормаль — нормированная ось вращения окружности
            const float denominator = blib::math::dot(rayDirection, circleNormal);
            constexpr float circleEpsilon = 0.000001f;
            if (denominator > -circleEpsilon && denominator < circleEpsilon)
            {
                return gizmoNoHitDistance; // луч параллелен плоскости
            }

            const float t = blib::math::dot(circleCenter - rayOrigin, circleNormal) / denominator;
            if (t < 0.0f)
            {
                return gizmoNoHitDistance; // позади камеры
            }

            const blib::math::Vector<float, 3> intersection = rayOrigin + rayDirection * t;
            const blib::math::Vector<float, 3> toCenter = intersection - circleCenter;
            const float radialDistance = blib::math::length(toCenter);
            if (radialDistance < circleEpsilon)
            {
                return gizmoNoHitDistance; // точка в центре — не на окружности
            }

            const float distance = radialDistance - circleRadius;
            return (distance < 0.0f) ? -distance : distance;
        }

        bool EditorApplication::hitTestGizmo(
            _In const blib::math::Vector<float, 3>& rayOrigin,
            _In const blib::math::Vector<float, 3>& rayDirection,
            _In const blib::math::Vector<float, 3>& gizmoPosition,
            _Out buint8& outAxis) const
        {
            // Порог близости в мировых единицах (масштабируется дистанцией)
            const float threshold =
                gizmoAxisHitPixelThreshold * this->impl->camera.getDistance() * gizmoPanScalePerPixel;

            const blib::math::Vector<float, 3> axisX(1.0f, 0.0f, 0.0f);
            const blib::math::Vector<float, 3> axisY(0.0f, 1.0f, 0.0f);
            const blib::math::Vector<float, 3> axisZ(0.0f, 0.0f, 1.0f);

            float distanceX = gizmoNoHitDistance;
            float distanceY = gizmoNoHitDistance;
            float distanceZ = gizmoNoHitDistance;

            if (this->impl->gizmoMode == GizmoMode::Rotate)
            {
                // Окружности в плоскостях, перпендикулярных осям
                distanceX = this->gizmoRayCircleDistance(rayOrigin, rayDirection, gizmoPosition, axisX, gizmoAxisLength);
                distanceY = this->gizmoRayCircleDistance(rayOrigin, rayDirection, gizmoPosition, axisY, gizmoAxisLength);
                distanceZ = this->gizmoRayCircleDistance(rayOrigin, rayDirection, gizmoPosition, axisZ, gizmoAxisLength);
            }
            else
            {
                // Стрелки (Translate/Scale)
                distanceX = this->gizmoRayAxisDistance(rayOrigin, rayDirection, gizmoPosition, axisX);
                distanceY = this->gizmoRayAxisDistance(rayOrigin, rayDirection, gizmoPosition, axisY);
                distanceZ = this->gizmoRayAxisDistance(rayOrigin, rayDirection, gizmoPosition, axisZ);
            }

            // Ближайшая ось в пределах порога
            const float bestDistance =
                (distanceX < distanceY) ? ((distanceX < distanceZ) ? distanceX : distanceZ)
                                        : ((distanceY < distanceZ) ? distanceY : distanceZ);
            if (bestDistance > threshold)
            {
                return false;
            }

            if (distanceX <= bestDistance + 0.0001f && distanceX <= threshold)
            {
                outAxis = 0;
            }
            else if (distanceY <= threshold)
            {
                outAxis = 1;
            }
            else
            {
                outAxis = 2;
            }
            return true;
        }

        void EditorApplication::updateGizmoManipulator()
        {
            const EntityID selected = this->impl->selectedEntity;
            this->impl->gizmoDragBeganThisFrame = false;

            if (__blib_unlikely(selected == invalidEntity))
            {
                this->impl->gizmoHotAxis = gizmoNoAxis;
                this->impl->gizmoDrag.active = false;
                return;
            }

            beng::TransformComponent* transform =
                this->impl->scene.tryGetComponent<beng::TransformComponent>(selected);
            if (__blib_unlikely(transform == nullptr))
            {
                this->impl->gizmoHotAxis = gizmoNoAxis;
                this->impl->gizmoDrag.active = false;
                return;
            }

            // Луч мыши: из камеры через текущий курсор
            float ndcX = 0.0f;
            float ndcY = 0.0f;
            this->impl->viewportPanel.getCursorNdc(ndcX, ndcY);
            blib::math::Vector<float, 3> rayOrigin;
            blib::math::Vector<float, 3> rayDirection;
            if (!this->computeViewportRay(ndcX, ndcY, rayOrigin, rayDirection))
            {
                return;
            }

            const blib::math::Vector<float, 3> gizmoPosition = transform->getWorldPosition();

            if (!this->impl->gizmoDrag.active)
            {
                // Подсветка + захват драга за стрелку/окружность
                buint8 hitAxis = gizmoNoAxis;
                const bool hit = this->impl->viewportPanel.isCursorOverViewport() &&
                    this->hitTestGizmo(rayOrigin, rayDirection, gizmoPosition, hitAxis);
                this->impl->gizmoHotAxis = hit ? hitAxis : gizmoNoAxis;

                if (hit && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    this->impl->gizmoDrag.active = true;
                    this->impl->gizmoDrag.axis = hitAxis;
                    this->impl->gizmoDrag.entity = selected;
                    this->impl->gizmoDrag.startTrs.position = transform->getLocalPosition();
                    this->impl->gizmoDrag.startTrs.rotation = transform->getLocalRotation();
                    this->impl->gizmoDrag.startTrs.scale = transform->getLocalScale();
                    // Клик ушёл манипулятору — pick в этом кадре не выполнять
                    this->impl->gizmoDragBeganThisFrame = true;
                }
                return;
            }

            // Драг активен
            this->impl->gizmoHotAxis = this->impl->gizmoDrag.axis;

            ImGuiIO& io = ImGui::GetIO();
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                // Конец драга: снимок нового TRS + команда в историю
                TransformSnapshot newTrs;
                newTrs.position = transform->getLocalPosition();
                newTrs.rotation = transform->getLocalRotation();
                newTrs.scale = transform->getLocalScale();
                this->impl->commandHistory.recordTransformChange(
                    this->impl->scene, selected, this->impl->gizmoDrag.startTrs, newTrs);
                this->impl->gizmoDrag.active = false;
                return;
            }

            // Смещение в горизонтальной плоскости взгляда камеры
            blib::math::Vector<float, 3> lookDirection =
                this->impl->camera.getTarget() - this->impl->camera.getPosition();
            lookDirection.y = 0.0f;
            lookDirection = blib::math::normalize(lookDirection);
            const blib::math::Vector<float, 3> worldUp(0.0f, 1.0f, 0.0f);
            const blib::math::Vector<float, 3> right = blib::math::normalize(
                blib::math::cross(lookDirection, worldUp));

            const float panScale = this->impl->camera.getDistance() * gizmoPanScalePerPixel;
            const blib::math::Vector<float, 3> planeDelta =
                right * (io.MouseDelta.x * panScale) +
                lookDirection * (-io.MouseDelta.y * panScale);

            const buint8 axis = this->impl->gizmoDrag.axis;

            switch (this->impl->gizmoMode)
            {
                case GizmoMode::Translate:
                {
                    // Движение вдоль активной оси: X/Z — проекция
                    // плоскостного смещения, Y — экранное dy
                    const blib::math::Vector<float, 3> position = transform->getLocalPosition();
                    if (axis == 0)
                    {
                        transform->setLocalPosition(position + blib::math::Vector<float, 3>(planeDelta.x, 0.0f, 0.0f));
                    }
                    else if (axis == 1)
                    {
                        transform->setLocalPosition(position + blib::math::Vector<float, 3>(0.0f, -io.MouseDelta.y * panScale, 0.0f));
                    }
                    else
                    {
                        transform->setLocalPosition(position + blib::math::Vector<float, 3>(0.0f, 0.0f, planeDelta.z));
                    }
                    break;
                }

                case GizmoMode::Rotate:
                {
                    // Вращение вокруг активной оси от экранного драга.
                    // Знаки: ось Y — вправо = +, X/Z — вверх = + (MVP-подбор)
                    float angle = 0.0f;
                    if (axis == 0)
                    {
                        angle = -io.MouseDelta.y * gizmoRotateSpeedDegPerPixel;
                    }
                    else if (axis == 1)
                    {
                        angle = io.MouseDelta.x * gizmoRotateSpeedDegPerPixel;
                    }
                    else
                    {
                        angle = io.MouseDelta.y * gizmoRotateSpeedDegPerPixel;
                    }

                    const blib::math::Vector<float, 3> axisDirection =
                        (axis == 0) ? blib::math::Vector<float, 3>(1.0f, 0.0f, 0.0f)
                        : (axis == 1) ? blib::math::Vector<float, 3>(0.0f, 1.0f, 0.0f)
                                      : blib::math::Vector<float, 3>(0.0f, 0.0f, 1.0f);
                    const blib::math::Quaternion<float> deltaRotation(
                        blib::math::AngleDegreef(angle), axisDirection);

                    // Локальный поворот: дельта пред-умножается (мировые оси)
                    transform->setLocalRotation(deltaRotation * this->impl->gizmoDrag.startTrs.rotation);
                    break;
                }

                case GizmoMode::Scale:
                {
                    // Масштабирование вдоль активной оси: множитель от
                    // величины драга (относительно длины стрелки)
                    float amount = 0.0f;
                    if (axis == 0)
                    {
                        amount = planeDelta.x;
                    }
                    else if (axis == 1)
                    {
                        amount = -io.MouseDelta.y * panScale;
                    }
                    else
                    {
                        amount = planeDelta.z;
                    }

                    const float factor = 1.0f + amount / gizmoAxisLength;
                    blib::math::Vector<float, 3> scale = this->impl->gizmoDrag.startTrs.scale;
                    if (axis == 0)
                    {
                        scale.x *= factor;
                        if (scale.x < gizmoMinScaleComponent) scale.x = gizmoMinScaleComponent;
                    }
                    else if (axis == 1)
                    {
                        scale.y *= factor;
                        if (scale.y < gizmoMinScaleComponent) scale.y = gizmoMinScaleComponent;
                    }
                    else
                    {
                        scale.z *= factor;
                        if (scale.z < gizmoMinScaleComponent) scale.z = gizmoMinScaleComponent;
                    }
                    transform->setLocalScale(scale);
                    break;
                }
            }
        }

        float EditorApplication::gizmoRayAxisDistance(
            _In const blib::math::Vector<float, 3>& rayOrigin,
            _In const blib::math::Vector<float, 3>& rayDirection,
            _In const blib::math::Vector<float, 3>& axisOrigin,
            _In const blib::math::Vector<float, 3>& axisDirection) const
        {
            // Расстояние между двумя лучами (луч мыши и ось gizmo).
            // Направления нормированы: знаменатель = 1 - (d1·d2)^2
            const blib::math::Vector<float, 3> w0 = rayOrigin - axisOrigin;
            const float a = blib::math::dot(rayDirection, rayDirection);
            const float b = blib::math::dot(rayDirection, axisDirection);
            const float c = blib::math::dot(axisDirection, axisDirection);
            const float d = blib::math::dot(rayDirection, w0);
            const float e = blib::math::dot(axisDirection, w0);

            const float denominator = a * c - b * b;
            constexpr float rayAxisEpsilon = 0.000001f;
            if (denominator < rayAxisEpsilon)
            {
                // Лучи параллельны — расстояние от начала оси до луча
                const blib::math::Vector<float, 3> closest = axisOrigin - (rayOrigin + rayDirection * d);
                return blib::math::length(closest);
            }

            const float s = (b * e - c * d) / denominator;
            const float t = (a * e - b * d) / denominator;

            const blib::math::Vector<float, 3> rayPoint = rayOrigin + rayDirection * s;
            const blib::math::Vector<float, 3> axisPoint = axisOrigin + axisDirection * t;
            return blib::math::length(rayPoint - axisPoint);
        }

        void EditorApplication::drawGizmo()
        {
            const EntityID selected = this->impl->selectedEntity;
            if (__blib_unlikely(selected == invalidEntity))
            {
                return;
            }

            beng::TransformComponent* transform =
                this->impl->scene.tryGetComponent<beng::TransformComponent>(selected);
            if (__blib_unlikely(transform == nullptr))
            {
                return;
            }

            const blib::math::Vector<float, 3> position = transform->getWorldPosition();
            const buint8 hotAxis = this->impl->gizmoHotAxis;

            const blib::graphics::Color axisXColor(
                gizmoAxisXColorR, gizmoAxisXColorG, gizmoAxisXColorB, gizmoAxisAlpha);
            const blib::graphics::Color axisYColor(
                gizmoAxisYColorR, gizmoAxisYColorG, gizmoAxisYColorB, gizmoAxisAlpha);
            const blib::graphics::Color axisZColor(
                gizmoAxisZColorR, gizmoAxisZColorG, gizmoAxisZColorB, gizmoAxisAlpha);
            const blib::graphics::Color hotColor(
                gizmoHotColorR, gizmoHotColorG, gizmoHotColorB, gizmoAxisAlpha);

            const blib::graphics::Color xColor = (hotAxis == 0) ? hotColor : axisXColor;
            const blib::graphics::Color yColor = (hotAxis == 1) ? hotColor : axisYColor;
            const blib::graphics::Color zColor = (hotAxis == 2) ? hotColor : axisZColor;

            blib::graphics::LineRenderer& gizmo = this->impl->gizmoRenderer;
            gizmo.clear();

            const blib::math::Vector<float, 3> axisX(1.0f, 0.0f, 0.0f);
            const blib::math::Vector<float, 3> axisY(0.0f, 1.0f, 0.0f);
            const blib::math::Vector<float, 3> axisZ(0.0f, 0.0f, 1.0f);

            if (this->impl->gizmoMode == GizmoMode::Rotate)
            {
                // Три окружности в плоскостях, перпендикулярных осям:
                // ось X — окружность в плоскости YZ (базис Y, Z) и т.д.
                addGizmoCircle(gizmo, position, axisY, axisZ, xColor);
                addGizmoCircle(gizmo, position, axisX, axisZ, yColor);
                addGizmoCircle(gizmo, position, axisX, axisY, zColor);
            }
            else
            {
                // Стрелки (Translate и Scale — пока одинаковые; маркеры
                // наконечников — TODO)
                gizmo.addLine(position, position + axisX * gizmoAxisLength, xColor);
                gizmo.addLine(position, position + axisY * gizmoAxisLength, yColor);
                gizmo.addLine(position, position + axisZ * gizmoAxisLength, zColor);
            }

            // Gizmo рисуем поверх мешей (X-ray): выключаем тест глубины
            // на время отрисовки и возвращаем его обратно
            blib::graphics::IRenderTarget& renderTarget = this->impl->renderTarget;
            renderTarget.rc.api.ogl.__blib_glDisable(GL_DEPTH_TEST);
            renderTarget.draw(gizmo);
            renderTarget.rc.api.ogl.__blib_glEnable(GL_DEPTH_TEST);
        }

        void EditorApplication::resetCamera()
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                return;
            }

            this->impl->camera.setTarget(blib::graphics::Vector3f(cameraTargetX, cameraTargetY, cameraTargetZ));
            this->impl->camera.setDistance(cameraInitialDistance);
            this->impl->camera.update();
        }

        beng::Scene& EditorApplication::getScene()
        {
            return this->impl->scene;
        }

        blib::graphics::IRenderTarget& EditorApplication::getRenderTarget()
        {
            return this->impl->renderTarget;
        }

        blib::graphics::OrbitCamera& EditorApplication::getCamera()
        {
            return this->impl->camera;
        }

        blib::graphics::RenderWindow& EditorApplication::getWindow()
        {
            return this->impl->window;
        }

    } // namespace editor
} // namespace beng
