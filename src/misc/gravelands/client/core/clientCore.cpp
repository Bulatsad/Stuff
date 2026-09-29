#include <gravelands/client/core/clientCore.h>
#include <gravelands/client/core/networkClient.h>
#include <gravelands/common/protocol.h>
#include <gravelands/world/world.h>

#include <beng/client/components/ambientLightComponent.h>
#include <beng/client/components/directionalLightComponent.h>
#include <beng/client/components/meshRenderComponent.h>
#include <beng/client/systems/animationSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/graphics/color.h>
#include <blib/graphics/console/consoleWindow.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
#include <blib/graphics/isometricCamera.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/postprocess.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/shader.h>
#include <blib/graphics/sphere.h>
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
static WNDPROC s_engineWndProc = nullptr;

static LRESULT CALLBACK gravelandsImguiWndProc(HWND hwnd, UINT uMsg, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wparam, lparam))
    {
        return 0;
    }

    return CallWindowProc(s_engineWndProc, hwnd, uMsg, wparam, lparam);
}

namespace gravelands
{
    namespace
    {
        // Параметры изометрической камеры: лёгкая перспектива (малый FOV —
        // фирменный вид Hades), фиксированный наклон, фиксированный азимут.
        // Свободного вращения нет: ракурс — часть художественного стиля
        constexpr float cameraFovDegrees = 30.0f;
        constexpr float cameraNearDistance = 0.1f;
        constexpr float cameraFarDistance = 1000.0f;

        // Скорость перемещения цели камеры по миру (WASD), мир. ед./с
        constexpr float cameraMoveSpeed = 60.0f;

        // Экспоненциальное сглаживание цели камеры при следовании за
        // зеркалом игрока (1/с): гасит остаточную дрожь интерполяции
        // снапшотов — жёсткая привязка передавала бы её камере
        constexpr float cameraFollowSmoothing = 12.0f;

        // Скорость зума (Add/Subtract), изменение дистанции в ед./с
        constexpr float cameraZoomSpeed = 120.0f;

        // Имя консольной команды перезагрузки шейдеров (см. F5)
        constexpr const char* hotreloadCommand = "hotreload";

        // Консольное окно (тильда): размер и позиция внизу окна игры
        constexpr float consoleWidth = 900.0f;
        constexpr float consoleHeight = 340.0f;

        // Отступ и прозрачность оверлея-подсказки (полупрозрачный текст
        // в углу — вместо отладочной панели, см. фазу 4)
        constexpr float overlayPadding = 10.0f;
        constexpr float overlayBackgroundAlpha = 0.35f;

        // Радианы → градусы (для обратного расчёта углов света в оверлее)
        constexpr float lightRadToDeg = 57.29577951f;

        // Текст-заглушка оверлея, когда компонент света в сцене
        // отсутствует (после загрузки чужих сцен)
        constexpr const char* overlayValueMissing = "-";

        // ========== Сеть (зеркала юнитов из снапшотов) ==========

        // Визуал сетевого юнита: радиус и сегменты сферы-плейсхолдера
        constexpr buint32 networkUnitSegments = 24;
        constexpr buint8 networkUnitColorR = 200;
        constexpr buint8 networkUnitColorG = 160;
        constexpr buint8 networkUnitColorB = 110;
        constexpr buint8 networkUnitColorA = 255;

        // Текст статуса сети в оверлее
        constexpr const char* networkConnectedLabel = "connected";
        constexpr const char* networkOfflineLabel = "offline";
    }

    // Внутренности клиента: окно, рендер-таргет, изокамера, мир, таймер.
    // Полное определение скрыто в .cpp (pimpl) — заголовок не тянет
    // графические типы blib в потребителей.
    struct ClientCore::ClientCoreImpl
    {
        blib::graphics::RenderWindow window;
        blib::graphics::IRenderTarget renderTarget;
        blib::graphics::IsometricCamera camera;
        blib::graphics::PostProcess postProcess;
        bool postEnabled;

        // ECS-сцена клиента (к ней привязывается мир gravelands::World;
        // мир сценой не владеет). Объявлена ПОСЛЕ графических объектов —
        // разрушается РАНЬШЕ окна/таргета (меши освобождают GL-ресурсы
        // при живом контексте)
        beng::Scene scene;

        // Базовый рендер-пайплайн клиентской сцены: Transform →
        // Animation → Render (мир добавляет только свои системы —
        // тень/свет, см. gravelands::World). Системы объявлены после
        // сцены — разрушаются раньше неё (Scene хранит сырые указатели)
        beng::TransformSystem transformSystem;
        beng::AnimationSystem animationSystem;
        beng::RenderSystem renderSystem;

        // Мир Gravelands (системы тени/света + контент): общий с
        // эдитором. Объявлен после систем — разрушается раньше них
        gravelands::World world;

        // ========== Сеть ==========

        // Сетевой клиент (loopback, неблокирующий опрос в кадре)
        gravelands::NetworkClient networkClient;

        // EntityID игрока на сервере (из Welcome)
        buint64 playerNetworkEntity;

        // Зеркала серверных юнитов в клиентской сцене
        struct ClientMirror
        {
            buint64 serverEntityId;
            beng::EntityID clientEntityId;
        };
        ClientMirror mirrors[maxNetworkUnits];
        buint32 mirrorCount;

