#include <gravelands/plugin/gravelandsEditorHost.h>
#include <gravelands/common/config.h>
#include <gravelands/plugin/gamePanel.h>
#include <gravelands/plugin/pieSession.h>
#include <gravelands/world/world.h>

#include <beng/components/transform.h>
#include <beng/client/componentCameraAdapter.h>
#include <beng/client/components/cameraComponent.h>
#include <beng/client/components/meshRenderComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/client/systems/lightSystem.h>
#include <beng/client/systems/renderSystem.h>
#include <beng/core/scene.h>
#include <beng/editor/editorIcons.h>

#include <blib/core/console/console.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/mesh.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/shader.h>
#include <blib/graphics/skinmodel.h>
#include <blib/system/memory/globalAllocator.h>

#include <imgui/imgui.h>

#include <cmath>
#include <new>

namespace gravelands
{
    namespace
    {
        // Имя консольной команды перезагрузки шейдеров (см. F5)
        constexpr const char* hotreloadCommand = "hotreload";

        // Заголовок верхней полосы эдитора (полоса резервируется
        // каркасом — позицию/размер выставляет EditorApplication)
        constexpr const char* topBarTitle = "Gravelands";

        // Флаги верхней полосы: без шапки и скроллбара (полоса целиком —
        // контент; шапка-заголовок и скроллбар съедали её высоту), без
        // изменения размера
        constexpr ImGuiWindowFlags topBarFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar;

        // Кнопки PIE (иконки-квадраты: подписи — тултипы кнопок)
        constexpr const char* playButtonLabel = "Play";
        constexpr const char* stopButtonLabel = "Stop";
        constexpr const char* pauseButtonLabel = "Pause";
        constexpr const char* resumeButtonLabel = "Resume";
        constexpr const char* pieRunningLabel = "PIE: running";
        constexpr const char* piePausedLabel = "PIE: paused";
        constexpr const char* pieStoppedLabel = "PIE: stopped";
        constexpr float pieButtonIconSize = 16.0f;

        // Id вкладки «Scene» центральной области (вкладка каркаса;
        // вкладки хоста — id от registerCenterTab, 1..)
        constexpr buint32 sceneCenterTabId = 0;

        // «Вкладка не зарегистрирована» (registerCenterTab не звали)
        constexpr buint32 invalidCenterTabId = buint32Max;

        // Ray-picking (выбор кликом во вьюпорте): максимальная
        // дистанция луча (дальше — «мимо»), радиус сферы-фолбэка для
        // сущностей без мешей, эпсилон вырожденных сравнений
        constexpr float pickMaxDistance = 100000.0f;
        constexpr float pickFallbackRadius = 8.0f;
        constexpr float pickEpsilon = 0.000001f;

        // Интервал логирования среднего кадрового времени PIE (сек)
        constexpr float pieLogIntervalSeconds = 1.0f;

        // Аспект-заглушка превью Game (панель использует аспект только
        // при живой камере — см. GamePanel::setPreviewTexture)
        constexpr float previewFallbackAspect = 1.0f;
    }

    namespace
    {
        // Осевой бокс пикинга (локальные или мировые координаты)
        struct PickAabb
        {
            blib::math::Vector<float, 3> min;
            blib::math::Vector<float, 3> max;
        };

        // Минимум/максимум двух скаляров (без STL-зависимостей)
        float pickMin(float a, float b) { return (a < b) ? a : b; }
        float pickMax(float a, float b) { return (a > b) ? a : b; }

