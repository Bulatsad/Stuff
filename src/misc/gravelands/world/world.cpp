#include <gravelands/world/world.h>
#include <gravelands/world/isometricTileset.h>

#include <beng/client/components/ambientLightComponent.h>
#include <beng/client/components/animatorComponent.h>
#include <beng/client/components/blobShadowComponent.h>
#include <beng/client/components/cameraComponent.h>
#include <beng/client/components/directionalLightComponent.h>
#include <beng/client/components/meshRenderComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/client/systems/animationSystem.h>
#include <beng/client/systems/blobShadowSystem.h>
#include <beng/client/systems/cameraSystem.h>
#include <beng/client/systems/lightSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/scene.h>
#include <beng/systems/transformSystem.h>

#include <blib/core/console/console.h>
#include <blib/core/fileStream.h>
#include <blib/core/math/quaternion.h>
#include <blib/core/math/trigonometry.h>
#include <blib/core/resource/resourceManager.h>
#include <blib/graphics/blobShadow.h>
#include <blib/graphics/color.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/skinmodel.h>
#include <blib/graphics/sphere.h>
#include <blib/graphics/spritePlane.h>
#include <blib/system/memory/globalAllocator.h>

#include <cmath>
#include <fstream>
#include <new>

#include <Windows.h>

namespace gravelands
{
    namespace
    {
        // ---------------------------------------------------------------
        // Константы мира (правило проекта: без вшитых литералов)
        // ---------------------------------------------------------------

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

        // Консольные команды сохранения/загрузки сцены (тильда).
        // Путь — первый аргумент; без аргумента — дефолтное имя файла
        constexpr const char* sceneSaveCommandName = "scene_save";
        constexpr const char* sceneSaveCommandHelp =
            "saves the world scene to a JSON file (arg: path, default: gravelands_scene.json)";
        constexpr const char* sceneLoadCommandName = "scene_load";
        constexpr const char* sceneLoadCommandHelp =
            "loads the world scene from a JSON file (arg: path, default: gravelands_scene.json)";
        constexpr const char* sceneDefaultFilePath = "gravelands_scene.json";

        // Тестовые рисованные плоскости-«деревья» (фаза 5, NPR-гибрид):
        // unlit-квады с alpha-test вокруг сферы. Поворот к камере —
        // билборд: мир не знает камеру, плоскости разворачиваются на
        // стандартный ракурс изокамеры (азимут 45° — камера клиента
        // фиксирована и смотрит с +X+Z). TODO: хост будет передавать
        // направление своей камеры (эдитор — орбитальной)
        constexpr buint32 testTreeCount = 3;
        constexpr float testTreeWidth = 40.0f;
        constexpr float testTreeHeight = 70.0f;
        constexpr float treeBillboardYawDegrees = 45.0f;

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

        // Камера-сущность по умолчанию (см. cameraComponent.h):
        // параметры — дефолты компонента (FOV 60°, кадр 1280x720,
        // active); позиция/поворот — вид на центр мира с высоты —
        // именно её показывает Game-превью эдитора без Play и с неё
        // стартует клиент игры
        constexpr float defaultCameraPosX = 0.0f;
        constexpr float defaultCameraPosY = 70.0f;
        constexpr float defaultCameraPosZ = -140.0f;
        constexpr float defaultCameraTargetX = 0.0f;
        constexpr float defaultCameraTargetY = 10.0f;
        constexpr float defaultCameraTargetZ = 0.0f;
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

    // Внутренности мира: привязанная сцена хоста + системы + ссылки
    // на сущности. Полное определение скрыто в .cpp (pimpl) —
    // заголовок не тянет графические типы blib/beng в потребителей.
    struct World::WorldImpl
    {
        // Сцена ХОСТА: мир её не владеет (клиент — своя сцена,
        // эдитор — сцена каркаса EditorApplication). Привязывается
        // в initialize(scene); сцена обязана жить дольше мира
        beng::Scene* scene;

