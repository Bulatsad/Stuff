#include <gravelands/plugin/gravelandsEditorHost.h>
#include <gravelands/common/config.h>
#include <gravelands/world/world.h>

#include <beng/core/scene.h>
#include <beng/editor/panels/inspectorPanel.h>
#include <beng/editor/panels/sceneHierarchyPanel.h>

#include <blib/core/console/console.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/shader.h>
#include <blib/system/memory/globalAllocator.h>

#include <imgui/imgui.h>

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
    }

    // Внутренности хоста: мир Gravelands + панели эдитора. Каркас
    // (окно, FBO, камера, ImGui, сцена эдитора) живёт в базовом
    // EditorApplication. Полное определение скрыто в .cpp (pimpl) —
    // заголовок не тянет графические типы в потребителей.
    struct GravelandsEditorHost::GravelandsEditorHostImpl
    {
        // Мир: привязывается к сцене каркаса (та же сцена, что правит
        // клиент — см. GRAVELANDS.md). Разрушается в shutdown() хоста
        // ДО каркасного shutdown — меши освобождают GL-ресурсы при
        // живом контексте
        gravelands::World world;

        // Панели эдитора: иерархия сущностей сцены + инспектор полей
        // (рефлексия). Регистрируются в зоны каркаса в onInitialize
        beng::editor::SceneHierarchyPanel sceneHierarchyPanel;
        beng::editor::InspectorPanel inspectorPanel;

        GravelandsEditorHostImpl()
            : world()
            , sceneHierarchyPanel()
            , inspectorPanel()
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

        // Панели эдитора: Hierarchy (сущности сцены) слева сверху,
        // Inspector (поля через рефлексию) справа. Inspector берёт
        // выбор из Hierarchy (связка панелей)
        this->impl->sceneHierarchyPanel.setScene(&scene);
        this->impl->inspectorPanel.setScene(&scene);
        this->impl->inspectorPanel.setHierarchyPanel(&this->impl->sceneHierarchyPanel);

        this->registerPanel(&this->impl->sceneHierarchyPanel, beng::editor::PanelZone::LeftTop);
        this->registerPanel(&this->impl->inspectorPanel, beng::editor::PanelZone::Right);
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