        // Скалярное произведение векторов
        float pickDot(_In const blib::math::Vector<float, 3>& a, _In const blib::math::Vector<float, 3>& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        // AABB меша по вершинам (локальные координаты); false — меш пуст
        bool computeMeshAabb(_In const blib::graphics::Mesh& mesh, _Out PickAabb& outAabb)
        {
            if (mesh.vertices.empty())
            {
                return false;
            }

            outAabb.min = mesh.vertices[0];
            outAabb.max = mesh.vertices[0];
            for (const blib::math::Vector<float, 3>& vertex : mesh.vertices)
            {
                outAabb.min.x = pickMin(outAabb.min.x, vertex.x);
                outAabb.min.y = pickMin(outAabb.min.y, vertex.y);
                outAabb.min.z = pickMin(outAabb.min.z, vertex.z);
                outAabb.max.x = pickMax(outAabb.max.x, vertex.x);
                outAabb.max.y = pickMax(outAabb.max.y, vertex.y);
                outAabb.max.z = pickMax(outAabb.max.z, vertex.z);
            }
            return true;
        }

        // Перевод локального бокса в мировые координаты: масштаб +
        // трансляция (вращение игнорируется — приёмлемо для пикинга)
        void applyScaleTranslate(
            _In const PickAabb& local, _In const blib::math::Vector<float, 3>& position,
            _In const blib::math::Vector<float, 3>& scale, _Out PickAabb& outAabb)
        {
            outAabb.min = blib::math::Vector<float, 3>(
                position.x + local.min.x * scale.x,
                position.y + local.min.y * scale.y,
                position.z + local.min.z * scale.z);
            outAabb.max = blib::math::Vector<float, 3>(
                position.x + local.max.x * scale.x,
                position.y + local.max.y * scale.y,
                position.z + local.max.z * scale.z);
        }

        // Объединение бокса в аккумулятор (для AABB модели из мешей)
        void mergeAabb(_In const PickAabb& source, _In PickAabb& accumulator)
        {
            accumulator.min.x = pickMin(accumulator.min.x, source.min.x);
            accumulator.min.y = pickMin(accumulator.min.y, source.min.y);
            accumulator.min.z = pickMin(accumulator.min.z, source.min.z);
            accumulator.max.x = pickMax(accumulator.max.x, source.max.x);
            accumulator.max.y = pickMax(accumulator.max.y, source.max.y);
            accumulator.max.z = pickMax(accumulator.max.z, source.max.z);
        }

        // Ray-AABB slab-тест: true + t входа (t >= 0) при пересечении
        bool rayAabbHit(
            _In const blib::math::Vector<float, 3>& origin, _In const blib::math::Vector<float, 3>& direction,
            _In const blib::math::Vector<float, 3>& boxMin, _In const blib::math::Vector<float, 3>& boxMax,
            _Out float& outT)
        {
            float tmin = 0.0f;
            float tmax = pickMaxDistance;

            // Три оси — вручную (Vector<float,3> не итерируется)
            for (buint32 axis = 0; axis < 3; ++axis)
            {
                const float o = (axis == 0) ? origin.x : ((axis == 1) ? origin.y : origin.z);
                const float d = (axis == 0) ? direction.x : ((axis == 1) ? direction.y : direction.z);
                const float lo = (axis == 0) ? boxMin.x : ((axis == 1) ? boxMin.y : boxMin.z);
                const float hi = (axis == 0) ? boxMax.x : ((axis == 1) ? boxMax.y : boxMax.z);

                if (d > -pickEpsilon && d < pickEpsilon)
                {
                    // Луч параллелен оси: должен лежать внутри плиты
                    if (o < lo || o > hi)
                    {
                        return false;
                    }
                    continue;
                }

                float t1 = (lo - o) / d;
                float t2 = (hi - o) / d;
                if (t1 > t2)
                {
                    const float swap = t1;
                    t1 = t2;
                    t2 = swap;
                }
                tmin = pickMax(tmin, t1);
                tmax = pickMin(tmax, t2);
                if (tmin > tmax)
                {
                    return false;
                }
            }

            outT = tmin;
            return true;
        }

        // Ray-sphere: true + t входа (t >= 0); отрицательный вход
        // (камера внутри сферы) засчитывается как t = 0
        bool raySphereHit(
            _In const blib::math::Vector<float, 3>& origin, _In const blib::math::Vector<float, 3>& direction,
            _In const blib::math::Vector<float, 3>& center, float radius, _Out float& outT)
        {
            const blib::math::Vector<float, 3> oc = origin - center;
            const float b = pickDot(oc, direction);
            const float c = pickDot(oc, oc) - radius * radius;
            const float discriminant = b * b - c;
            if (discriminant < 0.0f)
            {
                return false;
            }

            const float root = std::sqrt(discriminant);
            float t = -b - root;
            if (t < 0.0f)
            {
                t = -b + root;
            }
            if (t < 0.0f)
            {
                return false;
            }

            outT = t;
            return true;
        }
    }

