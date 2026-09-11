#include <model_viewer/core/viewerCore.h>

#include <beng/client/components/animatorComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/client/systems/animationSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/components/transform.h>
#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/editor/panels/animationPanel.h>
#include <beng/editor/panels/consolePanel.h>
#include <beng/editor/panels/hierarchyPanel.h>
#include <beng/editor/panels/renderOptionsPanel.h>
#include <beng/editor/panels/viewportPanel.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/core/math/angle.h>
#include <blib/core/math/quaternion.h>
#include <blib/graphics/color.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/lineRenderer.h>
#include <blib/graphics/orbitCamera.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/skelet.h>
#include <blib/graphics/skinmodel.h>
#include <blib/system/memory/globalAllocator.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_win32.h>

#include <Windows.h>
#include <commdlg.h>
#include <gl/GL.h>

// ---------------------------------------------------------------
// ImGui-хук в оконную процедуру (как в старом main.cpp):
// ImGui обрабатывает ввод первым, остальное — движку.
// Объявлен на ГЛОБАЛЬНОМ скоупе: ImGui_ImplWin32_WndProcHandler —
// экспортируемый символ бэкенда (внутри namespace extern
// объявлял бы другую функцию)
// ---------------------------------------------------------------
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    WNDPROC s_engineWndProc = nullptr;

    LRESULT CALLBACK viewerImguiWndProc(HWND hwnd, UINT uMsg, WPARAM wparam, LPARAM lparam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wparam, lparam))
        {
            return 1;
        }

        return CallWindowProc(s_engineWndProc, hwnd, uMsg, wparam, lparam);
    }
}

namespace modelviewer
{
    namespace
    {
        // ---------------------------------------------------------------
        // Константы приложения (правило проекта: без вшитых литералов)
        // ---------------------------------------------------------------

        // Окно
        constexpr uint16_t windowWidth = 1280;
        constexpr uint16_t windowHeight = 720;
        constexpr const char* windowTitle = "Model Viewer";

        // Камера
        constexpr float cameraFovDegrees = 60.0f;
        constexpr float cameraNearDistance = 0.1f;
        constexpr float cameraFarDistance = 1000.0f;
        constexpr float cameraInitialDistance = 100.0f;
        // MD5-модели приходят с осью Z вверх, мир вьювера — Y вверх:
        // поворачиваем модель на -90° вокруг X
        constexpr float md5ZUpToYUpDegrees = -90.0f;

        // Лейаут панелей
        constexpr float leftPanelWidth = 280.0f;
        constexpr float rightPanelWidth = 320.0f;
        constexpr float modelBarHeight = 44.0f;
        constexpr float hierarchyHeightFraction = 0.65f;
        constexpr float consoleWidth = 900.0f;
        constexpr float consoleHeight = 340.0f;
        // Ширина двух кнопок верхней панели (Open + Browse) с отступами
        constexpr float modelBarButtonsWidth = 210.0f;

        // Путь к модели: буфер ввода в верхней панели
        constexpr size_t modelPathBufferSize = 512;

        // Минимальная измеряемая размерность вьюпорта (защита от
        // схлопнутого окна/панели)
        constexpr float minViewportDimension = 1.0f;

        // Строки UI
        constexpr const char* modelBarTitle = "Model";
        constexpr const char* pathInputLabel = "Path";
        constexpr const char* openButtonLabel = "Open";
        constexpr const char* browseButtonLabel = "Browse...";
        constexpr const char* fileDialogTitle = "Open 3D model";
        constexpr const char* modelFileFilter =
            "3D Models (*.md5mesh;*.fbx;*.obj;*.dae)\0*.md5mesh;*.fbx;*.obj;*.dae\0All Files (*.*)\0*.*\0";

        // Цвета скелета
        constexpr buint8 skeletonGrayComponent = 120;
        constexpr buint8 opaqueAlphaComponent = 255;
        constexpr buint8 selectedBoneRedComponent = 255;
        constexpr buint8 selectedBoneGreenComponent = 210;
        constexpr buint8 selectedBoneBlueComponent = 60;

        // Точка сброса орбитальной камеры
        constexpr float cameraTargetX = 0.0f;
        constexpr float cameraTargetY = 0.0f;
        constexpr float cameraTargetZ = 0.0f;

