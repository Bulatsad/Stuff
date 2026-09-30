#include <beng/client/clientApplication.h>
#include <beng/client/iClientGame.h>

#include <beng/client/componentCameraAdapter.h>
#include <beng/client/components/cameraComponent.h>
#include <beng/client/systems/animationSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/time.h>
#include <beng/server/serverApplication.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/graphics/color.h>
#include <blib/graphics/console/consoleWindow.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/postprocess.h>
#include <blib/graphics/shader.h>
#include <blib/network/tcpListener.h>
#include <blib/system/memory/globalAllocator.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_win32.h>

#include <cmath>
#include <new>

#include <Windows.h>
#include <gl/GL.h>

// ---------------------------------------------------------------
// ImGui-хук в оконную процедуру (паттерн model_viewer):
// ImGui обрабатывает ввод первым, остальное — движку.
// Объявлен на ГЛОБАЛЬНОМ скоупе: ImGui_ImplWin32_WndProcHandler —
// экспортируемый символ бэкенда (внутри namespace extern
// объявлял бы другую функцию)
// ---------------------------------------------------------------
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Оригинальная оконная процедура движка (восстанавливается в shutdown)
static WNDPROC s_bengClientEngineWndProc = nullptr;

static LRESULT CALLBACK bengClientImguiWndProc(HWND hwnd, UINT uMsg, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wparam, lparam))
    {
        return 0;
    }

    return CallWindowProc(s_bengClientEngineWndProc, hwnd, uMsg, wparam, lparam);
}

namespace beng
{
    namespace client
    {
        namespace
        {
            // Параметры изометрической камеры по умолчанию: лёгкая
            // перспектива (малый FOV — фирменный вид), фиксированный
            // наклон, фиксированный азимут (игра может переопределить
            // через активную beng.Camera сцены — syncCameraFromScene)
            constexpr float cameraFovDegrees = 30.0f;
            constexpr float cameraNearDistance = 0.1f;
            constexpr float cameraFarDistance = 1000.0f;

            // Имя консольной команды перезагрузки шейдеров (см. F5)
            constexpr const char* hotreloadCommand = "hotreload";

            // Консольное окно (тильда): размер и позиция внизу окна игры
            constexpr float consoleWidth = 900.0f;
            constexpr float consoleHeight = 340.0f;

            // Радианы → градусы (обратный расчёт углов)
            constexpr float lightRadToDeg = 57.29577951f;

            /**
             * Построить окно оболочки: оконный режим — собственное
             * ОС-окно/GL-контекст; PIE — headless (без контекста).
             *
             * ВАЖНО: окно строится через эту функцию, а НЕ тернарником
             * в init-списке члена: MSVC материализует результат
             * условного оператора во временный RenderWindow, чей
             * деструктор удаляет GL-контекст сразу после конструктора
             * окна (wglGetCurrentContext() == NULL у рендер-таргета —
             * InitGraphicsApi падает с «procedure not found»). Пара
             * «return prvalue» элидируется гарантированно (C++17).
             */
            blib::graphics::RenderWindow createClientWindow(_In const ClientApplicationParams& params)
            {
                if (params.imguiEnabled)
                {
                    return blib::graphics::RenderWindow(
                        static_cast<uint16_t>(params.width),
                        static_cast<uint16_t>(params.height),
                        params.title);
                }
                return blib::graphics::RenderWindow();
            }
        }

        // Внутренности оболочки: окно, рендер-таргет, изокамера,
        // ECS-сцена, сеть, таймер. Полное определение скрыто в .cpp
        // (pimpl) — заголовок не тянет лишние графические типы blib.
        struct ClientApplication::ClientApplicationImpl
        {
            blib::graphics::RenderWindow window;
            blib::graphics::IRenderTarget renderTarget;
            blib::graphics::IsometricCamera camera;
            blib::graphics::PostProcess postProcess;
            bool postEnabled;

            // ECS-сцена клиента. Объявлена ПОСЛЕ графических объектов —
            // разрушается РАНЬШЕ окна/таргета (меши освобождают
            // GL-ресурсы при живом контексте)
            beng::Scene scene;

            // Базовый рендер-пайплайн сцены: Transform → Animation →
            // Render (игра добавляет свои системы в onClientInitialize).
            // Системы объявлены после сцены — разрушаются раньше неё
            // (Scene хранит сырые указатели на системы)
            beng::TransformSystem transformSystem;
            beng::AnimationSystem animationSystem;
            beng::RenderSystem renderSystem;