    // Внутренности хоста: мир Gravelands + PIE-сессия + Game-превью.
    // Каркас (окно, FBO, камера, ImGui, сцена эдитора, сценные панели
    // Hierarchy/Inspector, gizmo) живёт в базовом EditorApplication.
    // Полное определение скрыто в .cpp (pimpl) — заголовок не тянет
    // графические типы в потребителей.
    struct GravelandsEditorHost::GravelandsEditorHostImpl
    {
        // Мир: привязывается к сцене каркаса (та же сцена, что правит
        // клиент — см. GRAVELANDS.md). Разрушается в shutdown() хоста
        // ДО каркасного shutdown — меши освобождают GL-ресурсы при
        // живом контексте
        gravelands::World world;

        // Play In Editor: in-process хостинг сервера + клиента
        // (loopback TCP); клиент — headless (без окна), кадр игры
        // показывается вкладкой «Game» центральной области
        gravelands::PieSession pieSession;

        // Вкладка «Game» (ICenterTabView) — кадр PIE-клиента или
        // превью из активной камеры в таб-баре центральной области
        // (Scene | Game, как в Unity). Id вкладки — из registerCenterTab
        // (onInitialize)
        gravelands::GamePanel gamePanel;
        buint32 gameTabId;

        // ---- Game-превью (вкладка «Game» без Play) ----
        // FBO под разрешение активной камеры: создаётся в onInitialize
        // (GL-контекст каркаса уже жив), размер выставляет первый
        // проход превью (resize от pixelWidth/Height компонента).
        // Память — GlobalAllocator + placement new (IRenderTarget не
        // имеет дефолтного конструктора, а impl создаётся ДО GL)
        blib::graphics::IRenderTarget* gameTarget;
        buint32 gameTargetWidth;
        buint32 gameTargetHeight;

        // Камера-адаптер (ICamera из beng.Camera + Transform) и
        // собственные рендер/свет системы превью: рисуют сцену
        // эдитора в gameTarget. Собственные инстансы (не из сцены) —
        // проход превью не дублирует системы каркаса в scene.update()
        beng::ComponentCameraAdapter gameCamera;
        beng::RenderSystem gameRenderSystem;
        beng::LightSystem gameLightSystem;

        // Play отложен до начала следующего кадра: клиентское окно
        // (второй GL-контекст) нельзя создавать в середине ImGui-кадра —
        // иначе остаток кадра эдитора рисует в чужом контексте
        bool pieStartPending;

        // Замер кадрового времени PIE: суммарное dt и число кадров
        // за интервал логирования (см. onSceneWillUpdate) — диагностика
        // «тяжёлого кадра» (сеть в PIE кадрово-зависима)
        float pieLogAccumulator;
        buint32 pieLogFrameCount;

        GravelandsEditorHostImpl()
            : world()
            , pieSession()
            , gamePanel(&this->pieSession)
            , gameTabId(invalidCenterTabId)
            , gameTarget(nullptr)
            , gameTargetWidth(0)
            , gameTargetHeight(0)
            , gameCamera()
            , gameRenderSystem()
            , gameLightSystem()
            , pieStartPending(false)
            , pieLogAccumulator(0.0f)
            , pieLogFrameCount(0)
        {
        }
    };

