#include <gravelands/client/core/clientCore.h>
#include <gravelands/client/core/isometricTileset.h>

#include <beng/client/components/animatorComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/client/systems/animationSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/components/transform.h>
#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/core/math/quaternion.h>
#include <blib/core/math/trigonometry.h>
#include <blib/graphics/blobShadow.h>
#include <blib/graphics/color.h>
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

        // Скорости правки света клавишами
        constexpr float lightRotateSpeedDeg = 60.0f;
        constexpr float lightIntensitySpeed = 0.75f;
        constexpr float lightMinIntensity = 0.0f;
        constexpr float lightMaxIntensity = 3.0f;
        constexpr float lightMaxElevationDeg = 89.0f;
        constexpr float lightMinElevationDeg = 5.0f;

        // Имя консольной команды перезагрузки шейдеров (см. F5)
        constexpr const char* hotreloadCommand = "hotreload";

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
        // сетки). Поворот — к камере, конвенция rotateY (см. SpritePlane)
        constexpr const char* dancerModelPath = "resources\\Hip Hop Dancing.fbx";
        constexpr float dancerScale = 0.1f;
        constexpr float dancerPositionX = 0.0f;
        constexpr float dancerPositionZ = 60.0f;
        constexpr float dancerYawDegrees = -45.0f;
        constexpr float dancerOutlineWidth = 0.25f;
        constexpr float dancerShadowRadius = 8.0f;
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
        gravelands::IsometricTileset tileset;
        blib::graphics::Sphere testSphere;
        blib::graphics::BlobShadow sphereShadow;
        blib::graphics::BlobShadow treeShadows[testTreeCount];
        blib::graphics::SpritePlane testTrees[testTreeCount];

        // Рендер-ECS (фаза 9): сцена со скелетной моделью. Объявлены
        // ПОСЛЕ графических объектов — разрушаются РАНЬШЕ окна/таргета
        // (модель освобождает GL-ресурсы при живом контексте)
        beng::Scene scene;
        beng::TransformSystem transformSystem;
        beng::AnimationSystem animationSystem;
        beng::RenderSystem renderSystem;
        beng::EntityID dancerEntity = beng::invalidEntity;
        blib::graphics::BlobShadow dancerShadow;
        beng::Time time;

        // Состояние отладочного управления светом (фаза 4): азимут
        // и элевация источника (градусы) + интенсивность. Применяются
        // к renderTarget.rc.directionalLight каждый кадр
        float lightAzimuthDeg;
        float lightElevationDeg;
        float lightIntensity;

        ClientCoreImpl()
            : window(static_cast<uint16_t>(windowWidth), static_cast<uint16_t>(windowHeight), gameTitle)
            , renderTarget(windowWidth, windowHeight)
            , camera()
            , postProcess()
            , postEnabled(true)
            , tileset()
            , testSphere()
            , sphereShadow()
            , treeShadows()
            , testTrees()
            , scene()
            , transformSystem()
            , animationSystem()
            , renderSystem()
            , dancerEntity(beng::invalidEntity)
            , dancerShadow()
            , time()
            , lightAzimuthDeg(defaultLightAzimuthDeg)
            , lightElevationDeg(defaultLightElevationDeg)
            , lightIntensity(defaultLightIntensity)
        {
            // Сфера-«персонаж» в центре сетки, стоит на земле
            // (центр поднят на радиус над плоскостью y = 0)
            this->testSphere.createSpere(testSphereRadius, testSphereSegments,
                blib::graphics::Color(testSphereColorR, testSphereColorG, testSphereColorB, testSphereColorA));
            this->testSphere.setPosition(0.0f, testSphereRadius, 0.0f);

            // Шахматная текстура по UV: unlit покажет узор, toon —
            // узор + свет. Клетка (x, y) — светлая при чётной сумме
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
            this->testSphere.getMesh().material.diffuseImage = sphereTexture;

            // Toon-режим материала сферы (мягкий свет — фаза 4);
            // тайлы остаются unlit (чистое альбедо, как рисованные фоны)
            this->testSphere.getMesh().material.shadingMode = blib::graphics::ShadingMode::Toon;

            // Контур сферы (inverted hull, фаза 8): тонкая тёмная кайма.
            // Статика и плоскости — без контура (обводка в текстуре).
            // TODO: толщина должна масштабироваться от размера объекта
            // и дистанции камеры (актуально для реальных моделей, фаза 9)
            {
                blib::graphics::Material& sphereMaterial = this->testSphere.getMesh().material;
                sphereMaterial.outlineEnabled = true;
                sphereMaterial.outlineWidth = 0.6f;
            }

            // Плоскости-«деревья» (фаза 5): общая процедурная текстура,
            // поворот к камере ставится в initialize(), нога в земле
            // (центр поднят на половину высоты)
            blib::graphics::Image treeImage;
            generateTreeImage(treeImage);

            for (buint32 i = 0; i < testTreeCount; ++i)
            {
                this->testTrees[i].create(testTreeWidth, testTreeHeight, treeImage);
                this->testTrees[i].setPosition(
                    testTreePositions[i][0],
                    testTreeHeight * 0.5f,
                    testTreePositions[i][1]);
            }

            // Blob-тени (фаза 7): общий радиальный градиент, тени
            // прижаты к земле с небольшим подъёмом (z-fighting)
            blib::graphics::Image shadowImage;
            generateShadowImage(shadowImage);

            this->sphereShadow.create(sphereShadowRadius, shadowImage);
            this->sphereShadow.setPosition(0.0f, shadowHeightOffset, 0.0f);

            for (buint32 i = 0; i < testTreeCount; ++i)
            {
                this->treeShadows[i].create(treeShadowRadius, shadowImage);
                this->treeShadows[i].setPosition(
                    testTreePositions[i][0],
                    shadowHeightOffset,
                    testTreePositions[i][1]);
            }

            // Тень «танцора» (фаза 9) — под скелетной моделью
            this->dancerShadow.create(dancerShadowRadius, shadowImage);
            this->dancerShadow.setPosition(dancerPositionX, shadowHeightOffset, dancerPositionZ);
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

        // Разворот плоскостей-«деревьев» к камере. Конвенция rotateY
        // в движке: локальная нормаль +Z после поворота уходит в
        // (-sin(yaw), 0, cos(yaw)) — поэтому yaw = atan2(-dir.x, dir.z)
        // из направления К камере (позиция камеры → цель). BUG-FIX:
        // раньше бралось target - position (направление ОТ камеры) —
        // плоскости смотрели от камеры на 180°, что маскировалось
        // отсутствием culling (дерево симметричное); с culling задняя
        // грань отсекается и плоскости исчезали
        {
            blib::graphics::Vector3f cameraDirection = impl->camera.getPosition() - impl->camera.getTarget();
            cameraDirection.y = 0.0f;
            cameraDirection = blib::math::normalize(cameraDirection);

            const float treeYawDegrees =
                blib::math::atan2(-cameraDirection.x, cameraDirection.z) * treeRadToDeg;

            for (buint32 i = 0; i < testTreeCount; ++i)
            {
                impl->testTrees[i].setRotation(0.0f, treeYawDegrees, 0.0f);
            }
        }

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

        // Рендер-ECS (beng-client, фаза 9): сцена + системы анимации
        // и отрисовки скелетных моделей. Регистрация типов — строго
        // до запуска цикла (реестр не thread-safe, см. BENG.md)
        impl->scene.registerComponentType<beng::TransformComponent>();
        impl->scene.registerComponentType<beng::SkinnedMeshComponent>();
        impl->scene.registerComponentType<beng::AnimatorComponent>();

        impl->scene.addSystem(&impl->transformSystem);
        impl->scene.addSystem(&impl->animationSystem);
        impl->scene.addSystem(&impl->renderSystem);

        impl->renderSystem.setRenderTarget(&impl->renderTarget);

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

        // Прокачка оконных сообщений (закрытие по X, перерисовка) —
        // как в model_viewer, иначе окно не живёт
        impl->window.update();

        // Обновление состояния клавиатуры перед опросом (для камеры и выхода)
        blib::graphics::Keyboard::update();

        // Escape закрывает окно
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Escape))
        {
            impl->window.close();
            return;
        }

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
            blib::graphics::Material& sphereMaterial = impl->testSphere.getMesh().material;
            sphereMaterial.shadingMode =
                sphereMaterial.shadingMode == blib::graphics::ShadingMode::Toon
                ? blib::graphics::ShadingMode::Unlit
                : blib::graphics::ShadingMode::Toon;
        }

        // O — включение/выключение контура сферы (inverted hull, фаза 8)
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::O))
        {
            blib::graphics::Material& sphereMaterial = impl->testSphere.getMesh().material;
            sphereMaterial.outlineEnabled = !sphereMaterial.outlineEnabled;
        }

        updateCamera(deltaTime);
        updateLight(deltaTime);

        impl->renderTarget.clear(blib::graphics::Color::Black);
        impl->tileset.draw(impl->renderTarget);

        // Blob-тени: ПОСЛЕ земли, ДО плоскостей и персонажей.
        // Блендинг/глубина управляются внутри BlobShadow::draw
        impl->renderTarget.draw(impl->sphereShadow);
        impl->renderTarget.draw(impl->dancerShadow);
        for (buint32 i = 0; i < testTreeCount; ++i)
        {
            impl->renderTarget.draw(impl->treeShadows[i]);
        }

        // Рисованные плоскости (alpha-test, unlit): сортировка не нужна,
        // видимость решает depth-test
        for (buint32 i = 0; i < testTreeCount; ++i)
        {
            impl->renderTarget.draw(impl->testTrees[i]);
        }

        impl->renderTarget.draw(impl->testSphere);

        // Рендер-ECS: TransformSystem → AnimationSystem → RenderSystem
        // обновляют и рисуют скелетную модель (после ручных примитивов)
        impl->scene.update(deltaTime);

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
        // Стрелки вращают источник: Left/Right — азимут, Up/Down — элевация
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Left))
        {
            impl->lightAzimuthDeg -= lightRotateSpeedDeg * deltaTime;
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Right))
        {
            impl->lightAzimuthDeg += lightRotateSpeedDeg * deltaTime;
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Up))
        {
            impl->lightElevationDeg += lightRotateSpeedDeg * deltaTime;
            if (impl->lightElevationDeg > lightMaxElevationDeg)
            {
                impl->lightElevationDeg = lightMaxElevationDeg;
            }
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Down))
        {
            impl->lightElevationDeg -= lightRotateSpeedDeg * deltaTime;
            if (impl->lightElevationDeg < lightMinElevationDeg)
            {
                impl->lightElevationDeg = lightMinElevationDeg;
            }
        }

        // [ / ] — интенсивность света
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LBracket))
        {
            impl->lightIntensity -= lightIntensitySpeed * deltaTime;
            if (impl->lightIntensity < lightMinIntensity)
            {
                impl->lightIntensity = lightMinIntensity;
            }
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::RBracket))
        {
            impl->lightIntensity += lightIntensitySpeed * deltaTime;
            if (impl->lightIntensity > lightMaxIntensity)
            {
                impl->lightIntensity = lightMaxIntensity;
            }
        }

        // Применение: direction — направление ИЗ источника К поверхности
        // (вниз к земле), цвет источника — тёплый белый (как дефолт
        // RenderContext)
        const float azimuthRad = blib::math::AngleDegreef(impl->lightAzimuthDeg).toRadian().data;
        const float elevationRad = blib::math::AngleDegreef(impl->lightElevationDeg).toRadian().data;

        blib::graphics::DirectionalLight& light = impl->renderTarget.rc.directionalLight;
        light.direction = blib::graphics::Vector3f(
            blib::math::cos(elevationRad) * blib::math::cos(azimuthRad),
            -blib::math::sin(elevationRad),
            blib::math::cos(elevationRad) * blib::math::sin(azimuthRad));
        light.intensity = impl->lightIntensity;
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

        ImGui::TextUnformatted("WASD move | +/- zoom | Esc quit");
        ImGui::TextUnformatted("F5 hotreload | N normals | M toon/unlit");
        ImGui::TextUnformatted("Arrows: light dir | [ ]: light intensity");
        ImGui::TextUnformatted("P: post on/off | O: outline on/off");
        ImGui::Separator();
        ImGui::Text("light azimuth: %.0f deg", static_cast<double>(impl->lightAzimuthDeg));
        ImGui::Text("light elevation: %.0f deg", static_cast<double>(impl->lightElevationDeg));
        ImGui::Text("light intensity: %.2f", static_cast<double>(impl->lightIntensity));
        ImGui::Text("sphere shading: %s",
            impl->testSphere.getMesh().material.shadingMode == blib::graphics::ShadingMode::Toon
            ? "toon" : "unlit");
        ImGui::Text("normals view: %s", impl->renderTarget.rc.showNormals ? "on" : "off");
        ImGui::Text("post-process: %s", impl->postEnabled ? "on" : "off");
        ImGui::Text("sphere outline: %s",
            impl->testSphere.getMesh().material.outlineEnabled ? "on" : "off");
        ImGui::Text("dancer model: %s",
            impl->dancerEntity != beng::invalidEntity ? "loaded" : "not loaded");
        ImGui::Text("camera distance: %.0f", static_cast<double>(impl->camera.getDistance()));

        ImGui::End();
    }

    void ClientCore::loadDancerModel()
    {
        // Тестовая скелетная модель (фаза 9): Mixamo-FBX с анимацией.
        // Путь резолвится из cwd/каталога exe/родителей — см. resolveContentPath
        const std::string modelPath = resolveContentPath(dancerModelPath);

        const beng::EntityID entity = impl->scene.createEntity();
        impl->scene.addComponent<beng::TransformComponent>(entity, &impl->scene);
        beng::SkinnedMeshComponent& meshComp = impl->scene.addComponent<beng::SkinnedMeshComponent>(entity);
        beng::AnimatorComponent& animComp = impl->scene.addComponent<beng::AnimatorComponent>(entity);

        if (__blib_unlikely(!meshComp.loadFromFile(modelPath)))
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
        // сетки; поворот к камере (конвенция rotateY, см. SpritePlane)
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

        __blib_log_info("dancer model loaded: %s", modelPath.c_str());
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

        // Явный вызов деструктора + возврат памяти глобальному аллокатору
        impl->~ClientCoreImpl();
        blib::memory::GlobalAllocator::instance().deallocate(impl, sizeof(ClientCoreImpl));
        impl = nullptr;

        __blib_log_info("%s client core shut down", gameTitle);
    }

} // namespace gravelands