            // Игра (хуки IClientGame) — не владеет, живёт дольше ядра
            IClientGame* game;

            // Local-server mode: in-process сервер (nullptr — внешний
            // сервер/офлайн). Тикает ПЕРЕД сетью клиента в каждом кадре
            beng::server::ServerApplication* localServer;

            // Сетевой клиент репликации (зеркала юнитов в сцене)
            beng::ReplicationClient replicationClient;

            // Сессия открыта (Welcome принят) — для детекта переходов
            // ready ↔ lost (см. tick)
            bool sessionReadyNotified;

            beng::Time time;

            // Консоль (тильда): ImGui-окно поверх blib::console::Console
            blib::graphics::console::ConsoleWindow consoleWindow;
            bool showConsole;

            // ImGui-презентация включена (false — PIE: контекст эдитора)
            bool imguiEnabled;

            // Флаг жизни ядра (условие цикла headless-режима)
            bool running;

            ClientApplicationImpl(_In IClientGame& gameParam, _In const ClientApplicationParams& params)
                // Оконный режим — собственное ОС-окно/GL-контекст;
                // PIE — headless-окно (без контекста): рендер идёт в
                // FBO клиента в контексте эдитора, кадр показывает
                // Game-панель (см. GRAPHICS.md, «Владение GL»)
                : window(createClientWindow(params))
                , renderTarget(params.width, params.height)
                , camera()
                , postProcess()
                // Пост-пасс в PIE выключен: кадр эдитора и так двойной
                // (вьюпорт + клиентское окно) — экономия целого прохода
                , postEnabled(params.imguiEnabled)
                , scene()
                , transformSystem()
                , animationSystem()
                , renderSystem()
                , game(&gameParam)
                , localServer(nullptr)
                , replicationClient()
                , sessionReadyNotified(false)
                , time()
                , consoleWindow()
                , showConsole(false)
                , imguiEnabled(params.imguiEnabled)
                , running(false)
            {
            }
        };

        ClientApplication::ClientApplication()
            : impl(nullptr)
        {
        }

        ClientApplication::~ClientApplication()
        {
            // Страховка: если владелец не вызвал shutdown явно
            this->shutdown();
        }

        bool ClientApplication::initialize(_In IClientGame& game, _In const ClientApplicationParams& params)
        {
            auto& globalAllocator = blib::memory::GlobalAllocator::instance();

            // Аллокация через GlobalAllocator + placement new (правило
            // проекта: выделяющие new/delete запрещены, placement — да)
            this->impl = static_cast<ClientApplicationImpl*>(
                globalAllocator.allocate(sizeof(ClientApplicationImpl)));
            new (this->impl) ClientApplicationImpl(game, params);

            // Изометрическая камера: фиксированный ракурс, лёгкая
            // перспектива. Наклон/азимут стоят по умолчанию в
            // конструкторе камеры. Аспект — из рендер-таргета
            // (в оконном режиме совпадает с окном, в PIE окна нет —
            // источник истины один: FBO)
            const buint32 rtWidth = this->impl->renderTarget.getContext().viewportWidth;
            const buint32 rtHeight = this->impl->renderTarget.getContext().viewportHeight;
            this->impl->camera.setPerspective(
                blib::math::AngleDegreef(cameraFovDegrees),
                static_cast<float>(rtWidth) / static_cast<float>(rtHeight),
                cameraNearDistance,
                cameraFarDistance);
            this->impl->camera.setTarget(blib::math::Vector<float, 3>(0.0f, 0.0f, 0.0f));
            this->impl->camera.update();

            this->impl->renderTarget.rc.setCamera(&this->impl->camera);

            // Базовый рендер-пайплайн сцены: Transform → Animation →
            // Render. Игра добавит свои системы в onClientInitialize
            this->impl->scene.addSystem(&this->impl->transformSystem);
            this->impl->scene.addSystem(&this->impl->animationSystem);
            this->impl->scene.addSystem(&this->impl->renderSystem);
            this->impl->renderSystem.setRenderTarget(&this->impl->renderTarget);

            // Игра: типы, системы, контент, консольные команды, сеть
            this->impl->game->onClientInitialize(this->impl->scene, *this);

            // Консольные команды графики: "hotreload" / "reload_shaders" —
            // перекомпиляция шейдеров с диска без перезапуска (см. F5)
            blib::graphics::registerGraphicsConsoleCommands();

            // Старт игры от активной камеры сцены (beng.Camera): ракурс,
            // позиция и параметры проекции берутся из компонента —
            // камера начинает ровно с того, что видно в Game-превью
            float activeNearDistance = cameraNearDistance;
            float activeFarDistance = cameraFarDistance;
            this->syncCameraFromScene(activeNearDistance, activeFarDistance);

            // Пост-пасс: дефолты класса + параметры проекции из камеры
            // (линеаризация глубины в шейдере). Ставится ПОСЛЕ синхронизации
            // камеры с компонентом — near/far совпадают
            {
                blib::graphics::PostProcessSettings postSettings = this->impl->postProcess.getSettings();
                postSettings.nearPlane = activeNearDistance;
                postSettings.farPlane = activeFarDistance;
                this->impl->postProcess.setSettings(postSettings);
            }

            // ImGui + WndProc-хук (паттерн model_viewer). В PIE-режиме
            // (headless) контекст НЕ создаётся — он уже есть у эдитора
            // (второй сломал бы его кадр), окна у клиента нет, кадр
            // показывает Game-панель эдитора
            if (this->impl->imguiEnabled)
            {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO(); (void)io;
                // Не писать imgui.ini в рабочую директорию
                io.IniFilename = nullptr;
                ImGui::StyleColorsDark();

                HWND hwnd = __blib_render_window_context(this->impl->window.__getCtx())->hwnd;
                ImGui_ImplWin32_Init(hwnd);
                ImGui_ImplOpenGL3_Init();
                s_bengClientEngineWndProc = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(bengClientImguiWndProc)));