    void GravelandsEditorHost::onInitialize(_In beng::Scene& scene)
    {
        // Мир Gravelands — В КАРКАСНУЮ СЦЕНУ эдитора: эдитор правит
        // сцену игры (Hierarchy/Inspector/сериализация), кадр каркаса
        // сам вызывает scene.update() — RenderSystem каркаса рисует мир.
        // Базовый рендер-пайплайн каркас уже повесил (Transform →
        // Animation → Render); мир добавляет только свои системы
        // (тень/свет) и контент
        this->impl->world.initialize(scene);
        this->impl->world.setRenderTarget(&this->getRenderTarget());
        this->impl->world.registerConsoleCommands();

        // Сцена перезагружается (scene_load → Scene::reset): история
        // команд каркаса ссылается на старые данные — очистить
        this->impl->world.setSceneResetCallback([this]() {
            this->getCommandHistory().clear();
        });

        // Консольные команды графики: "hotreload"/"reload_shaders" —
        // перекомпиляция шейдеров с диска без перезапуска эдитора
        blib::graphics::registerGraphicsConsoleCommands();

        // Контент мира: тайлы, сфера, деревья, тени, свет + танцор
        this->impl->world.setupWorld();
        this->impl->world.loadDancerModel();

        // Вкладка «Game» центральной области (кадр PIE-клиента или
        // превью из активной камеры): id нужен Play/Stop для
        // автопереключения вкладок Game/Scene и превью — для гейта
        // «вкладка видима»
        this->impl->gameTabId = this->registerCenterTab(&this->impl->gamePanel);

        // Game-превью: FBO под разрешение активной камеры (GL-контекст
        // каркаса уже жив — ImGui инициализирован выше). Размер 1x1 —
        // временный: первый проход превью отресайзит под камеру
        {
            auto& globalAllocator = blib::memory::GlobalAllocator::instance();
            this->impl->gameTarget = static_cast<blib::graphics::IRenderTarget*>(
                globalAllocator.allocate(sizeof(blib::graphics::IRenderTarget)));
            new (this->impl->gameTarget) blib::graphics::IRenderTarget(1, 1);

            // Собственные системы прохода превью (см. impl): свет —
            // в rc превью-таргета, отрисовка — в превью-FBO
            this->impl->gameRenderSystem.setRenderTarget(this->impl->gameTarget);
            this->impl->gameLightSystem.setRenderTarget(this->impl->gameTarget);
        }

        // Сценные панели (Scene Hierarchy + Inspector через рефлексию)
        // регистрирует САМ каркас — хост их не создаёт (см.
        // EditorApplication::initialize). Selection — в каркасе:
        // gizmo читает getSelectedEntity()
    }

