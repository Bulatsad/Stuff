#include <model_viewer/core/viewerCore.h>

#include <beng/client/components/animatorComponent.h>
#include <beng/client/components/skinnedMeshComponent.h>
#include <beng/components/transform.h>
#include <beng/core/scene.h>
#include <beng/editor/panels/animationPanel.h>
#include <beng/editor/panels/dialogWindow.h>
#include <beng/editor/panels/hierarchyPanel.h>
#include <beng/editor/panels/renderOptionsPanel.h>

#include <blib/core/math/angle.h>
#include <blib/core/math/quaternion.h>
#include <blib/graphics/color.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/lineRenderer.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/skelet.h>
#include <blib/graphics/skinmodel.h>
#include <blib/system/memory/globalAllocator.h>

#include <imgui/imgui.h>

#include <Windows.h>
#include <commdlg.h>
#include <gl/GL.h>

namespace modelviewer
{
    namespace
    {
        // ---------------------------------------------------------------
        // Константы вьювера (правило проекта: без вшитых литералов)
        // ---------------------------------------------------------------

        // Окно (передаётся каркасу в initialize)
        constexpr uint16_t windowWidth = 1280;
        constexpr uint16_t windowHeight = 720;
        constexpr const char* windowTitle = "Model Viewer";

        // MD5-модели приходят с осью Z вверх, мир вьювера — Y вверх:
        // поворачиваем модель на -90° вокруг X. FBX/DAE/OBJ — Y вверх,
        // для них поворот не нужен (см. isMd5MeshPath)
        constexpr float md5ZUpToYUpDegrees = -90.0f;
        // Расширение файлов MD5-моделей (по нему определяется ось вверх)
        constexpr const char* md5MeshExtension = ".md5mesh";

        // Ширина кнопок верхней панели (Open + Browse + Change Skin +
        // Add Animation) с отступами
        constexpr float modelBarButtonsWidth = 420.0f;

        // Путь к модели: буфер ввода в верхней панели
        constexpr size_t modelPathBufferSize = 512;

        // Строки UI
        constexpr const char* modelBarTitle = "Model";
        constexpr const char* pathInputLabel = "Path";
        constexpr const char* openButtonLabel = "Open";
        constexpr const char* browseButtonLabel = "Browse...";
        constexpr const char* changeSkinButtonLabel = "Change Skin...";
        constexpr const char* addAnimationButtonLabel = "Add Animation...";
        constexpr const char* fileDialogTitle = "Open 3D model";
        constexpr const char* skinFileDialogTitle = "Change skin (same skeleton)";
        constexpr const char* animationFileDialogTitle = "Add animation";
        constexpr const char* modelFileFilter =
            "3D Models (*.md5mesh;*.fbx;*.obj;*.dae)\0*.md5mesh;*.fbx;*.obj;*.dae\0All Files (*.*)\0*.*\0";
        constexpr const char* skinFileFilter =
            "Skinned Models (*.fbx;*.dae;*.obj)\0*.fbx;*.dae;*.obj\0All Files (*.*)\0*.*\0";
        constexpr const char* animationFileFilter =
            "Animations (*.fbx;*.dae)\0*.fbx;*.dae\0All Files (*.*)\0*.*\0";

        // Диалог несовместимого скина (DialogWindow)
        constexpr const char* skinMismatchDialogTitle = "Skeleton mismatch";
        constexpr const char* skinMismatchDialogMessage =
            "The new skin has a different skeleton (bone count / names / bind pose).\n"
            "Weights of unknown bones will be dropped and renormalized.\n"
            "Apply anyway?";
        constexpr const char* forceApplyButtonLabel = "Force Apply";

        // Цвета скелета
        constexpr buint8 skeletonGrayComponent = 120;
        constexpr buint8 opaqueAlphaComponent = 255;
        constexpr buint8 selectedBoneRedComponent = 255;
        constexpr buint8 selectedBoneGreenComponent = 210;
        constexpr buint8 selectedBoneBlueComponent = 60;
    }

    // ---------------------------------------------------------------
    // Внутренности вьювера: модель, панели, диалоги, отладочные слои.
    // Каркас (окно, FBO, камера, ECS-сцена, ImGui) живёт в базовом
    // EditorApplication. Полное определение скрыто в .cpp (pimpl) —
    // заголовок не тянет графические типы blib в потребителей.
    // ---------------------------------------------------------------
    struct ViewerCore::ViewerCoreImpl
    {
        // Рендерер линий отладочного скелета. Объявлен ДО панелей,
        // разрушается после них — и до гашения каркаса (shutdown()
        // уничтожает impl раньше GL-контекста)
        blib::graphics::LineRenderer skeletonRenderer;

        // Сущность текущей модели (invalidEntity — модели нет)
        beng::EntityID modelEntity;