                __blib_log_info("%s client application initialized (%ux%u window)",
                    this->impl->game->getGameName(), params.width, params.height);
            }
            else
            {
                __blib_log_info("%s client application initialized (PIE headless, %ux%u FBO)",
                    this->impl->game->getGameName(), params.width, params.height);
            }

            this->impl->running = true;
            return true;
        }

        bool ClientApplication::startLocalServer(_In beng::server::IServerGame& serverGame, buint32 port)
        {
            if (__blib_unlikely(this->impl == nullptr || this->impl->localServer != nullptr))
            {
                return false;
            }

            // Пробник занятости порта: bind на том же порту успешен —
            // порт свободен (своего сервера у процесса нет). Порт занят
            // (внешний/PIE-сервер уже слушает) — поднимать сервер не
            // надо, иначе NetworkServer залогировал бы bind-ошибку
            // как ERROR при штатном фолбэке одиночного клиента.
            // TOCTOU-гонка теоретически возможна, но все сценарии
            // (local-server/PIE) живут в одном процессе
            blib::network::TcpListener probe;
            blib::network::SocketStatus probeStatus = probe.open(blib::network::AddressType::IPv4);
            if (probeStatus == blib::network::SocketStatus::OK)
            {
                blib::network::Address probeAddress(blib::network::Address::LocalhostIPv4);
                probeAddress.setPort(static_cast<int>(port));
                probeStatus = probe.bind(probeAddress);
            }
            probe.close();

            if (probeStatus != blib::network::SocketStatus::OK)
            {
                // Порт занят — внешний сервер; хост подключится к нему
                // через connect() (см. ReplicationClient)
                __blib_log_info("%s client: local server skipped (port %u busy)",
                    this->impl->game->getGameName(), port);
                return false;
            }

            auto& globalAllocator = blib::memory::GlobalAllocator::instance();
            beng::server::ServerApplication* server = static_cast<beng::server::ServerApplication*>(
                globalAllocator.allocate(sizeof(beng::server::ServerApplication)));
            new (server) beng::server::ServerApplication();

            if (!server->initialize(serverGame, port))
            {
                // Порт занят (внешний/PIE-сервер уже слушает) — сервер
                // убираем; хост подключится к внешнему через connect()
                server->~ServerApplication();
                globalAllocator.deallocate(server, sizeof(beng::server::ServerApplication));
                __blib_log_info("%s client: local server skipped (port %u busy)",
                    this->impl->game->getGameName(), port);
                return false;
            }

            this->impl->localServer = server;
            this->impl->replicationClient.connect(port);

            __blib_log_info("%s client: local server started on port %u",
                this->impl->game->getGameName(), port);
            return true;
        }

        void ClientApplication::tick()
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                return;
            }

            // Переменный dt кадра (клиентская сторона гибридного
            // таймстепа; фиксированный тикрейт живёт на сервере —
            // см. ARCHITECTURE.md)
            this->impl->time.tick();
            const float deltaTime = this->impl->time.getDeltaTime();

            // Пауза на открытой консоли: симуляция получает нулевой dt —
            // мир замерзает, рендер продолжает рисовать замершее
            // состояние. time.tick() при этом работает каждый кадр —
            // скачка симуляции после закрытия консоли нет
            const float simDeltaTime = this->impl->showConsole ? 0.0f : deltaTime;

            // Прокачка оконных сообщений (закрытие по X, перерисовка) —
            // как в model_viewer, иначе окно не живёт
            this->impl->window.update();

            // Обновление состояния клавиатуры перед опросом
            blib::graphics::Keyboard::update();

            // Консоль/окно — только в оконном режиме: в PIE (headless)
            // консоли и окна нет, Escape обрабатывает эдитор (стоп PIE)
            if (this->impl->imguiEnabled)
            {
                // Тильда `~` открывает/закрывает консоль (Quake-стиль)
                if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Grave))
                {
                    this->impl->showConsole = !this->impl->showConsole;
                    if (this->impl->showConsole)
                    {
                        this->impl->consoleWindow.requestFocus();
                    }
                }

                // Escape: сперва закрывает консоль, иначе — окно
                if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Escape))
                {
                    if (this->impl->showConsole)
                    {
                        this->impl->showConsole = false;
                    }
                    else
                    {
                        this->impl->window.close();
                        return;
                    }
                }
            }

            // F5 — консольная команда перезагрузки шейдеров (hotreload):
            // правишь .glsl, жмёшь F5 — игра перекомпилирует без
            // перезапуска. Работает только при закрытой консоли
            // (глобальный Keyboard не знает про фокус ImGui)
            if (!this->impl->showConsole &&
                blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::F5))
            {
                blib::console::Console::instance().execute(hotreloadCommand);
            }

            // Local-server mode: авторитетная симуляция шагает ДО сети
            // клиента (порядок PIE-пары, см. PieSession)
            if (this->impl->localServer != nullptr)
            {
                this->impl->localServer->tick();
            }

            // Сеть: опрос (снапшоты → зеркало → интерполяция) + хуки
            // переходов сессии
            this->impl->replicationClient.poll(this->impl->scene);
            if (this->impl->replicationClient.isSessionReady() && !this->impl->sessionReadyNotified)
            {
                this->impl->sessionReadyNotified = true;
                this->impl->game->onSessionReady(
                    this->impl->replicationClient.getServerTickRate(),
                    this->impl->replicationClient.getPlayerEntityId());
            }
            else if (!this->impl->replicationClient.isSessionReady() && this->impl->sessionReadyNotified)
            {
                this->impl->sessionReadyNotified = false;
                this->impl->game->onSessionLost();
            }

            // Игра: ввод → сетевой кадр (события зеркал, команды,
            // предсказание) → состояние рендера (камера, свет)
            this->impl->game->onInput(simDeltaTime);
            this->impl->game->onNetworkUpdate();
            this->impl->game->onSceneWillUpdate(simDeltaTime);

            // Единственная точка отрисовки мира: вся сцена рисуется
            // RenderSystem'ом внутри scene.update() по слоям
            this->impl->renderTarget.clear(blib::graphics::Color::Black);
            this->impl->scene.update(simDeltaTime);

            this->impl->game->onSceneDidUpdate(simDeltaTime);

            // Презентация сцены: либо пост-пасс (дымка/grading/виньетка
            // из FBO-текстур сцены), либо прямой блит. Пост-пасс рисует
            // в back-буфер (FBO 0), ImGui — поверх
            if (this->impl->postEnabled)
            {
                this->impl->renderTarget.rc.api.ogl.ext.__blib_gl_glBindFramebuffer(GL_FRAMEBUFFER, 0);
                this->impl->postProcess.apply(
                    this->impl->renderTarget.rc,
                    this->impl->renderTarget.getColorTexture(),
                    this->impl->renderTarget.getDepthTexture());
            }
            else
            {
                this->impl->window.blitToBackbuffer(this->impl->renderTarget);
            }

            if (!this->impl->imguiEnabled)
            {
                // PIE-режим (headless): без ImGui-кадра и презентации —
                // кадр остаётся в FBO клиента, его показывает
                // Game-панель эдитора (тот же GL-контекст/поток)
                return;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            this->impl->game->onUi();

            // Консоль (тильда): ImGui-окно внизу экрана, Enter исполняет
            // строку (Console::execute внутри ConsoleWindow::draw)
            if (this->impl->showConsole)
            {
                ImGui::SetNextWindowPos(
                    ImVec2(0.0f, static_cast<float>(this->impl->window.getHeight()) - consoleHeight),
                    ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(consoleWidth, consoleHeight), ImGuiCond_FirstUseEver);
                this->impl->consoleWindow.draw();
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            this->impl->window.swapBuffers();
        }

        void ClientApplication::shutdown()
        {
            if (this->impl == nullptr)
            {
                return;
            }

            const char* gameName = this->impl->game->getGameName();

            // Игра гасится первой (мир и сеть ещё живы)
            this->impl->game->onShutdown();

            // Сеть клиента — до локального сервера (клиентские
            // GL-ресурсы мира освобождаются в деструкторе impl)
            this->impl->replicationClient.shutdown();

            // Local-server: корректное гашение + возврат памяти
            if (this->impl->localServer != nullptr)
            {
                this->impl->localServer->shutdown();
                this->impl->localServer->~ServerApplication();
                blib::memory::GlobalAllocator::instance().deallocate(
                    this->impl->localServer, sizeof(beng::server::ServerApplication));
                this->impl->localServer = nullptr;
            }

            // Вернуть оригинальную оконную процедуру до гашения ImGui
            // (паттерн model_viewer); в PIE-режиме хука не было
            if (this->impl->imguiEnabled)
            {
                HWND hwnd = __blib_render_window_context(this->impl->window.__getCtx())->hwnd;
                if (s_bengClientEngineWndProc != nullptr)
                {
                    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_bengClientEngineWndProc));
                    s_bengClientEngineWndProc = nullptr;
                }

                ImGui_ImplOpenGL3_Shutdown();
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
            }

            // Явный вызов деструктора + возврат памяти глобальному
            // аллокатору. Сцена (с мешами) разрушается внутри — раньше
            // окна/таргета (порядок членов impl): GL-контекст на момент
            // выгрузки жив
            this->impl->~ClientApplicationImpl();
            blib::memory::GlobalAllocator::instance().deallocate(
                this->impl, sizeof(ClientApplicationImpl));
            this->impl = nullptr;

            __blib_log_info("%s client application shut down", gameName);
        }

        bool ClientApplication::isRunning() const
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                return false;
            }
            // Оконный режим — жизнь окна; headless — до shutdown()
            return this->impl->imguiEnabled ? this->impl->window.isOpen() : this->impl->running;
        }

        buint64 ClientApplication::getColorTextureId() const
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                return 0;
            }

            // Текстура текущего кадрового буфера FBO (после scene.update()
            // в tick() — дорисованный кадр); в PIE-режиме её сэмплит
            // Game-панель эдитора
            return static_cast<buint64>(this->impl->renderTarget.getColorTexture().getContext().textureID);
        }

        beng::Scene& ClientApplication::getScene()
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                __blib_fatal("ClientApplication: getScene on uninitialized core");
            }
            return this->impl->scene;
        }

        blib::graphics::IRenderTarget& ClientApplication::getRenderTarget()
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                __blib_fatal("ClientApplication: getRenderTarget on uninitialized core");
            }
            return this->impl->renderTarget;
        }

        blib::graphics::IsometricCamera& ClientApplication::getCamera()
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                __blib_fatal("ClientApplication: getCamera on uninitialized core");
            }
            return this->impl->camera;
        }

        blib::graphics::RenderWindow& ClientApplication::getWindow()
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                __blib_fatal("ClientApplication: getWindow on uninitialized core");
            }
            return this->impl->window;
        }

        beng::ReplicationClient& ClientApplication::getReplicationClient()
        {
            if (__blib_unlikely(this->impl == nullptr))
            {
                __blib_fatal("ClientApplication: getReplicationClient on uninitialized core");
            }
            return this->impl->replicationClient;
        }

        bool ClientApplication::isPostProcessEnabled() const
        {
            return this->impl != nullptr && this->impl->postEnabled;
        }

        void ClientApplication::setPostProcessEnabled(bool enabled)
        {
            if (__blib_likely(this->impl != nullptr))
            {
                this->impl->postEnabled = enabled;
            }
        }

        bool ClientApplication::isConsoleOpen() const
        {
            return this->impl != nullptr && this->impl->showConsole;
        }

        void ClientApplication::syncCameraFromScene(_Out float& outNearDistance, _Out float& outFarDistance)
        {
            // Фолбэк: камеры-сущности нет — камера остаётся на дефолтах
            // оболочки, near/far наружу отдаются дефолтные
            outNearDistance = cameraNearDistance;
            outFarDistance = cameraFarDistance;

            beng::ComponentPool<beng::CameraComponent>* pool =
                this->impl->scene.tryGetComponentPool<beng::CameraComponent>();
            if (__blib_unlikely(pool == nullptr))
            {
                return;
            }

            // Активная камера — «последняя включённая» (максимальный
            // штамп, консистентно с CameraSystem). Итератор пула
            // пропускает неактивные компоненты
            const beng::CameraComponent* activeCamera = nullptr;
            beng::EntityID cameraEntity = beng::invalidEntity;
            for (auto it = pool->begin(); it != pool->end(); ++it)
            {
                if (activeCamera == nullptr ||
                    it->getActivationStamp() > activeCamera->getActivationStamp())
                {
                    activeCamera = &*it;
                    cameraEntity = it.getEntityId();
                }
            }
            if (__blib_unlikely(activeCamera == nullptr))
            {
                return;
            }

            beng::TransformComponent* transform =
                this->impl->scene.tryGetComponent<beng::TransformComponent>(cameraEntity);
            if (__blib_unlikely(transform == nullptr))
            {
                return;
            }

            // Базис взгляда из мирового поворота сущности (единая
            // математика с ComponentCameraAdapter)
            blib::math::Vector<float, 3> forward;
            blib::math::Vector<float, 3> right;
            blib::math::Vector<float, 3> up;
            beng::ComponentCameraAdapter::computeBasis(
                transform->getWorldRotation(), forward, right, up);

            // Проекция — из компонента; аспект — из FBO (источник
            // истины: в оконном режиме совпадает с окном, в PIE окна нет)
            const buint32 rtWidth = this->impl->renderTarget.getContext().viewportWidth;
            const buint32 rtHeight = this->impl->renderTarget.getContext().viewportHeight;
            const float aspect = (rtHeight > 0)
                ? static_cast<float>(rtWidth) / static_cast<float>(rtHeight) : 1.0f;

            this->impl->camera.setPerspective(
                blib::math::AngleDegreef(activeCamera->getFovDegrees()),
                aspect,
                activeCamera->getNearDistance(),
                activeCamera->getFarDistance());

            // Ракурс изометрии из направления взгляда: IsometricCamera
            // хранит направление «камера → цель» (position = target +
            // direction * distance), т.е. ПРОТИВОПОЛОЖНОЕ forward
            // сущности. pitch — из вертикальной компоненты, yaw — из
            // горизонтальной (см. isometricCamera.cpp update())
            const blib::math::Vector<float, 3> toTarget = forward * -1.0f;
            const float clampedY = (toTarget.y > 1.0f) ? 1.0f : ((toTarget.y < -1.0f) ? -1.0f : toTarget.y);
            const float pitchDegrees = std::asin(clampedY) * lightRadToDeg;
            const float yawDegrees = std::atan2(toTarget.x, toTarget.z) * lightRadToDeg;
            this->impl->camera.setPitch(blib::math::AngleDegreef(pitchDegrees));
            this->impl->camera.setYaw(blib::math::AngleDegreef(yawDegrees));

            // Целевая точка — перед сущностью вдоль взгляда на текущей
            // дистанции: позиция камеры после update() == позиции
            // сущности, взгляд направлен как у сущности
            const float distance = this->impl->camera.getDistance();
            this->impl->camera.setTarget(transform->getWorldPosition() + forward * distance);
            this->impl->camera.update();

            outNearDistance = activeCamera->getNearDistance();
            outFarDistance = activeCamera->getFarDistance();

            __blib_log_info("%s client camera: adopted active scene camera (fov %.1f deg)",
                this->impl->game->getGameName(), static_cast<double>(activeCamera->getFovDegrees()));
        }

    } // namespace client
} // namespace beng
