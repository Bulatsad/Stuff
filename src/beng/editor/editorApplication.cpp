#include <beng/editor/editorApplication.h>

#include <beng/client/components/ambientLightComponent.h>
#include <beng/client/components/animatorComponent.h>
#include <beng/client/components/blobShadowComponent.h>
#include <beng/client/components/directionalLightComponent.h>
#include <beng/client/components/meshRenderComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/client/systems/animationSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/editor/panels/consolePanel.h>
#include <beng/editor/panels/viewportPanel.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/core/math/angle.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/orbitCamera.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/renderWindow.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_win32.h>

#include <Windows.h>
#include <gl/GL.h>

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
                , panelListAllocator()
                , panels(blib::memory::StdAllocatorAdapter<RegisteredPanel>(&this->panelListAllocator))
                , windowTitle(title)
                , showConsole(false)
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

        EditorApplication::EditorApplication()
            : impl(nullptr)
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

            // ImGui + WndProc-хук
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            // Не писать imgui.ini в рабочую директорию (мусорит в корне
            // репозитория при запуске из студии)
            io.IniFilename = nullptr;
            ImGui::StyleColorsDark();

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
            this->impl->renderTarget.clear(blib::graphics::Color::Black);
            this->impl->scene.update(deltaTime);

            // Отладочные слои хоста поверх сцены (в тот же FBO)
            this->onSceneDidUpdate(deltaTime);

            // UI: переключаемся на back-буфер (иначе ImGui-бэкенд
            // рисует в FBO, а вьюпорт сэмплит его же — feedback loop)
            this->impl->renderTarget.rc.api.ogl.ext.__blib_gl_glBindFramebuffer(GL_FRAMEBUFFER, 0);
            this->impl->renderTarget.rc.api.ogl.__blib_gl_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Раскладка: зоны хоста
            {
                const float windowW = static_cast<float>(this->impl->window.getWight());
                const float windowH = static_cast<float>(this->impl->window.getHeight());
                const float hierarchyHeight = windowH * hierarchyHeightFraction;

                for (const EditorApplicationImpl::RegisteredPanel& registered : this->impl->panels)
                {
                    switch (registered.zone)
                    {
                        case PanelZone::LeftTop:
                            ImGui::SetNextWindowPos(ImVec2(0.0f, topBarHeight), ImGuiCond_FirstUseEver);
                            ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, hierarchyHeight - topBarHeight), ImGuiCond_FirstUseEver);
                            break;
                        case PanelZone::LeftBottom:
                            ImGui::SetNextWindowPos(ImVec2(0.0f, hierarchyHeight), ImGuiCond_FirstUseEver);
                            ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, windowH - hierarchyHeight), ImGuiCond_FirstUseEver);
                            break;
                        case PanelZone::Right:
                            ImGui::SetNextWindowPos(ImVec2(windowW - rightPanelWidth, topBarHeight), ImGuiCond_FirstUseEver);
                            ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, windowH - topBarHeight), ImGuiCond_FirstUseEver);
                            break;
                    }
                    registered.panel->draw();
                }

                // Свои ImGui-окна хоста: верхняя полоса (позицию/
                // размер задаёт каркас — полоса зарезервирована за
                // хостом), модальные диалоги
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(windowW, topBarHeight), ImGuiCond_FirstUseEver);
                this->onUi();

                // Центр: вьюпорт
                ImGui::SetNextWindowPos(ImVec2(leftPanelWidth, topBarHeight), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(windowW - leftPanelWidth - rightPanelWidth, windowH - topBarHeight), ImGuiCond_FirstUseEver);
                this->impl->viewportPanel.draw();

                // Консоль поверх всего
                if (this->impl->showConsole)
                {
                    ImGui::SetNextWindowPos(ImVec2(0.0f, windowH - consoleHeight), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowSize(ImVec2(consoleWidth, consoleHeight), ImGuiCond_FirstUseEver);
                    this->impl->consolePanel.draw();
                }
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