        // Кольцевой буфер последних снапшотов: интерполяция идёт по
        // tick number сервера между парой, охватывающей целевое время
        // (фикс. задержка рендера), а не по времени приёма — последнее
        // дышит вместе с кадром (PIE: двойной GL-контекст, ImGui) и
        // рвало бы скорость интерполяции (дрожь/телепорты зеркал)
        SnapshotEntry snapshotBuffer[snapshotBufferSize][maxNetworkUnits];
        buint32 snapshotCount[snapshotBufferSize];   // юнитов в слоте
        buint32 snapshotTick[snapshotBufferSize];    // tick number сервера
        bool snapshotValid[snapshotBufferSize];
        buint32 snapshotHead; // индекс слота под следующий снапшот

        // Следование камеры за зеркалом игрока: флаг «цель захвачена» —
        // при первом появлении зеркала камера встаёт сразу (без полёта
        // через карту), дальше — экспоненциальное сглаживание
        bool cameraFollowActive;

        // ========== Client-side prediction игрока ==========
        // Предсказанная позиция + текущий локальный ввод. Ввод
        // интегрируется локально той же формулой, что MovementSystem
        // сервера (нормализованная диагональ × playerMoveSpeed, кламп
        // worldBounds) — движение начинается МГНОВЕННО, сеть и кадровое
        // время влияют только на реконсиляцию (reconcilePlayerPrediction)
        blib::graphics::Vector3f predictedPosition;
        bool predictionActive;
        PlayerCommand predictionCommand;

        // Последняя отправленная команда (dedup — слать при изменении)
        PlayerCommand lastSentCommand;
        bool lastSentCommandValid;

        beng::Time time;

        // Консоль (тильда): ImGui-окно поверх blib::console::Console —
        // команды вводятся строкой (Enter), история/дополнение — ядро
        blib::graphics::console::ConsoleWindow consoleWindow;
        bool showConsole;

        // ImGui-презентация включена (false — PIE: контекст эдитора)
        bool imguiEnabled;

        ClientCoreImpl(bool imguiEnabledParam)
            : window(
                static_cast<uint16_t>(imguiEnabledParam ? windowWidth : pieWindowWidth),
                static_cast<uint16_t>(imguiEnabledParam ? windowHeight : pieWindowHeight),
                gameTitle)
            , renderTarget(
                imguiEnabledParam ? windowWidth : pieWindowWidth,
                imguiEnabledParam ? windowHeight : pieWindowHeight)
            , camera()
            , postProcess()
            // Пост-пасс в PIE выключен: кадр эдитора и так двойной
            // (вьюпорт + клиентское окно) — экономия целого прохода
            , postEnabled(imguiEnabledParam)
            , scene()
            , transformSystem()
            , animationSystem()
            , renderSystem()
            , world()
            , networkClient()
            , playerNetworkEntity(beng::invalidEntity)
            , mirrorCount(0)
            , snapshotHead(0)
            , cameraFollowActive(false)
            , predictedPosition(0.0f, 0.0f, 0.0f)
            , predictionActive(false)
            , predictionCommand{ 0, 0 }
            , lastSentCommand{ 0, 0 }
            , lastSentCommandValid(false)
            , time()
            , consoleWindow()
            , showConsole(false)
            , imguiEnabled(imguiEnabledParam)
        {
            // Зеркала: невалидные ID (нет привязки)
            for (buint32 i = 0; i < maxNetworkUnits; ++i)
            {
                this->mirrors[i].serverEntityId = beng::invalidEntity;
                this->mirrors[i].clientEntityId = beng::invalidEntity;
            }

            // Кольцевой буфер снапшотов: все слоты пусты
            for (buint32 i = 0; i < snapshotBufferSize; ++i)
            {
                this->snapshotCount[i] = 0;
                this->snapshotTick[i] = 0;
                this->snapshotValid[i] = false;
            }
        }
    };

    ClientCore::ClientCore()
        : impl(nullptr)
    {
    }

    ClientCore::~ClientCore()
    {
        // Страховка: если владелец не вызвал shutdown явно
        shutdown();
    }