        // Панели вьювера (не владеют данными — привязки снимаются
        // в unloadModel, см. BENG.md)
        beng::editor::HierarchyPanel hierarchyPanel;
        beng::editor::AnimationPanel animationPanel;
        beng::editor::RenderOptionsPanel renderOptionsPanel;

        // Модальный диалог несовместимого скина (Force Apply / Cancel).
        // Рисуется каждый кадр в onUi; состояние (путь кандидата)
        // захватывается лямбдой колбэка
        beng::editor::DialogWindow skinMismatchDialog;

        // Верхняя панель: путь к модели
        char modelPathBuffer[modelPathBufferSize];

        ViewerCoreImpl()
            : skeletonRenderer()
            , modelEntity(beng::invalidEntity)
            , hierarchyPanel()
            , animationPanel()
            , renderOptionsPanel()
            , skinMismatchDialog()
        {
            // Пустой буфер пути (иначе — мусор в поле ввода)
            memset(this->modelPathBuffer, 0, modelPathBufferSize);
        }
    };

    namespace
    {
        // Извлечение позиции сустава из глобальной трансформации кости:
        // column-major, трансляция хранится в последней колонке
        // (data[3][0..2]; см. CORE.md, «Конвенция матриц»)
        blib::graphics::Vector3f boneWorldPosition(_In const blib::graphics::TransformMatrix& transform)
        {
            return blib::graphics::Vector3f(
                transform.data[3][0],
                transform.data[3][1],
                transform.data[3][2]);
        }

        // MD5-модель (расширение ".md5mesh", без учёта регистра)?
        // Только MD5 приходит с осью Z вверх и требует поворота на -90°
        // вокруг X; FBX/DAE/OBJ — Y вверх, их не поворачиваем
        bool isMd5MeshPath(_In const std::string& path)
        {
            const size_t extensionLength = std::char_traits<char>::length(md5MeshExtension);
            if (path.size() < extensionLength)
            {
                return false;
            }

            const std::string suffix = path.substr(path.size() - extensionLength);
            for (size_t i = 0; i < extensionLength; ++i)
            {
                const char c = suffix[i];
                const char lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
                if (lower != md5MeshExtension[i])
                {
                    return false;
                }
            }
            return true;
        }
    }

    // ---------------------------------------------------------------
    // Хуки EditorApplication
    // ---------------------------------------------------------------

    void ViewerCore::onInitialize(_In beng::Scene& scene)
    {
        // Движковые типы (SkinnedMesh/Animator и др.) уже
        // зарегистрированы каркасом — у вьювера своих типов нет
        (void)scene;

        // Панели вьювера в дефолтную раскладку каркаса
        this->registerPanel(&this->impl->hierarchyPanel, beng::editor::PanelZone::LeftTop);
        this->registerPanel(&this->impl->renderOptionsPanel, beng::editor::PanelZone::LeftBottom);
        this->registerPanel(&this->impl->animationPanel, beng::editor::PanelZone::Right);
    }