        // Системы МИРА (уникальные для игры): камера (инвариант «одна
        // активная»), тень и свет. Базовый рендер-пайплайн (Transform →
        // Animation → Render) вешает хост — см. world.h. Системы живут
        // здесь (Scene хранит сырые указатели) и разрушаются раньше сцены
        beng::CameraSystem cameraSystem;
        beng::BlobShadowSystem blobShadowSystem;
        beng::LightSystem lightSystem;

        // Колбэк сброса сцены (хост): вызывается после Scene::reset()
        // в scene_load — см. setSceneResetCallback
        std::function<void()> sceneResetCallback;

        // Сущности мира (для отладочных клавиш и статуса в оверлее)
        beng::EntityID sphereEntity = beng::invalidEntity;
        beng::EntityID sphereShadowEntity = beng::invalidEntity;
        beng::EntityID dancerShadowEntity = beng::invalidEntity;
        beng::EntityID treeEntities[testTreeCount] = {};
        beng::EntityID treeShadowEntities[testTreeCount] = {};
        beng::EntityID dancerEntity = beng::invalidEntity;

        WorldImpl()
            : scene(nullptr)
            , cameraSystem()
            , blobShadowSystem()
            , lightSystem()
        {
        }
    };

    World::World()
        : impl(nullptr)
    {
    }

    World::~World()
    {
        // Страховка: если владелец не вызвал shutdown явно
        shutdown();
    }

    bool World::initialize(_In beng::Scene& scene)
    {
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();

        // Аллокация через GlobalAllocator + placement new (проектное правило:
        // выделяющие new/delete запрещены, placement new разрешён)
        impl = static_cast<WorldImpl*>(globalAllocator.allocate(sizeof(WorldImpl)));
        new (impl) WorldImpl();

        // Привязка к сцене хоста: мир строит контент В СЦЕНУ ХОСТА
        // (эдитор правит сцену каркаса, клиент — свою) — см. world.h
        impl->scene = &scene;

        // Рендер-ECS (beng-client): типы компонентов регистрируются с
        // guard'ом — хост мог зарегистрировать их сам (каркас
        // EditorApplication регистрирует движковые типы заранее).
        // Регистрация — строго до запуска цикла (реестр не
        // thread-safe, см. BENG.md). TransformComponent регистрируется
        // сценой автоматически (инвариант: каждая сущность рождается
        // с Transform) — явная регистрация запрещена
        if (!impl->scene->isRegisteredComponentType<beng::SkinnedMeshComponent>())
        {
            impl->scene->registerComponentType<beng::SkinnedMeshComponent>();
        }
        if (!impl->scene->isRegisteredComponentType<beng::AnimatorComponent>())
        {
            impl->scene->registerComponentType<beng::AnimatorComponent>();
        }
        if (!impl->scene->isRegisteredComponentType<beng::MeshRenderComponent>())
        {
            impl->scene->registerComponentType<beng::MeshRenderComponent>();
        }
        if (!impl->scene->isRegisteredComponentType<beng::BlobShadowComponent>())
        {
            impl->scene->registerComponentType<beng::BlobShadowComponent>();
        }
        if (!impl->scene->isRegisteredComponentType<beng::DirectionalLightComponent>())
        {
            impl->scene->registerComponentType<beng::DirectionalLightComponent>();
        }
        if (!impl->scene->isRegisteredComponentType<beng::AmbientLightComponent>())
        {
            impl->scene->registerComponentType<beng::AmbientLightComponent>();
        }
        if (!impl->scene->isRegisteredComponentType<beng::CameraComponent>())
        {
            impl->scene->registerComponentType<beng::CameraComponent>();
        }

        // Системы мира: камера (приоритет -150: нормализация «одна
        // активная камера» до любой симуляции), тень (50) и свет (90).
        // Базовый рендер-пайплайн (Transform -100 → Animation -50 →
        // Render 100) вешает ХОСТ — мир его не дублирует
        impl->scene->addSystem(&impl->cameraSystem);
        impl->scene->addSystem(&impl->blobShadowSystem);
        impl->scene->addSystem(&impl->lightSystem);

        return true;
    }

    void World::setRenderTarget(_In_opt blib::graphics::IRenderTarget* target)
    {
        // Таргет — только LightSystem мира (применяет свет сцены к
        // RenderContext хоста). RenderSystem — система хоста, её
        // таргет выставляет сам хост
        impl->lightSystem.setRenderTarget(target);
    }