        // Обёртка консоли: команда clear чистит вывод
        constexpr const char* clearCommandName = "clear";
        constexpr const char* clearCommandHelp = "clears the console output";
    }

    // ---------------------------------------------------------------
    // Внутренности ядра: окно, рендер, ECS, панели, ImGui-состояние.
    // Полное определение скрыто в .cpp (pimpl) — заголовок не тянет
    // графические типы blib/beng в потребителей.
    // ---------------------------------------------------------------
    struct ViewerCore::ViewerCoreImpl
    {
        // Окно + рендер (порядок важен: окно создаёт GL-контекст,
        // рендер-таргет инициализирует GL-функции в нём)
        blib::graphics::RenderWindow window;
        blib::graphics::IRenderTarget renderTarget;
        blib::graphics::OrbitCamera camera;
        blib::graphics::LineRenderer skeletonRenderer;

        // ECS
        beng::Scene scene;
        beng::Time time;
        beng::TransformSystem transformSystem;
        beng::AnimationSystem animationSystem;
        beng::RenderSystem renderSystem;

        beng::EntityID modelEntity;

        // Панели
        beng::editor::HierarchyPanel hierarchyPanel;
        beng::editor::AnimationPanel animationPanel;
        beng::editor::ViewportPanel viewportPanel;
        beng::editor::RenderOptionsPanel renderOptionsPanel;
        beng::editor::ConsolePanel consolePanel;

        // Верхняя панель: путь к модели
        char modelPathBuffer[modelPathBufferSize];

        // Консоль
        bool showConsole;

        // Отложенный ресайз FBO под размер вьюпорт-панели (0 = нет):
        // размер измеряется в ImGui-кадре, а сцена рендерится раньше —
        // применяем в начале следующего кадра
        buint32 pendingViewportWidth;
        buint32 pendingViewportHeight;

        ViewerCoreImpl()
            : window(windowWidth, windowHeight, windowTitle)
            , renderTarget(windowWidth, windowHeight)
            , camera()
            , skeletonRenderer()
            , scene()
            , time()
            , transformSystem()
            , animationSystem()
            , renderSystem()
            , modelEntity(beng::invalidEntity)
            , hierarchyPanel()
            , animationPanel()
            , viewportPanel()
            , renderOptionsPanel()
            , consolePanel()
            , showConsole(false)
            , pendingViewportWidth(0)
            , pendingViewportHeight(0)
        {
            // Пустой буфер пути (иначе — мусор в поле ввода)
            memset(this->modelPathBuffer, 0, modelPathBufferSize);
        }
    };

    namespace
    {
        // Извлечение позиции сустава из глобальной трансформации кости:
        // трансляция хранится в последней колонке (data[i][3])
        blib::graphics::Vector3f boneWorldPosition(_In const blib::graphics::TransformMatrix& transform)
        {
            return blib::graphics::Vector3f(
                transform.data[0][3],
                transform.data[1][3],
                transform.data[2][3]);
        }
    }

    ViewerCore::ViewerCore()
        : impl(nullptr)
    {
    }

    ViewerCore::~ViewerCore()
    {
        // Страховка: если владелец не вызвал shutdown явно
        this->shutdown();
    }