    void GravelandsEditorHost::onInput()
    {
        // F5 — hot-reload шейдеров (как в клиенте игры)
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::F5))
        {
            blib::console::Console::instance().execute(hotreloadCommand);
        }
    }

    void GravelandsEditorHost::onSceneWillUpdate(float deltaTime)
    {
        // Отладочное управление светом (стрелки/[ ]/PageUp/PageDown):
        // мир крутит компоненты света своей сцены до отрисовки
        this->impl->world.updateLight(deltaTime);

        // Отложенный Play: клиентское окно (второй GL-контекст)
        // создаётся в НАЧАЛЕ кадра, а не в середине ImGui-кадра
        if (this->impl->pieStartPending)
        {
            this->impl->pieStartPending = false;
            this->startPie();
        }

        // PIE: кадр игровой сессии (сервер + клиент) внутри кадра
        // эдитора. Клиент headless: рендерит в свой FBO в том же
        // GL-контексте — кадр сразу готов для Game-вкладки
        if (this->impl->pieSession.isRunning())
        {
            // Замер кадрового времени PIE: сеть кадрово-зависима
            // (команды/снапшоты движутся с частотой кадров эдитора) —
            // среднее за секунду видно в debug-логе
            this->impl->pieLogAccumulator += deltaTime;
            ++this->impl->pieLogFrameCount;
            if (this->impl->pieLogAccumulator >= pieLogIntervalSeconds)
            {
                __blib_log_debug("PIE frame: %.1f ms average over %u frames",
                    static_cast<double>(
                        this->impl->pieLogAccumulator * 1000.0f
                        / static_cast<float>(this->impl->pieLogFrameCount)),
                    static_cast<unsigned int>(this->impl->pieLogFrameCount));
                this->impl->pieLogAccumulator = 0.0f;
                this->impl->pieLogFrameCount = 0;
            }

            this->impl->pieSession.tick();
        }
    }

    void GravelandsEditorHost::onSceneDidUpdate(float deltaTime)
    {
        // В PIE кадр Game-вкладки даёт клиент — превью не рисуем
        if (this->impl->pieSession.isRunning())
        {
            this->impl->gamePanel.setPreviewTexture(0, false, previewFallbackAspect);
            return;
        }

        // Вкладка Game не видна — проход не нужен (текстура прошлого
        // прохода остаётся в FBO и покажется при возврате на вкладку)
        if (this->impl->gameTabId == invalidCenterTabId ||
            this->getActiveCenterTab() != this->impl->gameTabId)
        {
            return;
        }

        beng::Scene& scene = this->getScene();

        // Активная камера сцены: «последняя включённая» (максимальный
        // штамп — консистентно с CameraSystem; итератор пула пропускает
        // неактивные)
        beng::CameraComponent* activeCamera = nullptr;
        beng::EntityID cameraEntity = beng::invalidEntity;
        beng::ComponentPool<beng::CameraComponent>* cameraPool =
            scene.tryGetComponentPool<beng::CameraComponent>();
        if (cameraPool != nullptr)
        {
            for (auto it = cameraPool->begin(); it != cameraPool->end(); ++it)
            {
                if (activeCamera == nullptr ||
                    it->getActivationStamp() > activeCamera->getActivationStamp())
                {
                    activeCamera = &*it;
                    cameraEntity = it.getEntityId();
                }
            }
        }

        if (__blib_unlikely(activeCamera == nullptr))
        {
            // Камеры нет — панель покажет заглушку «No active camera»
            this->impl->gamePanel.setPreviewTexture(0, false, previewFallbackAspect);
            return;
        }

        beng::TransformComponent* cameraTransform =
            scene.tryGetComponent<beng::TransformComponent>(cameraEntity);
        if (__blib_unlikely(cameraTransform == nullptr))
        {
            this->impl->gamePanel.setPreviewTexture(0, false, previewFallbackAspect);
            return;
        }

        // Разрешение кадра — из камеры (downscale до окна вкладки —
        // GPU-фильтрация ImGui::Image): FBO ресайзится при изменении
        buint32 cameraWidth = activeCamera->getPixelWidth();
        buint32 cameraHeight = activeCamera->getPixelHeight();
        if (__blib_unlikely(cameraWidth < 1 || cameraHeight < 1))
        {
            this->impl->gamePanel.setPreviewTexture(0, false, previewFallbackAspect);
            return;
        }
        if (cameraWidth != this->impl->gameTargetWidth ||
            cameraHeight != this->impl->gameTargetHeight)
        {
            this->impl->gameTarget->resize(cameraWidth, cameraHeight);
            this->impl->gameTargetWidth = cameraWidth;
            this->impl->gameTargetHeight = cameraHeight;
        }

        const float aspect =
            static_cast<float>(cameraWidth) / static_cast<float>(cameraHeight);

        // Сцена из «глаз» камеры-сущности в превью-FBO. Проход идёт в
        // onSceneDidUpdate — ПОСЛЕ scene.update() каркаса: анимация и
        // тени этого кадра уже применены, повторно продвигать симуляцию
        // не нужно (рисуем только свет + меши)
        this->impl->gameCamera.sync(*activeCamera, *cameraTransform, aspect);
        this->impl->gameTarget->clear(blib::graphics::Color::Black);
        this->impl->gameTarget->rc.setCamera(&this->impl->gameCamera);
        this->impl->gameLightSystem.update(scene, deltaTime);
        this->impl->gameRenderSystem.update(scene, deltaTime);

        // Возврат FBO вьюпорта каркасу: следующий шаг кадра (drawGizmo)
        // рисует в ТЕКУЩИЙ GL-бинд — см. IRenderTarget::bind
        this->getRenderTarget().bind();

        // Кадр превью готов — панель сэмплит его текстуру
        this->impl->gamePanel.setPreviewTexture(
            static_cast<buint64>(
                this->impl->gameTarget->getColorTexture().getContext().textureID),
            true,
            aspect);
    }

    void GravelandsEditorHost::onViewportClick(
        _In const blib::math::Vector<float, 3>& rayOrigin,
        _In const blib::math::Vector<float, 3>& rayDirection)
    {
        // Выбор кликом: ближайшее пересечение луча с сущностями мира.
        // Плагин знает свои рендер-типы (каркас — игра-агностик):
        // MeshRender/SkinnedMesh — ray-AABB по мешам, сущности без
        // мешей (свет, тени) — сфера-фолбэк вокруг позиции
        beng::Scene& scene = this->getScene();

        beng::EntityID bestEntity = beng::invalidEntity;
        float bestT = pickMaxDistance;

        const buint32 entityCount = scene.getEntityCount();
        for (buint32 i = 0; i < entityCount; ++i)
        {
            const beng::EntityID id = scene.getEntityId(i);
            if (__blib_unlikely(id == beng::invalidEntity))
            {
                continue;
            }

            beng::TransformComponent* transform = scene.tryGetComponent<beng::TransformComponent>(id);
            if (__blib_unlikely(transform == nullptr))
            {
                continue;
            }

            const blib::math::Vector<float, 3> position = transform->getWorldPosition();
            const blib::math::Vector<float, 3> scale = transform->getWorldScale();

            float hitT = 0.0f;
            bool hit = false;

            // Статические меши: точный AABB
            beng::MeshRenderComponent* meshRender = scene.tryGetComponent<beng::MeshRenderComponent>(id);
            if (meshRender != nullptr)
            {
                PickAabb localAabb;
                if (computeMeshAabb(meshRender->getMesh(), localAabb))
                {
                    PickAabb worldAabb;
                    applyScaleTranslate(localAabb, position, scale, worldAabb);
                    hit = rayAabbHit(rayOrigin, rayDirection, worldAabb.min, worldAabb.max, hitT);
                }
            }

            // Скелетная модель: AABB по мешам модели (bind-поза)
            if (!hit)
            {
                beng::SkinnedMeshComponent* skinned = scene.tryGetComponent<beng::SkinnedMeshComponent>(id);
                if (skinned != nullptr && skinned->getModel() != nullptr)
                {
                    bool hasAabb = false;
                    PickAabb combinedAabb;
                    for (blib::graphics::SkinMesh& skinMesh : skinned->getModel()->getMeshes())
                    {
                        PickAabb localAabb;
                        if (!computeMeshAabb(skinMesh.mesh, localAabb))
                        {
                            continue;
                        }
                        if (!hasAabb)
                        {
                            combinedAabb = localAabb;
                            hasAabb = true;
                        }
                        else
                        {
                            mergeAabb(localAabb, combinedAabb);
                        }
                    }

                    if (hasAabb)
                    {
                        PickAabb worldAabb;
                        applyScaleTranslate(combinedAabb, position, scale, worldAabb);
                        hit = rayAabbHit(rayOrigin, rayDirection, worldAabb.min, worldAabb.max, hitT);
                    }
                }
            }

            // Фолбэк: сфера вокруг позиции (масштаб — наибольший)
            if (!hit)
            {
                const float maxScale = pickMax(scale.x, pickMax(scale.y, scale.z));
                hit = raySphereHit(
                    rayOrigin, rayDirection, position,
                    pickFallbackRadius * maxScale, hitT);
            }

            if (hit && hitT < bestT)
            {
                bestT = hitT;
                bestEntity = id;
            }
        }

        // Клик мимо — снять выбор (как в Unity/Unreal)
        this->selectEntity(bestEntity);
    }

    void GravelandsEditorHost::onUi()
    {
        // Верхняя полоса эдитора (позицию/размер уже выставил каркас):
        // имя игры + счётчик сущностей редактируемой сцены + кнопки PIE
        if (ImGui::Begin(topBarTitle, nullptr, topBarFlags))
        {
            ImGui::Text("%s | entities: %u | F5 hotreload | ` console",
                gameTitle,
                static_cast<unsigned int>(this->getScene().getEntityCount()));

            ImGui::SameLine();
            if (this->impl->pieSession.isRunning() || this->impl->pieStartPending)
            {
                // Статус: на паузе — «paused» (мир замер, кадр в Game)
                const bool paused = this->impl->pieSession.isPaused();
                ImGui::TextUnformatted(paused ? piePausedLabel : pieRunningLabel);
                ImGui::SameLine();

                // Пауза — только у запущенной сессии (при отложенном
                // старте клиента ещё нет); кнопка подсвечена, пока
                // сессия на паузе (как в Unity)
                if (this->impl->pieSession.isRunning())
                {
                    if (beng::editor::iconButton(
                        this->getIconFont(), beng::editor::icons::pause, paused,
                        pieButtonIconSize,
                        paused ? resumeButtonLabel : pauseButtonLabel))
                    {
                        this->impl->pieSession.setPaused(!paused);
                    }
                    ImGui::SameLine();
                }

                if (beng::editor::iconButton(
                    this->getIconFont(), beng::editor::icons::stop, false,
                    pieButtonIconSize, stopButtonLabel))
                {
                    // Stop синхронно допустим: контекст эдитора в этот
                    // момент текущий, клиентских ресурсов не под рукой
                    this->impl->pieStartPending = false;
                    this->stopPie();
                }
            }
            else
            {
                ImGui::TextUnformatted(pieStoppedLabel);
                ImGui::SameLine();
                if (beng::editor::iconButton(
                    this->getIconFont(), beng::editor::icons::play, false,
                    pieButtonIconSize, playButtonLabel))
                {
                    // Отложенный запуск: выполнится в начале следующего
                    // кадра (onSceneWillUpdate) — см. pieStartPending
                    this->impl->pieStartPending = true;
                }
            }
        }
        ImGui::End();
    }

    bool GravelandsEditorHost::onEscapePressed()
    {
        // PIE идёт — Escape останавливает его (как в Unity); кадр
        // остаётся в Game-панели, вкладка возвращается на Scene.
        // Сессия не запущена — нажатие не наше: каркас закроет
        // приложение
        if (this->impl->pieSession.isRunning())
        {
            this->stopPie();
            return true;
        }

        return false;
    }

    void GravelandsEditorHost::startPie()
    {
        if (this->impl->pieSession.isRunning())
        {
            return;
        }

        if (!this->impl->pieSession.start(serverDefaultPort))
        {
            __blib_log_error("PIE: failed to start session");
            return;
        }

        // Ввод игры и горячие клавиши эдитора конфликтуют на общей
        // клавиатуре (W/E/R и т.д.) — редакторский ввод выключается
        this->setEditorInputEnabled(false);

        // Автопереключение на вкладку Game (как Unity при Play)
        if (this->impl->gameTabId != invalidCenterTabId)
        {
            this->setActiveCenterTab(this->impl->gameTabId);
        }
    }

    void GravelandsEditorHost::stopPie()
    {
        if (!this->impl->pieSession.isRunning())
        {
            return;
        }

        this->impl->pieSession.stop();
        this->setEditorInputEnabled(true);

        // Возврат на вкладку Scene (как Unity после Stop)
        this->setActiveCenterTab(sceneCenterTabId);
    }

    GravelandsEditorHost::GravelandsEditorHost()
        : impl(nullptr)
    {
    }

    GravelandsEditorHost::~GravelandsEditorHost()
    {
        // Страховка: если владелец не вызвал shutdown явно
        this->shutdown();
    }

    bool GravelandsEditorHost::initialize(
        _In uint16_t windowWidth, _In uint16_t windowHeight, _In const char* windowTitle)
    {
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();

        // Аллокация через GlobalAllocator + placement new (проектное
        // правило: выделяющие new/delete запрещены). impl хоста
        // создаётся ДО каркаса: onInitialize() внутри каркасного
        // initialize() уже использует мир
        this->impl = static_cast<GravelandsEditorHostImpl*>(globalAllocator.allocate(sizeof(GravelandsEditorHostImpl)));
        new (this->impl) GravelandsEditorHostImpl();

        // Параметры окна — от вызывающего (плагин-контракт зовёт
        // initialize() без аргументов — дефолты каркаса)
        return this->EditorApplication::initialize(windowWidth, windowHeight, windowTitle);
    }

    void GravelandsEditorHost::shutdown()
    {
        if (this->impl == nullptr)
        {
            return;
        }

        // PIE-сессия гасится первой (клиентское окно/GL-контекст),
        // затем мир, затем каркас
        this->stopPie();

        // Game-превью FBO: явное разрушение до impl (GL-контекст
        // каркаса ещё жив — ImGui/окно гасятся только ниже)
        if (this->impl->gameTarget != nullptr)
        {
            this->impl->gameTarget->~IRenderTarget();
            blib::memory::GlobalAllocator::instance().deallocate(
                this->impl->gameTarget, sizeof(blib::graphics::IRenderTarget));
            this->impl->gameTarget = nullptr;
        }

        // Мир разрушаем ДО каркаса: меши обязаны умереть раньше
        // ImGui/GL-контекста каркаса
        this->impl->~GravelandsEditorHostImpl();
        blib::memory::GlobalAllocator::instance().deallocate(this->impl, sizeof(GravelandsEditorHostImpl));
        this->impl = nullptr;

        // Каркас: ImGui, окно, сцена (мир уже разрушен)
        this->EditorApplication::shutdown();
    }

    beng::editor::EditorApplication* gravelandsCreateEditorHost()
    {
        // Фабрика хоста (внутренняя функция модуля): снаружи модуль
        // торчит только точкой входа bengGetGameModule (см. ниже).
        // Память — GlobalAllocator; владелец гасит парной
        // gravelandsDestroyEditorHost
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();
        GravelandsEditorHost* host = static_cast<GravelandsEditorHost*>(
            globalAllocator.allocate(sizeof(GravelandsEditorHost)));
        new (host) GravelandsEditorHost();
        return static_cast<beng::editor::EditorApplication*>(host);
    }

    void gravelandsDestroyEditorHost(_In beng::editor::EditorApplication* host)
    {
        // Парная фабрике: конкретный тип известен только плагину —
        // явный деструктор + возврат памяти GlobalAllocator'у
        // (в DLL-режиме вызывающий не может сделать это сам)
        if (host == nullptr)
        {
            return;
        }

        GravelandsEditorHost* typed = static_cast<GravelandsEditorHost*>(host);
        typed->~GravelandsEditorHost();
        blib::memory::GlobalAllocator::instance().deallocate(typed, sizeof(GravelandsEditorHost));
    }

    namespace
    {
        // Статический id игры — из common/config.h (контрактный
        // идентификатор: имя gravelands.dll и значение --game)
        const char* getGameModuleName()
        {
            return gameModuleName;
        }

        // Игровой модуль эдитора: контракт GameModuleFunctions
        // (beng/editor/gameModule.h) — единственная точка стыковки
        // «эдитор ↔ игра». Память — статическая, освобождать не нужно
        const beng::editor::GameModuleFunctions gameModuleFunctions = {
            &getGameModuleName,
            &gravelandsCreateEditorHost,
            &gravelandsDestroyEditorHost,
            beng::editor::gameModuleContractVersion
        };
    }

} // namespace gravelands

// Точка входа игрового модуля — глобальный extern "C"-символ
// (объявление — в заголовке, ВНЕ namespace gravelands): единственный
// экспортируемый символ gravelands.dll (стабильное имя —
// gameModuleEntryName); статический режим зовёт её напрямую
const beng::editor::GameModuleFunctions* bengGetGameModule()
{
    // Статическая структура контракта (см. выше): память не
    // требует освобождения
    return &gravelands::gameModuleFunctions;
}