    bool ClientCore::initialize(bool imguiEnabled)
    {
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();

        // Аллокация через GlobalAllocator + placement new (проектное правило:
        // выделяющие new/delete запрещены, placement new разрешён)
        impl = static_cast<ClientCoreImpl*>(globalAllocator.allocate(sizeof(ClientCoreImpl)));
        new (impl) ClientCoreImpl(imguiEnabled);

        // Изометрическая камера: фиксированный ракурс, лёгкая перспектива.
        // Наклон/азимут уже стоят по умолчанию в конструкторе камеры
        impl->camera.setPerspective(
            blib::math::AngleDegreef(cameraFovDegrees),
            static_cast<float>(impl->window.getWight()) / static_cast<float>(impl->window.getHeight()),
            cameraNearDistance,
            cameraFarDistance);
        impl->camera.setTarget(blib::graphics::Vector3f(0.0f, 0.0f, 0.0f));
        impl->camera.update();

        impl->renderTarget.rc.setCamera(&impl->camera);

        // Пост-пасс (фаза 6): дефолты класса + параметры проекции
        // из конфигурации камеры (линеаризация глубины в шейдере)
        {
            blib::graphics::PostProcessSettings postSettings = impl->postProcess.getSettings();
            postSettings.nearPlane = cameraNearDistance;
            postSettings.farPlane = cameraFarDistance;
            impl->postProcess.setSettings(postSettings);
        }

        // Базовый рендер-пайплайн сцены клиента: Transform → Animation
        // → Render. Мир (ниже) добавит свои системы (тень/свет)
        impl->scene.addSystem(&impl->transformSystem);
        impl->scene.addSystem(&impl->animationSystem);
        impl->scene.addSystem(&impl->renderSystem);
        impl->renderSystem.setRenderTarget(&impl->renderTarget);

        // Мир (gravelands-world): привязка к сцене клиента (типы,
        // системы тени/света), контент, консольные команды
        // scene_save/scene_load. Рендер-таргет мира — наш FBO
        impl->world.initialize(impl->scene);
        impl->world.setRenderTarget(&impl->renderTarget);
        impl->world.registerConsoleCommands();

        // Консольные команды графики: "hotreload" / "reload_shaders" —
        // перекомпиляция шейдеров с диска без перезапуска (см. F5)
        blib::graphics::registerGraphicsConsoleCommands();

        // Мир: тайлы, сфера, деревья, тени, свет + скелетная модель
        // с анимацией (Mixamo-FBX)
        impl->world.setupWorld();
        impl->world.loadDancerModel();

        // Сеть: попытка подключения к локальному серверу (loopback).
        // При неудаче клиент остаётся в офлайн-дебаг-режиме
        // (локальная сфера-персонаж, WASD двигает камеру)
        impl->networkClient.connect(serverDefaultPort);

        // ImGui + WndProc-хук (паттерн model_viewer): нужен для
        // полупрозрачного оверлея-подсказки в углу. В PIE-режиме
        // (imguiEnabled == false) контекст НЕ создаётся — он уже есть
        // у эдитора, второй контекст сломал бы его кадр
        if (impl->imguiEnabled)
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            // Не писать imgui.ini в рабочую директорию
            io.IniFilename = nullptr;
            ImGui::StyleColorsDark();

            HWND hwnd = __blib_render_window_context(impl->window.__getCtx())->hwnd;
            ImGui_ImplWin32_Init(hwnd);
            ImGui_ImplOpenGL3_Init();
            s_engineWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gravelandsImguiWndProc)));
        }

        __blib_log_info("%s client core initialized (%ux%u window)",
            gameTitle, windowWidth, windowHeight);

        return true;
    }

    void ClientCore::tick()
    {
        if (__blib_unlikely(impl == nullptr))
        {
            return;
        }

        // Переменный dt кадра (клиентская сторона гибридного таймстепа;
        // фиксированный тикрейт живёт на сервере — см. ARCHITECTURE.md)
        impl->time.tick();
        const float deltaTime = impl->time.getDeltaTime();

        // Пауза на открытой консоли: симуляция получает нулевой dt —
        // мир замерзает (анимация/тени/камера/свет не продвигаются),
        // рендер продолжает рисовать замершее состояние. time.tick()
        // при этом работает каждый кадр, dt остаётся покадровым —
        // скачка симуляции после закрытия консоли нет
        const float simDeltaTime = impl->showConsole ? 0.0f : deltaTime;

        // Прокачка оконных сообщений (закрытие по X, перерисовка) —
        // как в model_viewer, иначе окно не живёт
        impl->window.update();

        // Обновление состояния клавиатуры перед опросом (для камеры и выхода)
        blib::graphics::Keyboard::update();

        // Тильда `~` открывает/закрывает консоль (Quake-стиль)
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Grave))
        {
            impl->showConsole = !impl->showConsole;
            if (impl->showConsole)
            {
                impl->consoleWindow.requestFocus();
            }
        }

        // Escape: сперва закрывает консоль, иначе — окно
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Escape))
        {
            if (impl->showConsole)
            {
                impl->showConsole = false;
            }
            else
            {
                impl->window.close();
                return;
            }
        }

        // Отладочные клавиши работают ТОЛЬКО при закрытой консоли:
        // глобальный Keyboard не знает про фокус ImGui, и при печати
        // в консоли нажатия «протекали» бы в игру (M/O/N/P/F5).
        // Escape и тильда — управление самой консолью — живут выше,
        // вне этого гейта
        if (!impl->showConsole)
        {
            // F5 — консольная команда перезагрузки шейдеров (hotreload):
            // правишь .glsl, жмёшь F5 — игра перекомпилирует без перезапуска
            if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::F5))
            {
                blib::console::Console::instance().execute(hotreloadCommand);
            }

            // N — отладочная раскраска нормалями (проверка проброса
            // нормали из VBO во фрагментный шейдер, фаза 2)
            if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::N))
            {
                impl->renderTarget.rc.showNormals = !impl->renderTarget.rc.showNormals;
            }

            // P — включение/выключение пост-пасса (фаза 6, сравнение до/после)
            if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::P))
            {
                impl->postEnabled = !impl->postEnabled;
            }

            // M — переключение сферы unlit/toon (сравнение до/после света).
            // Сфера — в мире: доступ через tryGetComponent (после
            // scene_load ID может устареть — не fatal)
            if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::M))
            {
                beng::MeshRenderComponent* sphereMesh =
                    impl->world.getScene().tryGetComponent<beng::MeshRenderComponent>(impl->world.getSphereEntity());
                if (sphereMesh != nullptr)
                {
                    blib::graphics::Material& sphereMaterial = sphereMesh->getMesh().material;
                    sphereMaterial.shadingMode =
                        sphereMaterial.shadingMode == blib::graphics::ShadingMode::Toon
                        ? blib::graphics::ShadingMode::Unlit
                        : blib::graphics::ShadingMode::Toon;
                }
            }

            // O — включение/выключение контура сферы (inverted hull, фаза 8)
            if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::O))
            {
                beng::MeshRenderComponent* sphereMesh =
                    impl->world.getScene().tryGetComponent<beng::MeshRenderComponent>(impl->world.getSphereEntity());
                if (sphereMesh != nullptr)
                {
                    blib::graphics::Material& sphereMaterial = sphereMesh->getMesh().material;
                    sphereMaterial.outlineEnabled = !sphereMaterial.outlineEnabled;
                }
            }
        }

        updateCamera(simDeltaTime);

        // Отладочное управление светом (стрелки/[ ]/PageUp/PageDown) —
        // в мире (крутит компоненты света в его сцене)
        impl->world.updateLight(simDeltaTime);

        // Сеть: опрос + применение снапшотов (зеркала юнитов)
        this->updateNetworkState();

        // Команда игрока (WASD) — при подключённом сервере; она же
        // питает локальное предсказание (см. updatePlayerPrediction)
        if (impl->networkClient.getState() == gravelands::NetworkClient::State::Connected)
        {
            this->sendMovementCommand();
        }

        // Client-side prediction: локальный ввод применяется к зеркалу
        // игрока немедленно (интерполированная позиция перекрывается) —
        // отклик на клавиши мгновенный, независимо от кадрового времени
        this->updatePlayerPrediction(simDeltaTime);

        // Единственная точка отрисовки мира: вся сцена (тайлы, тени,
        // плоскости, сфера, скелетная модель) рисуется RenderSystem'ом
        // внутри world.update() по слоям (см. RenderLayer)
        impl->renderTarget.clear(blib::graphics::Color::Black);
        impl->world.update(simDeltaTime);

        // Презентация сцены: либо пост-пасс (дымка/grading/виньетка
        // из FBO-текстур сцены), либо прямой блит. Пост-пасс рисует
        // в back-буфер (FBO 0), ImGui — поверх
        if (impl->postEnabled)
        {
            impl->renderTarget.rc.api.ogl.ext.__blib_gl_glBindFramebuffer(GL_FRAMEBUFFER, 0);
            impl->postProcess.apply(
                impl->renderTarget.rc,
                impl->renderTarget.getColorTexture(),
                impl->renderTarget.getDepthTexture());
        }
        else
        {
            impl->window.blitToBackbuffer(impl->renderTarget);
        }

        if (!impl->imguiEnabled)
        {
            // PIE-режим: без ImGui-кадра — только презентация
            impl->window.swapBuffers();
            return;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        drawOverlay();

        // Консоль (тильда): ImGui-окно внизу экрана, Enter исполняет
        // строку (Console::execute внутри ConsoleWindow::draw)
        if (impl->showConsole)
        {
            ImGui::SetNextWindowPos(
                ImVec2(0.0f, static_cast<float>(impl->window.getHeight()) - consoleHeight),
                ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(consoleWidth, consoleHeight), ImGuiCond_FirstUseEver);
            impl->consoleWindow.draw();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        impl->window.swapBuffers();
    }

    void ClientCore::updateCamera(float deltaTime)
    {
        // Зум — в обоих режимах: Add — приближение (дистанция
        // уменьшается), Subtract — отдаление
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Add))
        {
            impl->camera.zoom(-cameraZoomSpeed * deltaTime);
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Subtract))
        {
            impl->camera.zoom(cameraZoomSpeed * deltaTime);
        }

        // Сетевой режим: камера следует за зеркалом игрового юнита
        // (WASD уходит в команды серверу — см. sendMovementCommand)
        if (impl->networkClient.getState() == gravelands::NetworkClient::State::Connected)
        {
            const beng::EntityID mirrorId = this->findMirror(impl->playerNetworkEntity);
            if (mirrorId != beng::invalidEntity)
            {
                beng::TransformComponent* mirrorTransform =
                    impl->world.getScene().tryGetComponent<beng::TransformComponent>(mirrorId);
                if (mirrorTransform != nullptr)
                {
                    const blib::graphics::Vector3f mirrorPosition = mirrorTransform->getWorldPosition();
                    if (!impl->cameraFollowActive)
                    {
                        // Первое следование (старт сессии, переход
                        // офлайн→сеть): снап-установка — без полёта
                        // камеры через карту к далёкому зеркалу
                        impl->camera.setTarget(mirrorPosition);
                        impl->cameraFollowActive = true;
                    }
                    else
                    {
                        // Экспоненциальное сглаживание цели: alpha —
                        // frame-rate независимый фактор (1 - e^(-k*dt))
                        const float alpha = 1.0f - std::exp(-cameraFollowSmoothing * deltaTime);
                        const blib::graphics::Vector3f smoothed =
                            impl->camera.getTarget() + (mirrorPosition - impl->camera.getTarget()) * alpha;
                        impl->camera.setTarget(smoothed);
                    }
                    impl->camera.update();
                    return;
                }
            }
            // Зеркала игрока ещё нет (первый снапшот не пришёл) —
            // камера остаётся на месте
            impl->camera.update();
            return;
        }

        // Офлайн-режим: WASD двигает цель камеры в плоскости земли;
        // флаг следования сбрасывается — при следующем подключении
        // камера снова встанет на зеркало снап-установкой
        impl->cameraFollowActive = false;

        // Офлайн-режим: WASD двигает цель камеры в плоскости земли
        // Горизонтальное направление взгляда (от камеры к цели, без Y):
        // движение WASD сдвигает цель камеры в плоскости земли, W — «вверх
        // экрана», D — «вправо экрана» (ось right = forward x up)
        blib::graphics::Vector3f lookDirection = impl->camera.getTarget() - impl->camera.getPosition();
        lookDirection.y = 0.0f;

        // lookDirection нулевой только при цели ровно под камерой —
        // при фиксированном наклоне это невозможно
        blib::graphics::Vector3f forward = blib::math::normalize(lookDirection);
        blib::graphics::Vector3f right = blib::math::normalize(
            blib::math::cross(forward, blib::graphics::Vector3f(0.0f, 1.0f, 0.0f)));

        blib::graphics::Vector3f movement(0.0f, 0.0f, 0.0f);
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::W))
        {
            movement = movement + forward;
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::S))
        {
            movement = movement - forward;
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::D))
        {
            movement = movement + right;
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::A))
        {
            movement = movement - right;
        }

        // Движение без нормализации суммы: диагональ быстрее — приемлемо
        // для отладочного управления камерой
        impl->camera.moveTarget(movement * (cameraMoveSpeed * deltaTime));

        impl->camera.update();
    }

    void ClientCore::drawOverlay()
    {
        // Полупрозрачный текст в углу: подсказка по клавишам и текущие
        // параметры света. Без рамок и заголовка, ввод не перехватывает
        ImGui::SetNextWindowPos(ImVec2(overlayPadding, overlayPadding), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(overlayBackgroundAlpha);

        const ImGuiWindowFlags overlayFlags =
            ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_NoNav
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::Begin("overlay", nullptr, overlayFlags);

        ImGui::TextUnformatted("WASD move | +/- zoom | Esc quit | ` console");
        ImGui::TextUnformatted("F5 hotreload | N normals | M toon/unlit");
        ImGui::TextUnformatted("Arrows: light dir | [ ]: light intensity");
        ImGui::TextUnformatted("PgUp/PgDn: ambient | P: post | O: outline");
        ImGui::TextUnformatted("console: scene_save [name] | scene_load [name]");
        ImGui::Separator();

        // Свет — компоненты сцены мира (см. directionalLightComponent.h):
        // показать текущие значения; компонента нет (чужой файл сцены
        // без света) — прочерк
        {
            beng::Scene& scene = impl->world.getScene();

            beng::ComponentPool<beng::DirectionalLightComponent>* directionalPool =
                scene.tryGetComponentPool<beng::DirectionalLightComponent>();
            if (directionalPool != nullptr)
            {
                for (auto it = directionalPool->begin(); it != directionalPool->end(); ++it)
                {
                    const beng::DirectionalLightComponent& lightComp = *it;
                    const blib::math::Vector<float, 3>& dir = lightComp.getDirection();
                    const float dirY = (dir.y < -1.0f) ? -1.0f : ((dir.y > 1.0f) ? 1.0f : dir.y);
                    const float azimuthDeg = blib::math::atan2(dir.z, dir.x) * lightRadToDeg;
                    const float elevationDeg = -std::asin(dirY) * lightRadToDeg;

                    ImGui::Text("light azimuth: %.0f deg", static_cast<double>(azimuthDeg));
                    ImGui::Text("light elevation: %.0f deg", static_cast<double>(elevationDeg));
                    ImGui::Text("light intensity: %.2f", static_cast<double>(lightComp.getIntensity()));
                    break;
                }
            }
            else
            {
                ImGui::Text("light: %s", overlayValueMissing);
            }

            beng::ComponentPool<beng::AmbientLightComponent>* ambientPool =
                scene.tryGetComponentPool<beng::AmbientLightComponent>();
            if (ambientPool != nullptr)
            {
                for (auto it = ambientPool->begin(); it != ambientPool->end(); ++it)
                {
                    ImGui::Text("ambient intensity: %.2f", static_cast<double>(it->getIntensity()));
                    break;
                }
            }
            else
            {
                ImGui::Text("ambient: %s", overlayValueMissing);
            }
        }

        {
            // tryGetComponent: после scene_load ссылка может устареть
            beng::MeshRenderComponent* sphereMesh =
                impl->world.getScene().tryGetComponent<beng::MeshRenderComponent>(impl->world.getSphereEntity());
            if (sphereMesh != nullptr)
            {
                const blib::graphics::Material& sphereMaterial = sphereMesh->getMesh().material;
                ImGui::Text("sphere shading: %s",
                    sphereMaterial.shadingMode == blib::graphics::ShadingMode::Toon ? "toon" : "unlit");
                ImGui::Text("sphere outline: %s", sphereMaterial.outlineEnabled ? "on" : "off");
            }
        }

        ImGui::Text("normals view: %s", impl->renderTarget.rc.showNormals ? "on" : "off");
        ImGui::Text("post-process: %s", impl->postEnabled ? "on" : "off");
        ImGui::Text("dancer model: %s",
            impl->world.getDancerEntity() != beng::invalidEntity ? "loaded" : "not loaded");
        ImGui::Text("camera distance: %.0f", static_cast<double>(impl->camera.getDistance()));
        ImGui::Text("network: %s",
            impl->networkClient.getState() == gravelands::NetworkClient::State::Connected
                ? networkConnectedLabel : networkOfflineLabel);

        ImGui::End();
    }

    void ClientCore::updateNetworkState()
    {
        impl->networkClient.poll();

        // Приветствие сессии: EntityID игрока на сервере
        WelcomePacket welcome{ 0, 0 };
        if (impl->networkClient.takeWelcome(welcome))
        {
            impl->playerNetworkEntity = welcome.playerEntityId;
            __blib_log_info("network: welcome received (tick rate %u, player entity %llu)",
                welcome.serverTickRate,
                static_cast<unsigned long long>(welcome.playerEntityId));
        }

        // Снапшоты: слив ВСЕХ накопленных за кадр в кольцевой буфер.
        // (Раньше брался только последний — при догоняющих пачках
        // тиков сервера кольцо получало редкие снапшоты с дырками в
        // десятки тиков, lerp растягивался и рвал движение/позицию.)
        SnapshotEntry entries[maxNetworkUnits];
        buint32 drainedCount = 0;
        SnapshotEntry newestEntries[maxNetworkUnits];
        buint32 newestEntryCount = 0;

        // Был ли хоть один снапшот за сессию (до слива)
        bool hadSnapshot = false;
        for (buint32 i = 0; i < snapshotBufferSize; ++i)
        {
            if (impl->snapshotValid[i])
            {
                hadSnapshot = true;
                break;
            }
        }

        for (;;)
        {
            buint32 entryCount = 0;
            buint32 tickNumber = 0;
            if (!impl->networkClient.takeSnapshot(tickNumber, entries, maxNetworkUnits, entryCount))
            {
                break;
            }

            if (!hadSnapshot)
            {
                // Первый снапшот за сессию — сеть жива: локальная
                // сфера-заглушка больше не нужна (юниты приходят зеркалами)
                impl->world.removeLocalPlayerEntity();
                hadSnapshot = true;
            }

            const buint32 slot = impl->snapshotHead;
            impl->snapshotCount[slot] = entryCount;
            impl->snapshotTick[slot] = tickNumber;
            for (buint32 i = 0; i < entryCount; ++i)
            {
                impl->snapshotBuffer[slot][i] = entries[i];
            }
            impl->snapshotValid[slot] = true;
            impl->snapshotHead = (impl->snapshotHead + 1) % snapshotBufferSize;

            // Последний слитый — новейший: цель реконсиляции предсказания
            for (buint32 i = 0; i < entryCount; ++i)
            {
                newestEntries[i] = entries[i];
            }
            newestEntryCount = entryCount;
            ++drainedCount;
        }

        if (__blib_unlikely(drainedCount > 1))
        {
            // Догоняющая пачка (кадр длиннее тика) — диагностика перфа
            __blib_log_debug("network: drained %u snapshots this frame", drainedCount);
        }

        // Реконсиляция client-side prediction по новейшему снапшоту
        this->reconcilePlayerPrediction(newestEntries, newestEntryCount);

        this->applyNetworkInterpolation();
    }

    void ClientCore::applyNetworkInterpolation()
    {
        // Новейший снапшот в буфере (по tick number, не по позиции в
        // кольце — слоты пишутся по кругу)
        bool foundNewest = false;
        buint32 newestTick = 0;
        for (buint32 i = 0; i < snapshotBufferSize; ++i)
        {
            if (!impl->snapshotValid[i])
            {
                continue;
            }
            if (!foundNewest || impl->snapshotTick[i] > newestTick)
            {
                newestTick = impl->snapshotTick[i];
                foundNewest = true;
            }
        }
        if (!foundNewest)
        {
            return; // снапшотов ещё не было
        }

        // Рендерим на snapshotRenderDelayTicks тиков позади новейшего:
        // постоянный лаг вместо «сколько успело прийти» — скорость
        // интерполяции постоянна, джиттер приёма поглощается буфером
        const buint32 targetTick = newestTick - snapshotRenderDelayTicks;

        // Пара снапшотов, охватывающая targetTick: A — ближайший к нему
        // СНИЗУ (<=), B — ближайший СВЕРХУ (>)
        bool foundA = false;
        bool foundB = false;
        buint32 indexA = 0;
        buint32 indexB = 0;
        buint32 tickA = 0;
        buint32 tickB = 0;
        for (buint32 i = 0; i < snapshotBufferSize; ++i)
        {
            if (!impl->snapshotValid[i])
            {
                continue;
            }
            const buint32 tick = impl->snapshotTick[i];
            if (tick <= targetTick && (!foundA || tick > tickA))
            {
                indexA = i;
                tickA = tick;
                foundA = true;
            }
            if (tick > targetTick && (!foundB || tick < tickB))
            {
                indexB = i;
                tickB = tick;
                foundB = true;
            }
        }

        if (foundA && foundB)
        {
            // Интерполяция между A и B по tick number: span всегда
            // кратен тиковому интервалу — движение ровное; пропущенные
            // снапшоты (gap) не рвут скорость, а растягивают переход
            float factor = 0.0f;
            if (tickB > tickA)
            {
                factor = static_cast<float>(targetTick - tickA)
                    / static_cast<float>(tickB - tickA);
            }

            for (buint32 i = 0; i < impl->snapshotCount[indexB]; ++i)
            {
                const SnapshotEntry& entryB = impl->snapshotBuffer[indexB][i];

                blib::math::Vector<float, 3> position(
                    entryB.positionX, entryB.positionY, entryB.positionZ);
                for (buint32 j = 0; j < impl->snapshotCount[indexA]; ++j)
                {
                    const SnapshotEntry& entryA = impl->snapshotBuffer[indexA][j];
                    if (entryA.entityId == entryB.entityId)
                    {
                        position.x = entryA.positionX + (entryB.positionX - entryA.positionX) * factor;
                        position.y = entryA.positionY + (entryB.positionY - entryA.positionY) * factor;
                        position.z = entryA.positionZ + (entryB.positionZ - entryA.positionZ) * factor;
                        break;
                    }
                }

                this->applyMirrorPosition(entryB.entityId, position);
            }
            return;
        }

        // Краевой случай: охватывающей пары нет. targetTick старше всех
        // (буфер только начал наполняться) — старейшее состояние; новее
        // всех (буфер отстал) — заморозка на новейшем
        const buint32 sourceIndex = foundB ? indexB : indexA;
        for (buint32 i = 0; i < impl->snapshotCount[sourceIndex]; ++i)
        {
            const SnapshotEntry& entry = impl->snapshotBuffer[sourceIndex][i];
            blib::math::Vector<float, 3> position(
                entry.positionX, entry.positionY, entry.positionZ);
            this->applyMirrorPosition(entry.entityId, position);
        }
    }

    void ClientCore::applyMirrorPosition(buint64 serverEntityId,
        _In const blib::math::Vector<float, 3>& position)
    {
        // Зеркало юнита: найти или создать, применить позицию
        beng::EntityID mirrorId = this->findMirror(serverEntityId);
        if (mirrorId == beng::invalidEntity)
        {
            mirrorId = this->createMirror(serverEntityId);
        }
        if (mirrorId == beng::invalidEntity)
        {
            return;
        }

        beng::TransformComponent* mirrorTransform =
            impl->world.getScene().tryGetComponent<beng::TransformComponent>(mirrorId);
        if (mirrorTransform != nullptr)
        {
            mirrorTransform->setLocalPosition(position);
        }
    }

    void ClientCore::sendMovementCommand()
    {
        // WASD-вектор: D/A — ось X, W/S — ось Z (W — «от камеры»,
        // к центру мира при стартовом ракурсе)
        PlayerCommand command;
        command.moveX = static_cast<bint8>(
            (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::D) ? 1 : 0) -
            (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::A) ? 1 : 0));
        command.moveZ = static_cast<bint8>(
            (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::W) ? 1 : 0) -
            (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::S) ? 1 : 0));

        // Dedup: слать только при изменении вектора
        if (!impl->lastSentCommandValid ||
            command.moveX != impl->lastSentCommand.moveX ||
            command.moveZ != impl->lastSentCommand.moveZ)
        {
            impl->networkClient.sendCommand(command);
            impl->lastSentCommand = command;
            impl->lastSentCommandValid = true;
        }

        // Текущий ввод питает локальное предсказание (каждый кадр)
        impl->predictionCommand = command;
    }

    void ClientCore::reconcilePlayerPrediction(
        _In const SnapshotEntry* entries, buint32 entryCount)
    {
        if (impl->playerNetworkEntity == beng::invalidEntity)
        {
            return; // Welcome ещё не пришёл — игрока не знаем
        }

        // Позиция игрока в новейшем слитом снапшоте
        const SnapshotEntry* playerEntry = nullptr;
        for (buint32 i = 0; i < entryCount; ++i)
        {
            if (entries[i].entityId == impl->playerNetworkEntity)
            {
                playerEntry = &entries[i];
                break;
            }
        }
        if (playerEntry == nullptr)
        {
            return;
        }

        const blib::graphics::Vector3f serverPosition(
            playerEntry->positionX, playerEntry->positionY, playerEntry->positionZ);

        if (!impl->predictionActive)
        {
            // Первый снапшот игрока: предсказание стартует от серверной
            // позиции (скачка при старте сессии нет)
            impl->predictedPosition = serverPosition;
            impl->predictionActive = true;
            return;
        }

        // Штатное расхождение = скорость × латентность команды (сервер
        // воспроизводит те же команды, просто отстаёт на кадр) — ему
        // доверяем: постоянная коррекция дала бы видимый rubber-band
        // на остановке. Снап — только при реальной рассинхронизации
        const blib::graphics::Vector3f error = impl->predictedPosition - serverPosition;
        const float errorLength = blib::math::length(error);
        if (errorLength > predictionSnapDistance)
        {
            __blib_log_debug("network: prediction snapped to server (divergence %.1f)",
                static_cast<double>(errorLength));
            impl->predictedPosition = serverPosition;
        }
    }

    void ClientCore::updatePlayerPrediction(float deltaTime)
    {
        if (impl->networkClient.getState() != gravelands::NetworkClient::State::Connected)
        {
            // Разрыв сессии — предсказание гасится; при следующем
            // подключении стартует заново от первого снапшота
            impl->predictionActive = false;
            impl->predictionCommand = PlayerCommand{ 0, 0 };
            return;
        }
        if (!impl->predictionActive)
        {
            return; // первый снапшот ещё не пришёл
        }

        // Интеграция ввода — формула 1:1 с MovementSystem сервера
        // (нормализованная диагональ, playerMoveSpeed, кламп worldBounds):
        // предсказанная траектория совпадает с серверной, просто
        // начинается раньше (сервер применяет команды с лагом кадра)
        const bint8 moveX = impl->predictionCommand.moveX;
        const bint8 moveZ = impl->predictionCommand.moveZ;
        if (moveX != 0 || moveZ != 0)
        {
            blib::graphics::Vector3f direction(
                static_cast<float>(moveX), 0.0f, static_cast<float>(moveZ));
            direction = blib::math::normalize(direction);

            blib::graphics::Vector3f position = impl->predictedPosition;
            position = position + direction * (playerMoveSpeed * deltaTime);
            if (position.x > worldBounds) position.x = worldBounds;
            if (position.x < -worldBounds) position.x = -worldBounds;
            if (position.z > worldBounds) position.z = worldBounds;
            if (position.z < -worldBounds) position.z = -worldBounds;
            impl->predictedPosition = position;
        }

        // Зеркало игрока рендерится по предсказанию — интерполированная
        // позиция (applyNetworkInterpolation) для игрока перекрывается
        const beng::EntityID mirrorId = this->findMirror(impl->playerNetworkEntity);
        if (mirrorId != beng::invalidEntity)
        {
            beng::TransformComponent* mirrorTransform =
                impl->world.getScene().tryGetComponent<beng::TransformComponent>(mirrorId);
            if (mirrorTransform != nullptr)
            {
                mirrorTransform->setLocalPosition(impl->predictedPosition);
            }
        }
    }

    beng::EntityID ClientCore::findMirror(buint64 serverEntityId)
    {
        for (buint32 i = 0; i < impl->mirrorCount; ++i)
        {
            if (impl->mirrors[i].serverEntityId == serverEntityId)
            {
                return impl->mirrors[i].clientEntityId;
            }
        }
        return beng::invalidEntity;
    }

    beng::EntityID ClientCore::createMirror(buint64 serverEntityId)
    {
        if (impl->mirrorCount >= maxNetworkUnits)
        {
            __blib_log_warning("network: mirror limit reached (%u) — entity %llu not mirrored",
                maxNetworkUnits, static_cast<unsigned long long>(serverEntityId));
            return beng::invalidEntity;
        }

        // Сфера-плейсхолдер юнита (toon, как локальная сфера-персонаж)
        blib::graphics::Sphere sphere;
        sphere.createSpere(
            playerVisualRadius, networkUnitSegments,
            blib::graphics::Color(
                networkUnitColorR, networkUnitColorG, networkUnitColorB, networkUnitColorA));

        beng::Scene& scene = impl->world.getScene();
        const beng::EntityID entity = scene.createEntity();
        beng::MeshRenderComponent& meshComponent = scene.addComponent<beng::MeshRenderComponent>(
            entity, sphere.takeMesh(), beng::RenderLayer::Opaque);
        meshComponent.getMesh().material.shadingMode = blib::graphics::ShadingMode::Toon;

        impl->mirrors[impl->mirrorCount].serverEntityId = serverEntityId;
        impl->mirrors[impl->mirrorCount].clientEntityId = entity;
        ++impl->mirrorCount;

        return entity;
    }

    bool ClientCore::isRunning() const
    {
        return impl != nullptr && impl->window.isOpen();
    }

    void ClientCore::shutdown()
    {
        if (impl == nullptr)
        {
            return;
        }

        // Вернуть оригинальную оконную процедуру до гашения ImGui
        // (паттерн model_viewer); в PIE-режиме хука не было
        if (impl->imguiEnabled)
        {
            HWND hwnd = __blib_render_window_context(impl->window.__getCtx())->hwnd;
            if (s_engineWndProc != nullptr)
            {
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_engineWndProc));
                s_engineWndProc = nullptr;
            }

            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }

        // Явный вызов деструктора + возврат памяти глобальному аллокатору.
        // Мир (сцена с мешами) разрушается внутри — раньше окна/таргета
        // (порядок членов impl): GL-контекст на момент выгрузки жив
        impl->~ClientCoreImpl();
        blib::memory::GlobalAllocator::instance().deallocate(impl, sizeof(ClientCoreImpl));
        impl = nullptr;

        __blib_log_info("%s client core shut down", gameTitle);
    }

} // namespace gravelands
