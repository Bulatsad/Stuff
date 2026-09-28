#include <gravelands/plugin/gravelandsEditorHost.h>
#include <gravelands/common/config.h>
#include <gravelands/world/world.h>

#include <beng/components/transform.h>
#include <beng/client/components/meshRenderComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/core/scene.h>

#include <blib/core/console/console.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/mesh.h>
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
        // Параметры окна единого эдитора с игрой Gravelands
        // (правило проекта: без вшитых литералов)
        constexpr uint16_t editorWindowWidth = 1280;
        constexpr uint16_t editorWindowHeight = 720;
        constexpr const char* editorWindowTitle = "beng-editor";

        // Имя консольной команды перезагрузки шейдеров (см. F5)
        constexpr const char* hotreloadCommand = "hotreload";

        // Заголовок верхней полосы эдитора (полоса резервируется
        // каркасом — позицию/размер выставляет EditorApplication)
        constexpr const char* topBarTitle = "Gravelands";

        // Флаги верхней полосы: без изменения размера
        constexpr ImGuiWindowFlags topBarFlags = ImGuiWindowFlags_NoResize;

        // Ray-picking (выбор кликом во вьюпорте): максимальная
        // дистанция луча (дальше — «мимо»), радиус сферы-фолбэка для
        // сущностей без мешей, эпсилон вырожденных сравнений
        constexpr float pickMaxDistance = 100000.0f;
        constexpr float pickFallbackRadius = 8.0f;
        constexpr float pickEpsilon = 0.000001f;
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

    // Внутренности хоста: мир Gravelands. Каркас (окно, FBO, камера,
    // ImGui, сцена эдитора, сценные панели Hierarchy/Inspector, gizmo)
    // живёт в базовом EditorApplication. Полное определение скрыто в
    // .cpp (pimpl) — заголовок не тянет графические типы в потребителей.
    struct GravelandsEditorHost::GravelandsEditorHostImpl
    {
        // Мир: привязывается к сцене каркаса (та же сцена, что правит
        // клиент — см. GRAVELANDS.md). Разрушается в shutdown() хоста
        // ДО каркасного shutdown — меши освобождают GL-ресурсы при
        // живом контексте
        gravelands::World world;

        GravelandsEditorHostImpl()
            : world()
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
        // имя игры + счётчик сущностей редактируемой сцены
        if (ImGui::Begin(topBarTitle, nullptr, topBarFlags))
        {
            ImGui::Text("%s | entities: %u | F5 hotreload | ` console",
                gameTitle,
                static_cast<unsigned int>(this->getScene().getEntityCount()));
        }
        ImGui::End();
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

    bool GravelandsEditorHost::initialize()
    {
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();

        // Аллокация через GlobalAllocator + placement new (проектное
        // правило: выделяющие new/delete запрещены). impl хоста
        // создаётся ДО каркаса: onInitialize() внутри каркасного
        // initialize() уже использует мир
        this->impl = static_cast<GravelandsEditorHostImpl*>(globalAllocator.allocate(sizeof(GravelandsEditorHostImpl)));
        new (this->impl) GravelandsEditorHostImpl();

        return this->EditorApplication::initialize(editorWindowWidth, editorWindowHeight, editorWindowTitle);
    }

    void GravelandsEditorHost::shutdown()
    {
        if (this->impl == nullptr)
        {
            return;
        }

        // Мир разрушаем ДО каркаса: меши обязаны умереть раньше
        // ImGui/GL-контекста каркаса
        this->impl->~GravelandsEditorHostImpl();
        blib::memory::GlobalAllocator::instance().deallocate(this->impl, sizeof(GravelandsEditorHostImpl));
        this->impl = nullptr;

        // Каркас: ImGui, окно, сцена (мир уже разрушен)
        this->EditorApplication::shutdown();
    }

    GravelandsEditorHost* gravelandsCreateEditorHost()
    {
        // Фабрика — единственная точка входа плагина (на DLL-этапе
        // станет экспортируемым символом gravelands.dll). Память —
        // GlobalAllocator; владелец гасит и возвращает память сам
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();
        GravelandsEditorHost* host = static_cast<GravelandsEditorHost*>(
            globalAllocator.allocate(sizeof(GravelandsEditorHost)));
        new (host) GravelandsEditorHost();
        return host;
    }

} // namespace gravelands