    void ViewerCore::onInput()
    {
        // Ctrl+O — диалог выбора модели
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::O) &&
            blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::LControl))
        {
            this->browseModelFile();
        }
    }

    void ViewerCore::onSceneWillUpdate(float deltaTime)
    {
        (void)deltaTime;

        // Флаг диффузных текстур читается при отрисовке мешей
        // (RenderSystem внутри scene.update)
        this->getRenderTarget().rc.useDiffuseTextures =
            this->impl->renderOptionsPanel.isDiffuseTextureVisible();
    }

    void ViewerCore::onSceneDidUpdate(float deltaTime)
    {
        (void)deltaTime;

        // Отладочные слои поверх сцены (в тот же FBO вьюпорта)
        if (this->impl->renderOptionsPanel.isSkeletonVisible())
        {
            this->drawSkeleton();
        }
        if (this->impl->renderOptionsPanel.isWireframeVisible())
        {
            this->drawWireframe();
        }
    }

    void ViewerCore::onUi()
    {
        // Верхняя полоса (позицию/размер уже выставил каркас) +
        // модальный диалог несовместимого скина
        this->drawModelBar();
        this->impl->skinMismatchDialog.draw();
    }

    bool ViewerCore::onEscapePressed()
    {
        // Escape отменяет модальный диалог: DialogWindow сам ловит
        // ImGui-событие Escape внутри попапа, окно приложения
        // закрывать НЕ нужно (см. BENG.md, «Подводные камни»)
        return this->impl->skinMismatchDialog.isOpen();
    }

    // ---------------------------------------------------------------
    // Жизненный цикл
    // ---------------------------------------------------------------

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
        // правило: выделяющие new/delete запрещены). impl вьювера
        // создаётся ДО каркаса: onInitialize() внутри каркасного
        // initialize() уже использует его панели
        this->impl = static_cast<ViewerCoreImpl*>(globalAllocator.allocate(sizeof(ViewerCoreImpl)));
        new (this->impl) ViewerCoreImpl();

        return this->EditorApplication::initialize(windowWidth, windowHeight, windowTitle);
    }

    void ViewerCore::shutdown()
    {
        if (this->impl == nullptr)
        {
            return;
        }

        // Модель снимаем заранее: панели ссылаются на её данные
        this->unloadModel();

        // impl вьювера разрушаем ДО каркаса: панели и LineRenderer
        // обязаны умереть раньше ImGui/GL-контекста
        this->impl->~ViewerCoreImpl();
        blib::memory::GlobalAllocator::instance().deallocate(this->impl, sizeof(ViewerCoreImpl));
        this->impl = nullptr;

        // Каркас: ImGui, окно, сцена (модель уже выгружена)
        this->EditorApplication::shutdown();
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

        beng::Scene& scene = this->getScene();

        // Entity модели: Transform создаётся сценой автоматически
        // (инвариант: каждая сущность рождается с Transform) +
        // SkinnedMesh (данные) + Animator (плейбек)
        const beng::EntityID entity = scene.createEntity();
        beng::SkinnedMeshComponent& meshComp = scene.addComponent<beng::SkinnedMeshComponent>(entity);
        beng::AnimatorComponent& animComp = scene.addComponent<beng::AnimatorComponent>(entity);

        if (__blib_unlikely(!meshComp.loadFromFile(path)))
        {
            __blib_log_error("failed to load model '%s'", path.c_str());
            scene.destroyEntity(entity);
            return;
        }

        blib::graphics::SkinModel* model = meshComp.getModel();

        // Ориентация осей: MD5 — Z вверх, мир вьювера — Y вверх,
        // поворот на -90° вокруг X. FBX/DAE/OBJ — Y вверх: поворот
        // не применяется (у них локальный поворот остаётся identity)
        beng::TransformComponent& transform = scene.getComponent<beng::TransformComponent>(entity);
        if (isMd5MeshPath(path))
        {
            transform.setLocalRotation(blib::math::Quaternion<float>(
                blib::math::AngleDegreef(md5ZUpToYUpDegrees),
                blib::math::Vector<float, 3>(1.0f, 0.0f, 0.0f)));
        }

        // Плейбек: зацикливание по умолчанию включено, анимация НЕ
        // стартует — модель рисуется как есть (bind-поза, ТЗ)
        animComp.setAnimator(&model->getAnimator());
        animComp.setLoop(true);

        // Панели привязываем к новой модели
        this->impl->hierarchyPanel.setSkelet(&model->getSkelet());
        this->impl->animationPanel.setAnimatorComponent(&animComp);

        this->impl->modelEntity = entity;

        // Камеру возвращаем к модели (цель — в начало координат)
        this->resetCamera();

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
        this->getScene().destroyEntity(this->impl->modelEntity);
        this->impl->modelEntity = beng::invalidEntity;

        // Панели больше не должны ссылаться на модель
        this->impl->hierarchyPanel.setSkelet(nullptr);
        this->impl->animationPanel.setAnimatorComponent(nullptr);
    }

    bool ViewerCore::browseFile(_In const char* title, _In const char* filter, _Out char* outPath, size_t outSize)
    {
        // Стандартный диалог выбора файла (Win32). Буфер MAX_PATH —
        // системная константа
        char pathBuffer[MAX_PATH] = "";
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = __blib_render_window_context(this->getWindow().__getCtx())->hwnd;
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = pathBuffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

        if (!GetOpenFileNameA(&ofn))
        {
            return false;
        }

        strncpy_s(outPath, outSize, pathBuffer, _TRUNCATE);
        return true;
    }

    void ViewerCore::browseModelFile()
    {
        // Путь показываем в поле ввода и сразу загружаем
        if (this->browseFile(fileDialogTitle, modelFileFilter, this->impl->modelPathBuffer, modelPathBufferSize))
        {
            this->loadModel(std::string(this->impl->modelPathBuffer));
        }
    }

    void ViewerCore::changeSkin()
    {
        if (this->impl->modelEntity == beng::invalidEntity)
        {
            __blib_log_warning("change skin: no model loaded");
            return;
        }

        char pathBuffer[MAX_PATH] = "";
        if (!this->browseFile(skinFileDialogTitle, skinFileFilter, pathBuffer, MAX_PATH))
        {
            return;
        }

        beng::SkinnedMeshComponent* meshComp =
            this->getScene().tryGetComponent<beng::SkinnedMeshComponent>(this->impl->modelEntity);
        if (__blib_unlikely(!meshComp || !meshComp->getModel()))
        {
            __blib_log_error("change skin: model component is not available");
            return;
        }

        // Скелет, аниматор и плейбек не трогаются — панели остаются
        // привязанными к тем же объектам, выбранная кость сохраняется
        if (!meshComp->loadSkinFromFile(std::string(pathBuffer)))
        {
            // Несовместимый скелет (или ошибка загрузки): предлагаем
            // форсированное применение. Путь захватывается лямбдой —
            // диалог сам не хранит данные приложения
            this->impl->skinMismatchDialog.open(
                skinMismatchDialogTitle,
                skinMismatchDialogMessage,
                forceApplyButtonLabel,
                [this, path = std::string(pathBuffer)]() { this->applySkinForced(path); });
        }
    }

    void ViewerCore::applySkinForced(_In const std::string& path)
    {
        if (this->impl->modelEntity == beng::invalidEntity)
        {
            __blib_log_warning("force skin: no model loaded");
            return;
        }

        beng::SkinnedMeshComponent* meshComp =
            this->getScene().tryGetComponent<beng::SkinnedMeshComponent>(this->impl->modelEntity);
        if (__blib_unlikely(!meshComp || !meshComp->getModel()))
        {
            __blib_log_error("force skin: model component is not available");
            return;
        }

        if (meshComp->loadSkinFromFile(path, true))
        {
            __blib_log_info("skin force-applied: %s", path.c_str());
        }
    }

    void ViewerCore::addAnimation()
    {
        if (this->impl->modelEntity == beng::invalidEntity)
        {
            __blib_log_warning("add animation: no model loaded");
            return;
        }

        char pathBuffer[MAX_PATH] = "";
        if (!this->browseFile(animationFileDialogTitle, animationFileFilter, pathBuffer, MAX_PATH))
        {
            return;
        }

        beng::SkinnedMeshComponent* meshComp =
            this->getScene().tryGetComponent<beng::SkinnedMeshComponent>(this->impl->modelEntity);
        if (__blib_unlikely(!meshComp || !meshComp->getModel()))
        {
            __blib_log_error("add animation: model component is not available");
            return;
        }

        if (!meshComp->loadAnimationsFromFile(std::string(pathBuffer)))
        {
            return;
        }

        // Сразу выбираем последний добавленный клип и запускаем его
        beng::AnimatorComponent* animComp =
            this->getScene().tryGetComponent<beng::AnimatorComponent>(this->impl->modelEntity);
        if (animComp)
        {
            const std::vector<blib::graphics::AnimationClip>& animations = animComp->getAnimations();
            if (!animations.empty())
            {
                animComp->selectAnimation(animations.back().name);
                animComp->play();
            }
        }
    }

    void ViewerCore::drawSkeleton()
    {
        if (this->impl->modelEntity == beng::invalidEntity)
        {
            return;
        }

        beng::SkinnedMeshComponent* meshComp =
            this->getScene().tryGetComponent<beng::SkinnedMeshComponent>(this->impl->modelEntity);
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
        blib::graphics::IRenderTarget& renderTarget = this->getRenderTarget();
        renderTarget.rc.api.ogl.__blib_glDisable(GL_DEPTH_TEST);
        renderTarget.draw(this->impl->skeletonRenderer);
        renderTarget.rc.api.ogl.__blib_glEnable(GL_DEPTH_TEST);
    }

    void ViewerCore::drawWireframe()
    {
        if (this->impl->modelEntity == beng::invalidEntity)
        {
            return;
        }

        beng::SkinnedMeshComponent* meshComp =
            this->getScene().tryGetComponent<beng::SkinnedMeshComponent>(this->impl->modelEntity);
        if (__blib_unlikely(!meshComp || !meshComp->getModel()))
        {
            return;
        }

        blib::graphics::IRenderTarget& renderTarget = this->getRenderTarget();
        blib::graphics::RenderContext& rc = renderTarget.rc;
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
        renderTarget.draw(*meshComp->getModel());
        rc.useDiffuseTextures = fillUsesTextures;

        rc.api.ogl.__blib_glDisable(GL_POLYGON_OFFSET_LINE);
        rc.api.ogl.__blib_glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    void ViewerCore::drawModelBar()
    {
        // Позицию/размер полосы уже выставил каркас перед onUi
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

            ImGui::SameLine();
            const bool changeSkinClicked = ImGui::Button(changeSkinButtonLabel);

            ImGui::SameLine();
            const bool addAnimationClicked = ImGui::Button(addAnimationButtonLabel);

            if ((enterPressed || openClicked) && this->impl->modelPathBuffer[0] != '\0')
            {
                this->loadModel(std::string(this->impl->modelPathBuffer));
            }
            if (browseClicked)
            {
                this->browseModelFile();
            }
            if (changeSkinClicked)
            {
                this->changeSkin();
            }
            if (addAnimationClicked)
            {
                this->addAnimation();
            }
        }
        ImGui::End();
    }

} // namespace modelviewer
