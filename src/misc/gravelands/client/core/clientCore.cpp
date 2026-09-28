#include <gravelands/client/core/clientCore.h>
#include <gravelands/world/world.h>

#include <beng/client/components/ambientLightComponent.h>
#include <beng/client/components/directionalLightComponent.h>
#include <beng/client/components/meshRenderComponent.h>
#include <beng/client/systems/animationSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/graphics/console/consoleWindow.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
#include <blib/graphics/isometricCamera.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/postprocess.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/shader.h>
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

        beng::Time time;

        // Консоль (тильда): ImGui-окно поверх blib::console::Console —
        // команды вводятся строкой (Enter), история/дополнение — ядро
        blib::graphics::console::ConsoleWindow consoleWindow;
        bool showConsole;

        ClientCoreImpl()
            : window(static_cast<uint16_t>(windowWidth), static_cast<uint16_t>(windowHeight), gameTitle)
            , renderTarget(windowWidth, windowHeight)
            , camera()
            , postProcess()
            , postEnabled(true)
            , scene()
            , transformSystem()
            , animationSystem()
            , renderSystem()
            , world()
            , time()
            , consoleWindow()
            , showConsole(false)
        {
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

    bool ClientCore::initialize()
    {
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();

        // Аллокация через GlobalAllocator + placement new (проектное правило:
        // выделяющие new/delete запрещены, placement new разрешён)
        impl = static_cast<ClientCoreImpl*>(globalAllocator.allocate(sizeof(ClientCoreImpl)));
        new (impl) ClientCoreImpl();

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

        // ImGui + WndProc-хук (паттерн model_viewer): нужен для
        // полупрозрачного оверлея-подсказки в углу
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

        // Зум: Add — приближение (дистанция уменьшается), Subtract — отдаление
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Add))
        {
            impl->camera.zoom(-cameraZoomSpeed * deltaTime);
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Subtract))
        {
            impl->camera.zoom(cameraZoomSpeed * deltaTime);
        }

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

        ImGui::End();
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
        // (паттерн model_viewer)
        HWND hwnd = __blib_render_window_context(impl->window.__getCtx())->hwnd;
        if (s_engineWndProc != nullptr)
        {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_engineWndProc));
            s_engineWndProc = nullptr;
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        // Явный вызов деструктора + возврат памяти глобальному аллокатору.
        // Мир (сцена с мешами) разрушается внутри — раньше окна/таргета
        // (порядок членов impl): GL-контекст на момент выгрузки жив
        impl->~ClientCoreImpl();
        blib::memory::GlobalAllocator::instance().deallocate(impl, sizeof(ClientCoreImpl));
        impl = nullptr;

        __blib_log_info("%s client core shut down", gameTitle);
    }

} // namespace gravelands