    bool ViewerCore::initialize()
    {
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();

        // Аллокация через GlobalAllocator + placement new (проектное
        // правило: выделяющие new/delete запрещены)
        this->impl = static_cast<ViewerCoreImpl*>(globalAllocator.allocate(sizeof(ViewerCoreImpl)));
        new (this->impl) ViewerCoreImpl();

        // Камера: орбита вокруг начала координат
        this->impl->camera.setPerspective(
            blib::math::AngleDegreef(cameraFovDegrees),
            static_cast<float>(this->impl->window.getWight()) / static_cast<float>(this->impl->window.getHeight()),
            cameraNearDistance,
            cameraFarDistance);
        this->impl->camera.setTarget(blib::graphics::Vector3f(cameraTargetX, cameraTargetY, cameraTargetZ));
        this->impl->camera.setDistance(cameraInitialDistance);
        this->impl->camera.update();
        this->impl->renderTarget.rc.setCamera(&this->impl->camera);

        // ECS: типы компонентов и системы.
        // Системы живут в impl (Scene ими не владеет)
        this->impl->scene.registerComponentType<beng::TransformComponent>();
        this->impl->scene.registerComponentType<beng::SkinnedMeshComponent>();
        this->impl->scene.registerComponentType<beng::AnimatorComponent>();

        this->impl->scene.addSystem(&this->impl->transformSystem);
        this->impl->scene.addSystem(&this->impl->animationSystem);
        this->impl->scene.addSystem(&this->impl->renderSystem);

        this->impl->renderSystem.setRenderTarget(&this->impl->renderTarget);

        // Панели
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
        s_engineWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(viewerImguiWndProc)));

        // Команда clear: чистит и буфер ядра, и скроллбэк окна консоли.
        // Коллбэк захватывает consolePanel по ссылке — вызовы происходят
        // только внутри главного цикла, пока impl жив
        blib::console::Console::instance().registerCommand(
            clearCommandName, clearCommandHelp,
            [this](const std::vector<std::string>&)
            {
                this->impl->consolePanel.clearDisplay();
            });

        __blib_log_info("%s initialized (%ux%u window)", windowTitle, windowWidth, windowHeight);
        return true;
    }

    void ViewerCore::tick()
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

        // Горячие клавиши
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
            else
            {
                this->impl->window.close();
                return;
            }
        }
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::O) &&
            blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LControl))
        {
            this->browseModelFile();
        }

        // Симуляция + рендер сцены в FBO (RenderSystem внутри update).
        // Флаг диффузных текстур читается при отрисовке мешей
        this->impl->renderTarget.rc.useDiffuseTextures =
            this->impl->renderOptionsPanel.isDiffuseTextureVisible();

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

        this->impl->renderTarget.clear(blib::graphics::Color::Black);
        this->impl->scene.update(deltaTime);

        // Отладочные слои поверх сцены (в тот же FBO)
        if (this->impl->renderOptionsPanel.isSkeletonVisible())
        {
            this->drawSkeleton();
        }
        if (this->impl->renderOptionsPanel.isWireframeVisible())
        {
            this->drawWireframe();
        }

        // UI: переключаемся на back-буфер (иначе ImGui-бэкенд
        // рисует в FBO, а вьюпорт сэмплит его же — feedback loop)
        this->impl->renderTarget.rc.api.ogl.ext.__blib_gl_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        this->impl->renderTarget.rc.api.ogl.__blib_gl_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        this->drawPanels();

        if (this->impl->showConsole)
        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, static_cast<float>(this->impl->window.getHeight()) - consoleHeight), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(consoleWidth, consoleHeight), ImGuiCond_FirstUseEver);
            this->impl->consolePanel.draw();
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

    bool ViewerCore::isRunning() const
    {
        return this->impl != nullptr && this->impl->window.isOpen();
    }

    void ViewerCore::shutdown()
    {
        if (this->impl == nullptr)
        {
            return;
        }

        // Модель снимаем заранее: панели ссылаются на её данные
        this->unloadModel();

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

        // Явный деструктор + возврат памяти глобальному аллокатору
        this->impl->~ViewerCoreImpl();
        blib::memory::GlobalAllocator::instance().deallocate(this->impl, sizeof(ViewerCoreImpl));
        this->impl = nullptr;

        __blib_log_info("%s shut down", windowTitle);
    }

    bool ViewerCore::loadModelFromFile(_In const std::string& path)
    {
        if (__blib_unlikely(this->impl == nullptr))
        {
            return false;
        }

        this->loadModel(path);
        return this->impl->modelEntity != beng::invalidEntity;
    }

    void ViewerCore::loadModel(_In const std::string& path)
    {
        // Сначала выгружаем предыдущую модель (и отвязываем панели)
        this->unloadModel();

        // Entity модели: Transform (размещение) + SkinnedMesh (данные)
        // + Animator (плейбек)
        const beng::EntityID entity = this->impl->scene.createEntity();
        this->impl->scene.addComponent<beng::TransformComponent>(entity, &this->impl->scene);
        beng::SkinnedMeshComponent& meshComp = this->impl->scene.addComponent<beng::SkinnedMeshComponent>(entity);
        beng::AnimatorComponent& animComp = this->impl->scene.addComponent<beng::AnimatorComponent>(entity);

        if (__blib_unlikely(!meshComp.loadFromFile(path)))
        {
            __blib_log_error("failed to load model '%s'", path.c_str());
            this->impl->scene.destroyEntity(entity);
            return;
        }

        blib::graphics::SkinModel* model = meshComp.getModel();

        // MD5 (и большинство форматов скелетных моделей) — Z вверх,
        // мир вьювера — Y вверх: поворот на -90° вокруг X
        beng::TransformComponent& transform = this->impl->scene.getComponent<beng::TransformComponent>(entity);
        transform.setLocalRotation(blib::math::Quaternion<float>(
            blib::math::AngleDegreef(md5ZUpToYUpDegrees),
            blib::math::Vector<float, 3>(1.0f, 0.0f, 0.0f)));

        // Плейбек: зацикливание по умолчанию включено, анимация НЕ
        // стартует — модель рисуется как есть (bind-поза, ТЗ)
        animComp.setAnimator(&model->getAnimator());
        animComp.setLoop(true);

        // Панели привязываем к новой модели
        this->impl->hierarchyPanel.setSkelet(&model->getSkelet());
        this->impl->animationPanel.setAnimatorComponent(&animComp);

        this->impl->modelEntity = entity;

        // Камеру возвращаем к модели (цель — в начало координат)
        this->impl->camera.setTarget(blib::graphics::Vector3f(cameraTargetX, cameraTargetY, cameraTargetZ));
        this->impl->camera.setDistance(cameraInitialDistance);
        this->impl->camera.update();

        __blib_log_info("model loaded: %s", path.c_str());
    }

    void ViewerCore::unloadModel()
    {
        if (this->impl->modelEntity == beng::invalidEntity)
        {
            return;
        }

        // Уничтожение сущности разрушает её компоненты, включая
        // SkinnedMeshComponent (выгружает SkinModel)
        this->impl->scene.destroyEntity(this->impl->modelEntity);
        this->impl->modelEntity = beng::invalidEntity;

        // Панели больше не должны ссылаться на модель
        this->impl->hierarchyPanel.setSkelet(nullptr);
        this->impl->animationPanel.setAnimatorComponent(nullptr);
    }

    void ViewerCore::browseModelFile()
    {
        // Стандартный диалог выбора файла (Win32). Буфер MAX_PATH —
        // системная константа
        char pathBuffer[MAX_PATH] = "";
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = __blib_render_window_context(this->impl->window.__getCtx())->hwnd;
        ofn.lpstrFilter = modelFileFilter;
        ofn.lpstrFile = pathBuffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = fileDialogTitle;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

        if (GetOpenFileNameA(&ofn))
        {
            // Путь показываем в поле ввода и сразу загружаем
            strncpy_s(this->impl->modelPathBuffer, modelPathBufferSize, pathBuffer, _TRUNCATE);
            this->loadModel(std::string(this->impl->modelPathBuffer));
        }
    }

    void ViewerCore::drawSkeleton()
    {
        if (this->impl->modelEntity == beng::invalidEntity)
        {
            return;
        }

        beng::SkinnedMeshComponent* meshComp =
            this->impl->scene.tryGetComponent<beng::SkinnedMeshComponent>(this->impl->modelEntity);
        if (__blib_unlikely(!meshComp || !meshComp->getModel()))
        {
            return;
        }

        blib::graphics::SkinModel* model = meshComp->getModel();
        const blib::graphics::Skelet& skelet = model->getSkelet();

        const blib::graphics::Color skeletonColor(skeletonGrayComponent, skeletonGrayComponent, skeletonGrayComponent, opaqueAlphaComponent);
        const blib::graphics::Color selectedBoneColor(selectedBoneRedComponent, selectedBoneGreenComponent, selectedBoneBlueComponent, opaqueAlphaComponent);
        const blib::graphics::Bone* selectedBone = this->impl->hierarchyPanel.getSelectedBone();

        this->impl->skeletonRenderer.clear();

        // Отрезок от родительского сустава к суставу кости.
        // Выбранная кость подсвечивается: яркими рисуются все
        // сегменты, касающиеся её (входящий и исходящие)
        for (const blib::graphics::Bone& bone : skelet.getBoneStorage())
        {
            const blib::graphics::IHierarchal* parent = bone.getParent();
            if (!parent)
            {
                continue;
            }

            const blib::graphics::Bone* parentBone = static_cast<const blib::graphics::Bone*>(parent);
            const bool isHighlighted = (selectedBone == &bone || selectedBone == parentBone);

            this->impl->skeletonRenderer.addLine(
                boneWorldPosition(parentBone->globalTransform),
                boneWorldPosition(bone.globalTransform),
                isHighlighted ? selectedBoneColor : skeletonColor);
        }

        // Линии живут в локальном пространстве модели — применяем
        // ту же трансформацию, что использовал RenderSystem
        this->impl->skeletonRenderer.setTransform(model->getTransform());

        // Скелет рисуем поверх меша (X-ray): выключаем тест глубины
        // на время отрисовки и возвращаем его обратно
        this->impl->renderTarget.rc.api.ogl.__blib_glDisable(GL_DEPTH_TEST);
        this->impl->renderTarget.draw(this->impl->skeletonRenderer);
        this->impl->renderTarget.rc.api.ogl.__blib_glEnable(GL_DEPTH_TEST);
    }

    void ViewerCore::drawWireframe()
    {
        if (this->impl->modelEntity == beng::invalidEntity)
        {
            return;
        }

        beng::SkinnedMeshComponent* meshComp =
            this->impl->scene.tryGetComponent<beng::SkinnedMeshComponent>(this->impl->modelEntity);
        if (__blib_unlikely(!meshComp || !meshComp->getModel()))
        {
            return;
        }

        blib::graphics::RenderContext& rc = this->impl->renderTarget.rc;
        const bool fillUsesTextures = this->impl->renderOptionsPanel.isDiffuseTextureVisible();

        // Линии рёбер рисуем поверх заливки: GL_LINE + полигонный
        // офсет подтягивает линии к камере, чтобы они не зефитили
        rc.api.ogl.__blib_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        rc.api.ogl.__blib_glEnable(GL_POLYGON_OFFSET_LINE);
        rc.api.ogl.__blib_glPolygonOffset(-1.0f, -1.0f);

        // Контраст к заливке: если заливка текстурная — линии плоские
        // белые, если заливка плоская (текстуры выключены) — линии
        // текстурные (иначе белые линии не видны на белой заливке)
        rc.useDiffuseTextures = !fillUsesTextures;
        this->impl->renderTarget.draw(*meshComp->getModel());
        rc.useDiffuseTextures = fillUsesTextures;

        rc.api.ogl.__blib_glDisable(GL_POLYGON_OFFSET_LINE);
        rc.api.ogl.__blib_glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    void ViewerCore::drawPanels()
    {
        const float windowW = static_cast<float>(this->impl->window.getWight());
        const float windowH = static_cast<float>(this->impl->window.getHeight());

        // Слева: иерархия костей + опции рендера
        const float hierarchyHeight = windowH * hierarchyHeightFraction;
        ImGui::SetNextWindowPos(ImVec2(0.0f, modelBarHeight), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, hierarchyHeight - modelBarHeight), ImGuiCond_FirstUseEver);
        this->impl->hierarchyPanel.draw();

        ImGui::SetNextWindowPos(ImVec2(0.0f, hierarchyHeight), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, windowH - hierarchyHeight), ImGuiCond_FirstUseEver);
        this->impl->renderOptionsPanel.draw();

        // Справа: таблица анимаций с плейбеком
        ImGui::SetNextWindowPos(ImVec2(windowW - rightPanelWidth, modelBarHeight), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, windowH - modelBarHeight), ImGuiCond_FirstUseEver);
        this->impl->animationPanel.draw();

        // Верхняя панель: путь к модели + кнопки загрузки
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(windowW, modelBarHeight), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(modelBarTitle, nullptr, ImGuiWindowFlags_NoResize))
        {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - modelBarButtonsWidth);
            const bool enterPressed = ImGui::InputText(
                pathInputLabel, this->impl->modelPathBuffer, modelPathBufferSize,
                ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();
            const bool openClicked = ImGui::Button(openButtonLabel);

            ImGui::SameLine();
            const bool browseClicked = ImGui::Button(browseButtonLabel);

            if ((enterPressed || openClicked) && this->impl->modelPathBuffer[0] != '\0')
            {
                this->loadModel(std::string(this->impl->modelPathBuffer));
            }
            if (browseClicked)
            {
                this->browseModelFile();
            }
        }
        ImGui::End();

        // Центр: вьюпорт
        ImGui::SetNextWindowPos(ImVec2(leftPanelWidth, modelBarHeight), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(windowW - leftPanelWidth - rightPanelWidth, windowH - modelBarHeight), ImGuiCond_FirstUseEver);
        this->impl->viewportPanel.draw();
    }

} // namespace modelviewer