    void World::registerConsoleCommands()
    {
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
    }

    void World::setSceneResetCallback(_In_opt std::function<void()> callback)
    {
        impl->sceneResetCallback = std::move(callback);
    }

    void World::update(float deltaTime)
    {
        if (__blib_unlikely(impl == nullptr))
        {
            return;
        }

        // Единственная точка отрисовки мира: вся сцена (тайлы, тени,
        // плоскости, сфера, скелетная модель) рисуется RenderSystem'ом
        // внутри scene.update() по слоям (см. RenderLayer)
        impl->scene->update(deltaTime);
    }

    void World::updateLight(float deltaTime)
    {
        if (__blib_unlikely(impl == nullptr))
        {
            return;
        }

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
            impl->scene->tryGetComponentPool<beng::DirectionalLightComponent>();
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
            impl->scene->tryGetComponentPool<beng::AmbientLightComponent>();
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

    void World::setupWorld()
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
                impl->scene->getResources().construct<blib::graphics::Mesh>(tilesResourceKey);
            if (__blib_unlikely(tilesRef.isEmpty()))
            {
                __blib_log_error("failed to construct tiles resource slot");
            }
            else
            {
                gravelands::IsometricTileset::buildMeshInto(*tilesRef.get<blib::graphics::Mesh>());
                tilesRef = impl->scene->getResources().commit(tilesRef);
            }

            const beng::EntityID entity = impl->scene->createEntity();
            impl->scene->resolveComponent<beng::TransformComponent>(entity, impl->scene);
            impl->scene->addComponent<beng::MeshRenderComponent>(
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

            impl->sphereEntity = impl->scene->createEntity();
            impl->scene->resolveComponent<beng::TransformComponent>(impl->sphereEntity, impl->scene);
            beng::MeshRenderComponent& meshComp = impl->scene->addComponent<beng::MeshRenderComponent>(
                impl->sphereEntity, sphere.takeMesh(), beng::RenderLayer::Opaque);

            beng::TransformComponent& transform =
                impl->scene->getComponent<beng::TransformComponent>(impl->sphereEntity);
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
            //
            // Билборд-поворот — на стандартный ракурс изокамеры
            // (см. treeBillboardYawDegrees): камера клиента смотрит
            // с +X+Z под азимутом 45° — направление К камере
            // (sin45°, 0, cos45°) даёт ровно этот yaw
            const blib::math::Quaternion<float> treeRotation(
                blib::math::AngleDegreef(treeBillboardYawDegrees),
                blib::math::Vector<float, 3>(0.0f, 1.0f, 0.0f));

            blib::graphics::Image treeImage;
            generateTreeImage(treeImage);

            for (buint32 i = 0; i < testTreeCount; ++i)
            {
                blib::graphics::SpritePlane plane;
                plane.create(testTreeWidth, testTreeHeight, treeImage);

                impl->treeEntities[i] = impl->scene->createEntity();
                impl->scene->resolveComponent<beng::TransformComponent>(impl->treeEntities[i], impl->scene);
                impl->scene->addComponent<beng::MeshRenderComponent>(
                    impl->treeEntities[i], plane.takeMesh(), beng::RenderLayer::AlphaTested);

                beng::TransformComponent& transform =
                    impl->scene->getComponent<beng::TransformComponent>(impl->treeEntities[i]);
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

                impl->sphereShadowEntity = impl->scene->createEntity();
                impl->scene->resolveComponent<beng::TransformComponent>(impl->sphereShadowEntity, impl->scene);
                impl->scene->addComponent<beng::MeshRenderComponent>(
                    impl->sphereShadowEntity, shadow.takeMesh(), beng::RenderLayer::Shadow);

                impl->scene->getComponent<beng::TransformComponent>(impl->sphereShadowEntity)
                    .setLocalPosition(blib::math::Vector<float, 3>(0.0f, shadowHeightOffset, 0.0f));
            }

            for (buint32 i = 0; i < testTreeCount; ++i)
            {
                blib::graphics::BlobShadow shadow;
                shadow.create(treeShadowRadius, shadowImage);

                impl->treeShadowEntities[i] = impl->scene->createEntity();
                impl->scene->resolveComponent<beng::TransformComponent>(impl->treeShadowEntities[i], impl->scene);
                impl->scene->addComponent<beng::MeshRenderComponent>(
                    impl->treeShadowEntities[i], shadow.takeMesh(), beng::RenderLayer::Shadow);

                impl->scene->getComponent<beng::TransformComponent>(impl->treeShadowEntities[i])
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

            const beng::EntityID lightEntity = impl->scene->createEntity();
            beng::DirectionalLightComponent& lightComp =
                impl->scene->addComponent<beng::DirectionalLightComponent>(lightEntity);
            lightComp.setDirection(direction);
            lightComp.setIntensity(defaultLightIntensity);

            // Эмбиент: цвет и интенсивность — дефолты компонента
            // (совпадают с дефолтами RenderContext)
            const beng::EntityID ambientEntity = impl->scene->createEntity();
            impl->scene->addComponent<beng::AmbientLightComponent>(ambientEntity);
        }

        // --- Камера-сущность (beng.Camera): «взгляд» игры ---
        {
            // Позиция и поворот: смотрим на центр мира с высоты.
            // Поворот строится кватернионом из направления взгляда
            // (локальная +Z сущности → forward — конвенция камеры,
            // см. componentCameraAdapter.h)
            const blib::math::Vector<float, 3> cameraPos(
                defaultCameraPosX, defaultCameraPosY, defaultCameraPosZ);
            const blib::math::Vector<float, 3> cameraTarget(
                defaultCameraTargetX, defaultCameraTargetY, defaultCameraTargetZ);

            const blib::math::Vector<float, 3> forward =
                blib::math::normalize(cameraTarget - cameraPos);

            // Кватернион, переводящий +Z в forward: угол между +Z и
            // forward вокруг нормали к ним (см. CORE.md «Грабли math»)
            const blib::math::Vector<float, 3> zAxis(0.0f, 0.0f, 1.0f);
            const float forwardDot = blib::math::dot(forward, zAxis);
            const float clampedDot = (forwardDot > 1.0f) ? 1.0f : ((forwardDot < -1.0f) ? -1.0f : forwardDot);
            const float angleRad = std::acos(clampedDot);
            const blib::math::Vector<float, 3> axis = blib::math::cross(zAxis, forward);

            blib::math::Quaternion<float> cameraRotation(1.0f, 0.0f, 0.0f, 0.0f);
            if (blib::math::length(axis) > 0.0001f)
            {
                cameraRotation = blib::math::Quaternion<float>(
                    blib::math::AngleRadianf(angleRad),
                    blib::math::normalize(axis));
            }

            const beng::EntityID cameraEntity = impl->scene->createEntity();
            impl->scene->resolveComponent<beng::TransformComponent>(cameraEntity, impl->scene);
            impl->scene->addComponent<beng::CameraComponent>(cameraEntity);

            beng::TransformComponent& cameraTransform =
                impl->scene->getComponent<beng::TransformComponent>(cameraEntity);
            cameraTransform.setLocalPosition(cameraPos);
            cameraTransform.setLocalRotation(cameraRotation);
        }

        __blib_log_info("world scene built: %u entities",
            static_cast<unsigned int>(impl->scene->getEntityCount()));
    }

    void World::loadDancerModel()
    {
        // Тестовая скелетная модель (фаза 9): Mixamo-FBX с анимацией.
        // Путь резолвится из cwd/каталога exe/родителей — см. resolveContentPath
        const std::string modelPath = resolveContentPath(dancerModelPath);

        const beng::EntityID entity = impl->scene->createEntity();
        impl->scene->resolveComponent<beng::TransformComponent>(entity, impl->scene);
        beng::SkinnedMeshComponent& meshComp = impl->scene->addComponent<beng::SkinnedMeshComponent>(entity);
        beng::AnimatorComponent& animComp = impl->scene->addComponent<beng::AnimatorComponent>(entity);

        if (__blib_unlikely(!meshComp.loadFromFile(modelPath, impl->scene->getResources())))
        {
            __blib_log_error("failed to load model '%s'", modelPath.c_str());
            impl->scene->destroyEntity(entity);
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
        beng::TransformComponent& transform = impl->scene->getComponent<beng::TransformComponent>(entity);
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

            impl->dancerShadowEntity = impl->scene->createEntity();
            impl->scene->resolveComponent<beng::TransformComponent>(impl->dancerShadowEntity, impl->scene);
            impl->scene->addComponent<beng::MeshRenderComponent>(
                impl->dancerShadowEntity, shadow.takeMesh(), beng::RenderLayer::Shadow);
            impl->scene->addComponent<beng::BlobShadowComponent>(
                impl->dancerShadowEntity, entity, dancerShadowBoneName, shadowHeightOffset);
        }

        __blib_log_info("dancer model loaded: %s", modelPath.c_str());
    }

    beng::Scene& World::getScene()
    {
        // Мир обязан быть привязан (initialize(scene)) до обращения
        if (__blib_unlikely(impl == nullptr || impl->scene == nullptr))
        {
            __blib_fatal("World::getScene: world is not bound to a host scene (call initialize(scene) first)");
        }
        return *impl->scene;
    }

    beng::EntityID World::getSphereEntity() const
    {
        return impl->sphereEntity;
    }

    void World::removeLocalPlayerEntity()
    {
        // Локальный «персонаж»-сфера — заглушка офлайн-режима: в сетевой
        // игре зеркала юнитов приходят из снапшотов (см. GRAVELANDS.md)
        if (impl->sphereEntity != beng::invalidEntity)
        {
            impl->scene->destroyEntity(impl->sphereEntity);
            impl->sphereEntity = beng::invalidEntity;
        }
        if (impl->sphereShadowEntity != beng::invalidEntity)
        {
            impl->scene->destroyEntity(impl->sphereShadowEntity);
            impl->sphereShadowEntity = beng::invalidEntity;
        }
    }

    beng::EntityID World::getDancerEntity() const
    {
        return impl->dancerEntity;
    }

    void World::resetScene()
    {
        // Сброс сцены хоста НА МЕСТЕ (Scene::reset): реестр типов и
        // системы сохраняются, сущности/пулы/кеш ресурсов сносятся —
        // load() примет сцену как пустую, привязки хоста не рвутся.
        // Ссылки на сущности мира устаревают: ID в файле могут не
        // совпасть с предыдущими (отладочные клавиши/оверлей
        // используют tryGetComponent и безопасны на невалидном ID)
        impl->scene->reset();

        // Хост уведомлён: сцена сброшена — очистить привязанное к
        // старым данным состояние (undo/redo-история эдитора и т.п.)
        if (impl->sceneResetCallback)
        {
            impl->sceneResetCallback();
        }

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

    void World::saveSceneCommand(_In const std::vector<std::string>& args)
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

        const blib::core::SaveStatus status = impl->scene->save(fs);
        if (status != blib::core::SaveStatus::None)
        {
            __blib_log_error("scene_save: failed (status %u)",
                static_cast<unsigned int>(status));
            return;
        }

        __blib_log_info("scene_save: %u entities written to '%s'",
            static_cast<unsigned int>(impl->scene->getEntityCount()), path.c_str());
    }

    void World::loadSceneCommand(_In const std::vector<std::string>& args)
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

        const blib::core::LoadStatus status = impl->scene->load(fs);
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
        if (!impl->scene->verify())
        {
            __blib_log_warning("scene_load: verify() failed — loaded scene differs from the saved state");
        }
        else
        {
            __blib_log_info("scene_load: verify() passed — scene matches the saved state");
        }

        __blib_log_info("scene_load: %u entities loaded from '%s'",
            static_cast<unsigned int>(impl->scene->getEntityCount()), path.c_str());
    }

    void World::shutdown()
    {
        if (impl == nullptr)
        {
            return;
        }

        // Явный вызов деструктора + возврат памяти глобальному аллокатору
        impl->~WorldImpl();
        blib::memory::GlobalAllocator::instance().deallocate(impl, sizeof(WorldImpl));
        impl = nullptr;
    }

} // namespace gravelands
