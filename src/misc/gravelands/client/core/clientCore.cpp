#include <gravelands/client/core/clientCore.h>
#include <gravelands/client/core/isometricTileset.h>

#include <beng/client/components/ambientLightComponent.h>
#include <beng/client/components/animatorComponent.h>
#include <beng/client/components/blobShadowComponent.h>
#include <beng/client/components/directionalLightComponent.h>
#include <beng/client/components/meshRenderComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/client/systems/animationSystem.h>
#include <beng/client/systems/blobShadowSystem.h>
#include <beng/client/systems/lightSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/core/fileStream.h>
#include <blib/core/math/quaternion.h>
#include <blib/core/math/trigonometry.h>
#include <blib/core/resource/resourceManager.h>
#include <blib/graphics/blobShadow.h>
#include <blib/graphics/color.h>
#include <blib/graphics/console/consoleWindow.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
#include <blib/graphics/isometricCamera.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/postprocess.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/shader.h>
#include <blib/graphics/skinmodel.h>
#include <blib/graphics/sphere.h>
#include <blib/graphics/spritePlane.h>
#include <blib/system/memory/globalAllocator.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_win32.h>

#include <cmath>
#include <fstream>
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

        // Тестовая сфера-«персонаж» (фаза 2 NPR-пайплайна): радиус,
        // число сегментов экватора и цвет поверхности
        constexpr float testSphereRadius = 25.0f;
        constexpr buint32 testSphereSegments = 24;
        constexpr buint8 testSphereColorR = 200;
        constexpr buint8 testSphereColorG = 160;
        constexpr buint8 testSphereColorB = 110;
        constexpr buint8 testSphereColorA = 255;

        // Сторона шахматной текстуры сферы и тёмная клетка шахматки:
        // unlit-режим (M) показывает узор, toon — узор + свет
        // (однотонная 1x1 текстура сделала бы сравнение бессмысленным)
        constexpr buint16 testSphereTextureSize = 16;
        constexpr buint8 testSphereDarkR = 140;
        constexpr buint8 testSphereDarkG = 105;
        constexpr buint8 testSphereDarkB = 65;

        // Свет по умолчанию (согласовано с RenderContext-дефолтами):
        // азимут/элевация источника в градусах, интенсивность
        constexpr float defaultLightAzimuthDeg = 30.0f;
        constexpr float defaultLightElevationDeg = 55.0f;
        constexpr float defaultLightIntensity = 0.9f;
        constexpr float defaultAmbientIntensity = 0.5f;

        // Скорости правки света клавишами
        constexpr float lightRotateSpeedDeg = 60.0f;
        constexpr float lightIntensitySpeed = 0.75f;
        constexpr float lightMinIntensity = 0.0f;
        constexpr float lightMaxIntensity = 3.0f;
        constexpr float lightMaxElevationDeg = 89.0f;
        constexpr float lightMinElevationDeg = 5.0f;

        // Эмбиент (PageUp/PageDown): скорость и лимиты интенсивности
        constexpr float ambientIntensitySpeed = 0.5f;
        constexpr float ambientMinIntensity = 0.0f;
        constexpr float ambientMaxIntensity = 2.0f;

        // Имя консольной команды перезагрузки шейдеров (см. F5)
        constexpr const char* hotreloadCommand = "hotreload";

        // Консоль (тильда `~`): команды сохранения/загрузки сцены.
        // Путь — первый аргумент; без аргумента — дефолтное имя файла
        constexpr const char* sceneSaveCommandName = "scene_save";
        constexpr const char* sceneSaveCommandHelp =
            "saves the world scene to a JSON file (arg: path, default: gravelands_scene.json)";
        constexpr const char* sceneLoadCommandName = "scene_load";
        constexpr const char* sceneLoadCommandHelp =
            "loads the world scene from a JSON file (arg: path, default: gravelands_scene.json)";
        constexpr const char* sceneDefaultFilePath = "gravelands_scene.json";

        // Консольное окно (тильда): размер и позиция внизу окна игры
        constexpr float consoleWidth = 900.0f;
        constexpr float consoleHeight = 340.0f;

        // Отступ и прозрачность оверлея-подсказки (полупрозрачный текст
        // в углу — вместо отладочной панели, см. фазу 4)
        constexpr float overlayPadding = 10.0f;
        constexpr float overlayBackgroundAlpha = 0.35f;

        // Тестовые рисованные плоскости-«деревья» (фаза 5, NPR-гибрид):
        // unlit-квады с alpha-test вокруг сферы. Поворот к камере
        // вычисляется в initialize() (камера фиксирована) — см. SpritePlane
        constexpr buint32 testTreeCount = 3;
        constexpr float testTreeWidth = 40.0f;
        constexpr float testTreeHeight = 70.0f;

        // Радианы → градусы (для поворота плоскостей к камере)
        constexpr float treeRadToDeg = 57.29577951f;

        // Позиции деревьев на плоскости XZ (нога в земле, y = 0)
        constexpr float testTreePositions[testTreeCount][2] = {
            { -70.0f, -50.0f },
            { 60.0f, -20.0f },
            { -30.0f, 60.0f }
        };

        // Сторона процедурной текстуры дерева (пикселей)
        constexpr buint16 treeTextureSize = 64;

        // Параметры процедурной текстуры дерева (см. generateTreeImage):
        // ствол и крона в долях от размера текстуры
        constexpr float treeTrunkHalfWidth = 4.0f;
        constexpr float treeTrunkTopY = 20.0f;
        constexpr float treeCanopyCenterX = 32.0f;
        constexpr float treeCanopyCenterY = 26.0f;
        constexpr float treeCanopyRadius = 18.0f;
        constexpr float treeHighlightRadius = 11.0f;
        constexpr float treeHighlightOffset = 3.0f;

        // Цвета дерева: тёмно-зелёная крона со светлым пятном-объёмом
        // и коричневый ствол (рисованная манера, плоские цвета)
        constexpr buint8 treeCanopyR = 55;
        constexpr buint8 treeCanopyG = 105;
        constexpr buint8 treeCanopyB = 55;
        constexpr buint8 treeHighlightR = 95;
        constexpr buint8 treeHighlightG = 155;
        constexpr buint8 treeHighlightB = 80;
        constexpr buint8 treeTrunkR = 110;
        constexpr buint8 treeTrunkG = 70;
        constexpr buint8 treeTrunkB = 40;
        constexpr buint8 treeColorAlpha = 255;

        // Blob-тени (фаза 7): радиусы под сферой и деревьями, подъём
        // тени над землёй (защита от z-fighting с тайлами)
        constexpr float sphereShadowRadius = 30.0f;
        constexpr float treeShadowRadius = 16.0f;
        constexpr float shadowHeightOffset = 0.5f;

        // Параметры процедурного градиента тени: внутренняя граница
        // непрозрачности, внешняя граница нуля и максимальная альфа
        // (тень не полностью чёрная — мягкое затемнение)
        constexpr buint16 shadowTextureSize = 64;
        constexpr float shadowInnerEdge = 0.35f;
        constexpr float shadowOuterEdge = 0.95f;
        constexpr buint8 shadowMaxAlpha = 150;

        // Тестовая скелетная модель-«танцор» (фаза 9): Mixamo-FBX
        // с анимацией. Масштаб: Mixamo ~180 ед. роста → 0.1 (~18 ед.
        // сетки). Поворот — к камере КВАТЕРНИОНОМ (см. ниже)
        constexpr const char* dancerModelPath = "resources\\Hip Hop Dancing.fbx";
        constexpr float dancerScale = 0.1f;
        constexpr float dancerPositionX = 0.0f;
        constexpr float dancerPositionZ = 60.0f;

        // Логический ключ тайлового меша в кеше ресурсов сцены
        // (ResourceManager: dedup по содержимому, разделение слота)
        constexpr const char* tilesResourceKey = "gravelands.tiles";

        // Поворот танцора к камере КВАТЕРНИОНОМ (стандартная конвенция:
        // угол θ вокруг +Y отображает +Z в (sinθ, 0, cosθ); камера на
        // +X+Z → +45°). Если модель в FBX смотрит фронтом в −Z — танцор
        // окажется спиной к камере, тогда вернуть знак (см. CORE.md)
        constexpr float dancerYawDegrees = 45.0f;
        constexpr float dancerOutlineWidth = 0.25f;
        constexpr float dancerShadowRadius = 8.0f;

        // Кость, за которой следует тень танцора (root-motion танца
        // живёт в позе; система пробует также Hips/mixamorig:Hips)
        constexpr const char* dancerShadowBoneName = "mixamorig:Hips";
    }

    // Процедурная текстура blob-тени (фаза 7): радиальный градиент
    // чёрного с мягким краем — альфа канала решает затемнение
    // (блендинг в BlobShadow::draw), rgb нулевые
    void generateShadowImage(_Out blib::graphics::Image& outImage)
    {
        outImage.create(shadowTextureSize, shadowTextureSize, blib::graphics::Color::Transparent);

        const float center = static_cast<float>(shadowTextureSize) * 0.5f;

        for (buint16 y = 0; y < shadowTextureSize; ++y)
        {
            for (buint16 x = 0; x < shadowTextureSize; ++x)
            {
                // Дистанция от центра, нормализованная к половине стороны
                const float dx = (static_cast<float>(x) - center) / center;
                const float dy = (static_cast<float>(y) - center) / center;
                const float dist = std::sqrt(dx * dx + dy * dy);

                // Линейное затухание между внутренней и внешней границей
                float edge = 1.0f - (dist - shadowInnerEdge) / (shadowOuterEdge - shadowInnerEdge);
                if (edge < 0.0f)
                {
                    edge = 0.0f;
                }
                if (edge > 1.0f)
                {
                    edge = 1.0f;
                }

                const buint8 alpha = static_cast<buint8>(edge * static_cast<float>(shadowMaxAlpha));
                outImage[x][y] = blib::graphics::Color(0, 0, 0, alpha);
            }
        }
    }

    // Проверка существования файла (для резолва контента)
    bool contentFileExists(const std::string& path)
    {
        std::ifstream fin(path, std::ios::in);
        return fin.is_open();
    }

    // Резолвит путь к контенту игры: cwd → каталог exe → подъём по
    // родителям (dev-раскладка: exe в build\...\Debug, ресурсы в
    // <корне репо>\resources). Тот же принцип, что у шейдеров в blib
    // (Shader::compile) — см. GRAPHICS.md
    std::string resolveContentPath(const std::string& relativePath)
    {
        constexpr size_t maxPathLength = 1024;
        constexpr buint32 maxWalkUpLevels = 8;

        if (contentFileExists(relativePath))
        {
            return relativePath;
        }

        char exePathRaw[maxPathLength] = { 0 };
        const DWORD length = GetModuleFileNameA(nullptr, exePathRaw, static_cast<DWORD>(maxPathLength));
        if (length == 0 || length >= maxPathLength)
        {
            return relativePath;
        }

        // Отрезаем имя exe, оставляя каталог с завершающим разделителем
        for (DWORD i = length; i > 0; --i)
        {
            const char c = exePathRaw[i - 1];
            if (c == '\\' || c == '/')
            {
                exePathRaw[i] = '\0';
                break;
            }
        }

        std::string ancestor(exePathRaw);

        const std::string exeCandidate = ancestor + relativePath;
        if (contentFileExists(exeCandidate))
        {
            return exeCandidate;
        }

        // Подъём по родителям exe: на каждом уровне — <ancestor>\<path>
        for (buint32 level = 0; level < maxWalkUpLevels; ++level)
        {
            if (ancestor.empty())
            {
                break;
            }

            size_t pos = ancestor.size() - 1;
            if (ancestor[pos] == '\\' || ancestor[pos] == '/')
            {
                ancestor.erase(pos);
            }
            if (ancestor.empty())
            {
                break;
            }

            pos = ancestor.find_last_of("\\/");
            if (pos == std::string::npos)
            {
                ancestor.clear();
                break;
            }
            ancestor.erase(pos + 1);

            const std::string candidate = ancestor + relativePath;
            if (contentFileExists(candidate))
            {
                return candidate;
            }
        }

        // Не найдено: возвращаем как есть — лог загрузки покажет путь
        return relativePath;
    }

    // Процедурная текстура «дерева» для тестовых плоскостей (фаза 5):
    // ствол + крона со светлым пятном на прозрачном фоне. Альфа-канал
    // = форма кроны: alpha-test отбросит фон и оставит жёсткий край
    void generateTreeImage(_Out blib::graphics::Image& outImage)
    {
        outImage.create(treeTextureSize, treeTextureSize, blib::graphics::Color::Transparent);

        const blib::graphics::Color canopyColor(treeCanopyR, treeCanopyG, treeCanopyB, treeColorAlpha);
        const blib::graphics::Color highlightColor(treeHighlightR, treeHighlightG, treeHighlightB, treeColorAlpha);
        const blib::graphics::Color trunkColor(treeTrunkR, treeTrunkG, treeTrunkB, treeColorAlpha);

        for (buint16 y = 0; y < treeTextureSize; ++y)
        {
            for (buint16 x = 0; x < treeTextureSize; ++x)
            {
                const float fx = static_cast<float>(x);
                const float fy = static_cast<float>(y);

                // ВАЖНО: Image::operator[] индексируется [колонка][строка]
                // (image[x][y] = bitmap[y * width + x], см. image.h) —
                // здесь x — колонка, y — строка

                // Ствол: вертикальная полоса у нижней кромки
                if (fy <= treeTrunkTopY &&
                    fx >= treeCanopyCenterX - treeTrunkHalfWidth &&
                    fx <= treeCanopyCenterX + treeTrunkHalfWidth)
                {
                    outImage[x][y] = trunkColor;
                    continue;
                }

                // Крона: круг + светлое пятно (намёк на объём)
                const float dx = fx - treeCanopyCenterX;
                const float dy = fy - treeCanopyCenterY;
                if (dx * dx + dy * dy <= treeCanopyRadius * treeCanopyRadius)
                {
                    const float hx = fx - (treeCanopyCenterX + treeHighlightOffset);
                    const float hy = fy - (treeCanopyCenterY + treeHighlightOffset);

                    outImage[x][y] =
                        (hx * hx + hy * hy <= treeHighlightRadius * treeHighlightRadius)
                        ? highlightColor
                        : canopyColor;
                }
            }
        }
    }

    // Внутренности клиента: окно, рендер-таргет, изокамера, тайлы, таймер.
    // Полное определение скрыто в .cpp (pimpl) — заголовок не тянет
    // графические типы blib в потребителей.
    struct ClientCore::ClientCoreImpl
    {
        blib::graphics::RenderWindow window;
        blib::graphics::IRenderTarget renderTarget;
        blib::graphics::IsometricCamera camera;
        blib::graphics::PostProcess postProcess;
        bool postEnabled;

        // Весь мир — в ECS-сцене: отрисовка идёт ТОЛЬКО через
        // scene.update() (RenderSystem). Объявлена ПОСЛЕ графических
        // объектов — разрушается РАНЬШЕ окна/таргета (меши освобождают
        // GL-ресурсы при живом контексте)
        beng::Scene scene;
        beng::TransformSystem transformSystem;
        beng::AnimationSystem animationSystem;
        beng::BlobShadowSystem blobShadowSystem;
        beng::LightSystem lightSystem;
        beng::RenderSystem renderSystem;

        // Сущности мира (для отладочных клавиш и статуса в оверлее)
        beng::EntityID sphereEntity = beng::invalidEntity;
        beng::EntityID sphereShadowEntity = beng::invalidEntity;
        beng::EntityID dancerShadowEntity = beng::invalidEntity;
        beng::EntityID treeEntities[testTreeCount] = {};
        beng::EntityID treeShadowEntities[testTreeCount] = {};
        beng::EntityID dancerEntity = beng::invalidEntity;

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
            , blobShadowSystem()
            , lightSystem()
            , renderSystem()
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

        // Консольные команды графики: "hotreload" / "reload_shaders" —
        // перекомпиляция шейдеров с диска без перезапуска (см. F5)
        blib::graphics::registerGraphicsConsoleCommands();

        // Консольные команды сцены: сохранение/загрузка мира в JSON
        // (проверка Scene::save/load из игры; консоль — по тильде).
        // Коллбэки захватывают this — вызовы идут из главного цикла
        // (ConsoleWindow рисуется в ImGui-кадре), пока impl жив
        blib::console::Console::instance().registerCommand(
            sceneSaveCommandName, sceneSaveCommandHelp,
            [this](const std::vector<std::string>& args) { this->saveSceneCommand(args); });
        blib::console::Console::instance().registerCommand(
            sceneLoadCommandName, sceneLoadCommandHelp,
            [this](const std::vector<std::string>& args) { this->loadSceneCommand(args); });

        // Рендер-ECS (beng-client, фаза 9): сцена + системы анимации
        // и отрисовки. ВЕСЬ мир строится сущностями и рисуется только
        // через scene.update() (инвариант — см. BENG.md «beng-client»).
        // Регистрация типов — строго до запуска цикла (реестр не
        // thread-safe, см. BENG.md). TransformComponent регистрируется
        // сценой автоматически (инвариант: каждая сущность рождается
        // с Transform) — явная регистрация запрещена
        impl->scene.registerComponentType<beng::SkinnedMeshComponent>();
        impl->scene.registerComponentType<beng::AnimatorComponent>();
        impl->scene.registerComponentType<beng::MeshRenderComponent>();
        impl->scene.registerComponentType<beng::BlobShadowComponent>();
        impl->scene.registerComponentType<beng::DirectionalLightComponent>();
        impl->scene.registerComponentType<beng::AmbientLightComponent>();

        impl->scene.addSystem(&impl->transformSystem);
        impl->scene.addSystem(&impl->animationSystem);
        impl->scene.addSystem(&impl->blobShadowSystem);
        impl->scene.addSystem(&impl->lightSystem);
        impl->scene.addSystem(&impl->renderSystem);

        impl->renderSystem.setRenderTarget(&impl->renderTarget);
        impl->lightSystem.setRenderTarget(&impl->renderTarget);

        // Мир: тайлы, сфера, деревья, тени (сущности + слои рендера)
        setupWorld();

        // Скелетная модель с анимацией (Mixamo-FBX)
        loadDancerModel();

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

            // M — переключение сферы unlit/toon (сравнение до/после света)
            if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::M))
            {
                // tryGetComponent: после scene_load ссылка на сущность
                // может устареть (ID из другого файла) — не fatal
                beng::MeshRenderComponent* sphereMesh =
                    impl->scene.tryGetComponent<beng::MeshRenderComponent>(impl->sphereEntity);
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
                    impl->scene.tryGetComponent<beng::MeshRenderComponent>(impl->sphereEntity);
                if (sphereMesh != nullptr)
                {
                    blib::graphics::Material& sphereMaterial = sphereMesh->getMesh().material;
                    sphereMaterial.outlineEnabled = !sphereMaterial.outlineEnabled;
                }
            }
        }

        updateCamera(simDeltaTime);
        updateLight(simDeltaTime);

        // Единственная точка отрисовки мира: вся сцена (тайлы, тени,
        // плоскости, сфера, скелетная модель) рисуется RenderSystem'ом
        // внутри scene.update() по слоям (см. RenderLayer)
        impl->renderTarget.clear(blib::graphics::Color::Black);
        impl->scene.update(simDeltaTime);

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

    void ClientCore::updateLight(float deltaTime)
    {
        // Свет — компоненты сцены (см. directionalLightComponent.h /
        // ambientLightComponent.h): отладочные клавиши крутят компонент
        // напрямую (единственный источник истины), применение в
        // RenderContext делает LightSystem внутри scene.update().
        // Доступ через пул, без хранения EntityID — устойчиво к
        // scene_load (ID из файла могут не совпасть)

        // Направленный свет: стрелки — азимут/элевация, [ ] —
        // интенсивность. Углы — отладочный интерфейс: обратный расчёт
        // из direction (atan2/asin), после правки direction
        // пересчитывается заново (без накопления дрейфа)
        beng::ComponentPool<beng::DirectionalLightComponent>* directionalPool =
            impl->scene.tryGetComponentPool<beng::DirectionalLightComponent>();
        if (directionalPool != nullptr)
        {
            for (auto it = directionalPool->begin(); it != directionalPool->end(); ++it)
            {
                beng::DirectionalLightComponent& lightComp = *it;

                const blib::math::Vector<float, 3>& dir = lightComp.getDirection();
                // direction.y = -sin(elevation) → elevation = asin(-y).
                // Аргумент asin клампится в [-1, 1] (направление
                // нормировано по конвенции, но файл может быть чужим)
                const float dirY = (dir.y < -1.0f) ? -1.0f : ((dir.y > 1.0f) ? 1.0f : dir.y);
                float azimuthDeg = blib::math::atan2(dir.z, dir.x) * treeRadToDeg;
                float elevationDeg = -std::asin(dirY) * treeRadToDeg;
                float intensity = lightComp.getIntensity();

                if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Left))
                {
                    azimuthDeg -= lightRotateSpeedDeg * deltaTime;
                }
                if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Right))
                {
                    azimuthDeg += lightRotateSpeedDeg * deltaTime;
                }
                if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Up))
                {
                    elevationDeg += lightRotateSpeedDeg * deltaTime;
                    if (elevationDeg > lightMaxElevationDeg)
                    {
                        elevationDeg = lightMaxElevationDeg;
                    }
                }
                if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Down))
                {
                    elevationDeg -= lightRotateSpeedDeg * deltaTime;
                    if (elevationDeg < lightMinElevationDeg)
                    {
                        elevationDeg = lightMinElevationDeg;
                    }
                }

                if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LBracket))
                {
                    intensity -= lightIntensitySpeed * deltaTime;
                    if (intensity < lightMinIntensity)
                    {
                        intensity = lightMinIntensity;
                    }
                }
                if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::RBracket))
                {
                    intensity += lightIntensitySpeed * deltaTime;
                    if (intensity > lightMaxIntensity)
                    {
                        intensity = lightMaxIntensity;
                    }
                }

                // Пересчёт direction: направление ИЗ источника К
                // поверхности (вниз к земле), как было до компонентов
                const float azimuthRad = blib::math::AngleDegreef(azimuthDeg).toRadian().data;
                const float elevationRad = blib::math::AngleDegreef(elevationDeg).toRadian().data;
                lightComp.setDirection(blib::math::Vector<float, 3>(
                    blib::math::cos(elevationRad) * blib::math::cos(azimuthRad),
                    -blib::math::sin(elevationRad),
                    blib::math::cos(elevationRad) * blib::math::sin(azimuthRad)));
                lightComp.setIntensity(intensity);

                break; // один направленный источник (ограничение RenderContext)
            }
        }

        // Эмбиент: PageUp/PageDown — интенсивность
        beng::ComponentPool<beng::AmbientLightComponent>* ambientPool =
            impl->scene.tryGetComponentPool<beng::AmbientLightComponent>();
        if (ambientPool != nullptr)
        {
            for (auto it = ambientPool->begin(); it != ambientPool->end(); ++it)
            {
                beng::AmbientLightComponent& ambientComp = *it;
                float intensity = ambientComp.getIntensity();

                if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::PageUp))
                {
                    intensity += ambientIntensitySpeed * deltaTime;
                    if (intensity > ambientMaxIntensity)
                    {
                        intensity = ambientMaxIntensity;
                    }
                }
                if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::PageDown))
                {
                    intensity -= ambientIntensitySpeed * deltaTime;
                    if (intensity < ambientMinIntensity)
                    {
                        intensity = ambientMinIntensity;
                    }
                }

                ambientComp.setIntensity(intensity);
                break; // один эмбиент (ограничение RenderContext)
            }
        }
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

        // Свет — компоненты сцены (см. directionalLightComponent.h):
        // показать текущие значения (фолбэк — дефолтные константы,
        // если пула/компонента нет)
        {
            float azimuthDeg = defaultLightAzimuthDeg;
            float elevationDeg = defaultLightElevationDeg;
            float intensity = defaultLightIntensity;

            beng::ComponentPool<beng::DirectionalLightComponent>* directionalPool =
                impl->scene.tryGetComponentPool<beng::DirectionalLightComponent>();
            if (directionalPool != nullptr)
            {
                for (auto it = directionalPool->begin(); it != directionalPool->end(); ++it)
                {
                    const beng::DirectionalLightComponent& lightComp = *it;
                    const blib::math::Vector<float, 3>& dir = lightComp.getDirection();
                    const float dirY = (dir.y < -1.0f) ? -1.0f : ((dir.y > 1.0f) ? 1.0f : dir.y);
                    azimuthDeg = blib::math::atan2(dir.z, dir.x) * treeRadToDeg;
                    elevationDeg = -std::asin(dirY) * treeRadToDeg;
                    intensity = lightComp.getIntensity();
                    break;
                }
            }

            float ambientIntensity = defaultAmbientIntensity;
            beng::ComponentPool<beng::AmbientLightComponent>* ambientPool =
                impl->scene.tryGetComponentPool<beng::AmbientLightComponent>();
            if (ambientPool != nullptr)
            {
                for (auto it = ambientPool->begin(); it != ambientPool->end(); ++it)
                {
                    ambientIntensity = it->getIntensity();
                    break;
                }
            }

            ImGui::Text("light azimuth: %.0f deg", static_cast<double>(azimuthDeg));
            ImGui::Text("light elevation: %.0f deg", static_cast<double>(elevationDeg));
            ImGui::Text("light intensity: %.2f", static_cast<double>(intensity));
            ImGui::Text("ambient intensity: %.2f", static_cast<double>(ambientIntensity));
        }

        {
            // tryGetComponent: после scene_load ссылка может устареть
            beng::MeshRenderComponent* sphereMesh =
                impl->scene.tryGetComponent<beng::MeshRenderComponent>(impl->sphereEntity);
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
            impl->dancerEntity != beng::invalidEntity ? "loaded" : "not loaded");
        ImGui::Text("camera distance: %.0f", static_cast<double>(impl->camera.getDistance()));

        ImGui::End();
    }

    void ClientCore::setupWorld()
    {
        // -------------------------------------------------------------
        // Мир строится сущностями: вся отрисовка — только через
        // scene.update() (RenderSystem рисует по слоям, см. RenderLayer
        // в beng-client и BENG.md «beng-client»)
        // -------------------------------------------------------------

        // --- Земля: сетка тайлов (слой Ground) ---
        {
            // Процедурный меш строится ПРЯМО в слот кеша ресурсов
            // сцены (Mesh move-присваивание удалено — сборка на месте),
            // затем «опечатывается» (commit: hash + dedup-индекс)
            blib::resource::ResourceRef tilesRef =
                impl->scene.getResources().construct<blib::graphics::Mesh>(tilesResourceKey);
            if (__blib_unlikely(tilesRef.isEmpty()))
            {
                __blib_log_error("failed to construct tiles resource slot");
            }
            else
            {
                gravelands::IsometricTileset::buildMeshInto(*tilesRef.get<blib::graphics::Mesh>());
                tilesRef = impl->scene.getResources().commit(tilesRef);
            }

            const beng::EntityID entity = impl->scene.createEntity();
            impl->scene.resolveComponent<beng::TransformComponent>(entity, &impl->scene);
            impl->scene.addComponent<beng::MeshRenderComponent>(
                entity, tilesRef, beng::RenderLayer::Ground);
        }

        // --- Тестовая сфера (слой Opaque): toon + контур + шахматка ---
        {
            // Сфера генерируется примитивом blib; текстура заменяется
            // на шахматную (unlit показывает узор, toon — узор + свет)
            blib::graphics::Sphere sphere;
            sphere.createSpere(testSphereRadius, testSphereSegments,
                blib::graphics::Color(testSphereColorR, testSphereColorG, testSphereColorB, testSphereColorA));

            blib::graphics::Image sphereTexture;
            sphereTexture.create(testSphereTextureSize, testSphereTextureSize,
                blib::graphics::Color(testSphereColorR, testSphereColorG, testSphereColorB, testSphereColorA));
            for (buint16 y = 0; y < testSphereTextureSize; ++y)
            {
                for (buint16 x = 0; x < testSphereTextureSize; ++x)
                {
                    if ((x + y) % 2 != 0)
                    {
                        // ВАЖНО: Image::operator[] — [колонка][строка]
                        sphereTexture[x][y] =
                            blib::graphics::Color(testSphereDarkR, testSphereDarkG, testSphereDarkB, testSphereColorA);
                    }
                }
            }
            sphere.getMesh().material.diffuseImage = sphereTexture;

            impl->sphereEntity = impl->scene.createEntity();
            impl->scene.resolveComponent<beng::TransformComponent>(impl->sphereEntity, &impl->scene);
            beng::MeshRenderComponent& meshComp = impl->scene.addComponent<beng::MeshRenderComponent>(
                impl->sphereEntity, sphere.takeMesh(), beng::RenderLayer::Opaque);

            beng::TransformComponent& transform =
                impl->scene.getComponent<beng::TransformComponent>(impl->sphereEntity);
            transform.setLocalPosition(blib::math::Vector<float, 3>(0.0f, testSphereRadius, 0.0f));

            // NPR: мягкий toon + тонкий контур (inverted hull).
            // TODO: толщина контура должна масштабироваться от размера
            // объекта и дистанции камеры (актуально для моделей)
            blib::graphics::Material& material = meshComp.getMesh().material;
            material.shadingMode = blib::graphics::ShadingMode::Toon;
            material.outlineEnabled = true;
            material.outlineWidth = 0.6f;
        }

        // --- Деревья (слой AlphaTested): развёрнуты к камере ---
        {
            // ВАЖНО — конвенция поворота: TransformComponent вращает
            // КВАТЕРНИОНОМ (стандартная конвенция: поворот на угол θ
            // вокруг +Y отображает локальную +Z в (sinθ, 0, cosθ)).
            // yaw = atan2(dir.x, dir.z), dir — направление К камере.
            // НЕ путать с Euler-функциями rotateY/rotateZ (blib-graphics):
            // у них знак угла противоположный (+Z → (-sinθ, 0, cosθ)) —
            // при переносе ориентации с Euler на кватернионы менять знак
            // (иначе плоскость встаёт ребром к камере и отсекается
            // culling'ом) — см. CORE.md «Грабли math»
            blib::graphics::Vector3f cameraDirection = impl->camera.getPosition() - impl->camera.getTarget();
            cameraDirection.y = 0.0f;
            cameraDirection = blib::math::normalize(cameraDirection);

            const float treeYawDegrees =
                blib::math::atan2(cameraDirection.x, cameraDirection.z) * treeRadToDeg;
            const blib::math::Quaternion<float> treeRotation(
                blib::math::AngleDegreef(treeYawDegrees),
                blib::math::Vector<float, 3>(0.0f, 1.0f, 0.0f));

            blib::graphics::Image treeImage;
            generateTreeImage(treeImage);

            for (buint32 i = 0; i < testTreeCount; ++i)
            {
                blib::graphics::SpritePlane plane;
                plane.create(testTreeWidth, testTreeHeight, treeImage);

                impl->treeEntities[i] = impl->scene.createEntity();
                impl->scene.resolveComponent<beng::TransformComponent>(impl->treeEntities[i], &impl->scene);
                impl->scene.addComponent<beng::MeshRenderComponent>(
                    impl->treeEntities[i], plane.takeMesh(), beng::RenderLayer::AlphaTested);

                beng::TransformComponent& transform =
                    impl->scene.getComponent<beng::TransformComponent>(impl->treeEntities[i]);
                // Нога в земле: центр поднят на половину высоты
                transform.setLocalPosition(blib::math::Vector<float, 3>(
                    testTreePositions[i][0], testTreeHeight * 0.5f, testTreePositions[i][1]));
                transform.setLocalRotation(treeRotation);
            }
        }

        // --- Blob-тени (слой Shadow): под сферой и деревьями.
        // Тень танцора создаётся в loadDancerModel (нужна цель).
        // Блендинг и запрет записи глубины делает RenderSystem для
        // всего слоя Shadow (см. beng RenderSystem::drawLayer)
        {
            blib::graphics::Image shadowImage;
            generateShadowImage(shadowImage);

            {
                blib::graphics::BlobShadow shadow;
                shadow.create(sphereShadowRadius, shadowImage);

                impl->sphereShadowEntity = impl->scene.createEntity();
                impl->scene.resolveComponent<beng::TransformComponent>(impl->sphereShadowEntity, &impl->scene);
                impl->scene.addComponent<beng::MeshRenderComponent>(
                    impl->sphereShadowEntity, shadow.takeMesh(), beng::RenderLayer::Shadow);

                impl->scene.getComponent<beng::TransformComponent>(impl->sphereShadowEntity)
                    .setLocalPosition(blib::math::Vector<float, 3>(0.0f, shadowHeightOffset, 0.0f));
            }

            for (buint32 i = 0; i < testTreeCount; ++i)
            {
                blib::graphics::BlobShadow shadow;
                shadow.create(treeShadowRadius, shadowImage);

                impl->treeShadowEntities[i] = impl->scene.createEntity();
                impl->scene.resolveComponent<beng::TransformComponent>(impl->treeShadowEntities[i], &impl->scene);
                impl->scene.addComponent<beng::MeshRenderComponent>(
                    impl->treeShadowEntities[i], shadow.takeMesh(), beng::RenderLayer::Shadow);

                impl->scene.getComponent<beng::TransformComponent>(impl->treeShadowEntities[i])
                    .setLocalPosition(blib::math::Vector<float, 3>(
                        testTreePositions[i][0], shadowHeightOffset, testTreePositions[i][1]));
            }
        }

        // --- Свет: компоненты движка (beng-client). LightSystem
        // применяет их к RenderContext каждый кадр до отрисовки;
        // отладочные клавиши крутят компонент напрямую (см. updateLight)
        {
            // Направленный: азимут/элевация из констант → direction
            // (направление ИЗ источника К поверхности, см. updateLight)
            const float azimuthRad = blib::math::AngleDegreef(defaultLightAzimuthDeg).toRadian().data;
            const float elevationRad = blib::math::AngleDegreef(defaultLightElevationDeg).toRadian().data;
            const blib::math::Vector<float, 3> direction(
                blib::math::cos(elevationRad) * blib::math::cos(azimuthRad),
                -blib::math::sin(elevationRad),
                blib::math::cos(elevationRad) * blib::math::sin(azimuthRad));

            const beng::EntityID lightEntity = impl->scene.createEntity();
            beng::DirectionalLightComponent& lightComp =
                impl->scene.addComponent<beng::DirectionalLightComponent>(lightEntity);
            lightComp.setDirection(direction);
            lightComp.setIntensity(defaultLightIntensity);

            // Эмбиент: цвет и интенсивность — дефолты компонента
            // (совпадают с дефолтами RenderContext)
            const beng::EntityID ambientEntity = impl->scene.createEntity();
            impl->scene.addComponent<beng::AmbientLightComponent>(ambientEntity);
        }

        __blib_log_info("world scene built: %u entities",
            static_cast<unsigned int>(impl->scene.getEntityCount()));
    }

    void ClientCore::loadDancerModel()
    {
        // Тестовая скелетная модель (фаза 9): Mixamo-FBX с анимацией.
        // Путь резолвится из cwd/каталога exe/родителей — см. resolveContentPath
        const std::string modelPath = resolveContentPath(dancerModelPath);

        const beng::EntityID entity = impl->scene.createEntity();
        impl->scene.resolveComponent<beng::TransformComponent>(entity, &impl->scene);
        beng::SkinnedMeshComponent& meshComp = impl->scene.addComponent<beng::SkinnedMeshComponent>(entity);
        beng::AnimatorComponent& animComp = impl->scene.addComponent<beng::AnimatorComponent>(entity);

        if (__blib_unlikely(!meshComp.loadFromFile(modelPath, impl->scene.getResources())))
        {
            __blib_log_error("failed to load model '%s'", modelPath.c_str());
            impl->scene.destroyEntity(entity);
            return;
        }

        blib::graphics::SkinModel* model = meshComp.getModel();

        // NPR-материалы: мягкий toon + тонкий контур на всех мешах
        for (blib::graphics::SkinMesh& skinMesh : model->getMeshes())
        {
            blib::graphics::Material& material = skinMesh.mesh.material;
            material.shadingMode = blib::graphics::ShadingMode::Toon;
            material.outlineEnabled = true;
            material.outlineWidth = dancerOutlineWidth;
        }

        // Размещение: масштаб Mixamo-модели (~180 ед. роста) → ~18 ед.
        // сетки; поворот к камере кватернионом (см. dancerYawDegrees)
        beng::TransformComponent& transform = impl->scene.getComponent<beng::TransformComponent>(entity);
        transform.setLocalPosition(blib::math::Vector<float, 3>(dancerPositionX, 0.0f, dancerPositionZ));
        transform.setLocalScale(blib::math::Vector<float, 3>(dancerScale, dancerScale, dancerScale));
        transform.setLocalRotation(blib::math::Quaternion<float>(
            blib::math::AngleDegreef(dancerYawDegrees),
            blib::math::Vector<float, 3>(0.0f, 1.0f, 0.0f)));

        // Плейбек: первый клип, зациклен (Mixamo-файл — одна анимация)
        animComp.setAnimator(&model->getAnimator());
        animComp.setLoop(true);
        const std::vector<blib::graphics::AnimationClip>& animations = animComp.getAnimations();
        if (!animations.empty())
        {
            animComp.selectAnimation(animations[0].name);
            animComp.play();
        }

        impl->dancerEntity = entity;

        // Тень танцора (слой Shadow): следует за костью таза через
        // BlobShadowSystem — root-motion анимации двигает модель,
        // тень проецируется от источника света на землю
        {
            blib::graphics::Image shadowImage;
            generateShadowImage(shadowImage);

            blib::graphics::BlobShadow shadow;
            shadow.create(dancerShadowRadius, shadowImage);

            impl->dancerShadowEntity = impl->scene.createEntity();
            impl->scene.resolveComponent<beng::TransformComponent>(impl->dancerShadowEntity, &impl->scene);
            impl->scene.addComponent<beng::MeshRenderComponent>(
                impl->dancerShadowEntity, shadow.takeMesh(), beng::RenderLayer::Shadow);
            impl->scene.addComponent<beng::BlobShadowComponent>(
                impl->dancerShadowEntity, entity, dancerShadowBoneName, shadowHeightOffset);
        }

        __blib_log_info("dancer model loaded: %s", modelPath.c_str());
    }

    bool ClientCore::isRunning() const
    {
        return impl != nullptr && impl->window.isOpen();
    }

    void ClientCore::resetScene()
    {
        // Scene::load работает только в пустую сцену, а Scene
        // некопируема/неперемещаема — старую сцену разрушаем явно
        // (компоненты отпускают ref'ы кеша и модели), затем placement
        // new конструирует свежую на том же месте (разрушается она,
        // как и раньше, ДО окна/таргета — порядок членов impl не менялся)
        impl->scene.~Scene();
        new (&impl->scene) beng::Scene();

        // TransformComponent регистрируется сценой автоматически
        // (инвариант) — явная регистрация запрещена
        impl->scene.registerComponentType<beng::SkinnedMeshComponent>();
        impl->scene.registerComponentType<beng::AnimatorComponent>();
        impl->scene.registerComponentType<beng::MeshRenderComponent>();
        impl->scene.registerComponentType<beng::BlobShadowComponent>();
        impl->scene.registerComponentType<beng::DirectionalLightComponent>();
        impl->scene.registerComponentType<beng::AmbientLightComponent>();

        impl->scene.addSystem(&impl->transformSystem);
        impl->scene.addSystem(&impl->animationSystem);
        impl->scene.addSystem(&impl->blobShadowSystem);
        impl->scene.addSystem(&impl->lightSystem);
        impl->scene.addSystem(&impl->renderSystem);
        // Таргеты систем уже выставлены — рендер-таргет не менялся

        // Ссылки на сущности мира устаревают: ID в файле могут не
        // совпасть с предыдущими (отладочные клавиши/оверлей
        // используют tryGetComponent и безопасны на невалидном ID)
        impl->sphereEntity = beng::invalidEntity;
        impl->sphereShadowEntity = beng::invalidEntity;
        impl->dancerShadowEntity = beng::invalidEntity;
        impl->dancerEntity = beng::invalidEntity;
        for (buint32 i = 0; i < testTreeCount; ++i)
        {
            impl->treeEntities[i] = beng::invalidEntity;
            impl->treeShadowEntities[i] = beng::invalidEntity;
        }
    }

    void ClientCore::saveSceneCommand(_In const std::vector<std::string>& args)
    {
        // Путь — первый аргумент; без аргумента — дефолтное имя файла
        const std::string path = (args.size() > 0) ? args[0] : sceneDefaultFilePath;

        blib::core::FileStream fs;
        blib::core::FileStream::OpenModeFlags mode;
        mode.storage |= static_cast<buint8>(blib::core::OpenMode::Write);
        mode.storage |= static_cast<buint8>(blib::core::OpenMode::Binary);
        mode.storage |= static_cast<buint8>(blib::core::OpenMode::Truncate);
        if (fs.open(path.c_str(), mode) != blib::core::FileStatus::OK)
        {
            __blib_log_error("scene_save: cannot open '%s' for writing", path.c_str());
            return;
        }

        const blib::core::SaveStatus status = impl->scene.save(fs);
        if (status != blib::core::SaveStatus::None)
        {
            __blib_log_error("scene_save: failed (status %u)",
                static_cast<unsigned int>(status));
            return;
        }

        __blib_log_info("scene_save: %u entities written to '%s'",
            static_cast<unsigned int>(impl->scene.getEntityCount()), path.c_str());
    }

    void ClientCore::loadSceneCommand(_In const std::vector<std::string>& args)
    {
        const std::string path = (args.size() > 0) ? args[0] : sceneDefaultFilePath;

        blib::core::FileStream fs;
        blib::core::FileStream::OpenModeFlags mode;
        mode.storage |= static_cast<buint8>(blib::core::OpenMode::Read);
        mode.storage |= static_cast<buint8>(blib::core::OpenMode::Binary);
        if (fs.open(path.c_str(), mode) != blib::core::FileStatus::OK)
        {
            __blib_log_error("scene_load: cannot open '%s' for reading", path.c_str());
            return;
        }

        // Load — только в пустую сцену: пересоздаём мир целиком.
        // При неудаче (битый файл, неизвестный тип) откатываемся на
        // дефолтный процедурный мир — игра остаётся играбельной
        resetScene();

        const blib::core::LoadStatus status = impl->scene.load(fs);
        if (status != blib::core::LoadStatus::None)
        {
            __blib_log_error("scene_load: failed (status %u), rebuilding default world",
                static_cast<unsigned int>(status));
            setupWorld();
            loadDancerModel();
            return;
        }

        // Честная проверка round-trip: save -> свежая сцена -> load ->
        // strongCompare (см. Scene::verify). Плейбек аниматора тоже
        // сравнивается — состояние один в один
        if (!impl->scene.verify())
        {
            __blib_log_warning("scene_load: verify() failed — loaded scene differs from the saved state");
        }
        else
        {
            __blib_log_info("scene_load: verify() passed — scene matches the saved state");
        }

        __blib_log_info("scene_load: %u entities loaded from '%s'",
            static_cast<unsigned int>(impl->scene.getEntityCount()), path.c_str());
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

        // Явный вызов деструктора + возврат памяти глобальному аллокатору
        impl->~ClientCoreImpl();
        blib::memory::GlobalAllocator::instance().deallocate(impl, sizeof(ClientCoreImpl));
        impl = nullptr;

        __blib_log_info("%s client core shut down", gameTitle);
    }

} // namespace gravelands
