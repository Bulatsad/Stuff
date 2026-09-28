#include <gravelands/plugin/gravelandsEditorHost.h>
#include <gravelands/common/config.h>
#include <gravelands/world/world.h>

#include <beng/components/transform.h>
#include <beng/core/scene.h>

#include <blib/core/console/console.h>
#include <blib/graphics/color.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/lineRenderer.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/shader.h>
#include <blib/system/memory/globalAllocator.h>

#include <imgui/imgui.h>

#include <new>

#include <gl/GL.h>

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

        // Gizmo выбранной сущности: длина осей (мир. ед.) и цвета
        // осей X/Y/Z (красный/зелёный/синий)
        constexpr float gizmoAxisLength = 25.0f;
        constexpr buint8 gizmoAxisXColorR = 255;
        constexpr buint8 gizmoAxisXColorG = 90;
        constexpr buint8 gizmoAxisXColorB = 90;
        constexpr buint8 gizmoAxisYColorR = 90;
        constexpr buint8 gizmoAxisYColorG = 255;
        constexpr buint8 gizmoAxisYColorB = 90;
        constexpr buint8 gizmoAxisZColorR = 90;
        constexpr buint8 gizmoAxisZColorG = 90;
        constexpr buint8 gizmoAxisZColorB = 255;
        constexpr buint8 gizmoAxisAlpha = 255;
    }

    // Внутренности хоста: мир Gravelands + gizmo выбора. Каркас
    // (окно, FBO, камера, ImGui, сцена эдитора, сценные панели
    // Hierarchy/Inspector) живёт в базовом EditorApplication. Полное
    // определение скрыто в .cpp (pimpl) — заголовок не тянет
    // графические типы в потребителей.
    struct GravelandsEditorHost::GravelandsEditorHostImpl
    {
        // Gizmo выбранной сущности (оси в мировых координатах).
        // LineRenderer не освобождает GL-ресурсы; живёт до гашения
        // каркаса (impl разрушается в shutdown() раньше GL-контекста)
        blib::graphics::LineRenderer selectionGizmo;

        // Мир: привязывается к сцене каркаса (та же сцена, что правит
        // клиент — см. GRAVELANDS.md). Разрушается в shutdown() хоста
        // ДО каркасного shutdown — меши освобождают GL-ресурсы при
        // живом контексте
        gravelands::World world;

        GravelandsEditorHostImpl()
            : selectionGizmo()
            , world()
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

    void GravelandsEditorHost::onSceneDidUpdate(float deltaTime)
    {
        (void)deltaTime;

        // Gizmo выбранной сущности — в тот же FBO вьюпорта, поверх
        // отрисованной сцены
        this->drawSelectionGizmo();
    }

    void GravelandsEditorHost::drawSelectionGizmo()
    {
        // Выбор живёт в каркасе (пишет Scene Hierarchy, см.
        // EditorApplication::getSelectedEntity)
        const beng::EntityID selected = this->getSelectedEntity();
        if (__blib_unlikely(selected == beng::invalidEntity))
        {
            return;
        }

        // Выбор жив (Hierarchy сбрасывает протухший); мировая позиция —
        // начало осей (без поворота: маркер в мировых осях)
        beng::TransformComponent* transform =
            this->getScene().tryGetComponent<beng::TransformComponent>(selected);
        if (__blib_unlikely(transform == nullptr))
        {
            return;
        }

        const blib::math::Vector<float, 3> origin = transform->getWorldPosition();

        const blib::graphics::Color axisXColor(
            gizmoAxisXColorR, gizmoAxisXColorG, gizmoAxisXColorB, gizmoAxisAlpha);
        const blib::graphics::Color axisYColor(
            gizmoAxisYColorR, gizmoAxisYColorG, gizmoAxisYColorB, gizmoAxisAlpha);
        const blib::graphics::Color axisZColor(
            gizmoAxisZColorR, gizmoAxisZColorG, gizmoAxisZColorB, gizmoAxisAlpha);

        blib::graphics::LineRenderer& gizmo = this->impl->selectionGizmo;
        gizmo.clear();
        gizmo.addLine(
            origin,
            origin + blib::math::Vector<float, 3>(gizmoAxisLength, 0.0f, 0.0f),
            axisXColor);
        gizmo.addLine(
            origin,
            origin + blib::math::Vector<float, 3>(0.0f, gizmoAxisLength, 0.0f),
            axisYColor);
        gizmo.addLine(
            origin,
            origin + blib::math::Vector<float, 3>(0.0f, 0.0f, gizmoAxisLength),
            axisZColor);

        // Оси рисуем поверх мешей (X-ray): выключаем тест глубины на
        // время отрисовки и возвращаем его обратно (паттерн вьювера)
        blib::graphics::IRenderTarget& renderTarget = this->getRenderTarget();
        renderTarget.rc.api.ogl.__blib_glDisable(GL_DEPTH_TEST);
        renderTarget.draw(gizmo);
        renderTarget.rc.api.ogl.__blib_glEnable(GL_DEPTH_TEST);
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
