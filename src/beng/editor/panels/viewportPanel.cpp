#include <beng/editor/panels/viewportPanel.h>

#include <beng/editor/editorApplication.h>
#include <beng/editor/editorIcons.h>

#include <imgui/imgui.h>

namespace beng
{
    namespace editor
    {
        namespace
        {
            constexpr const char* panelTitle = "Viewport";
            constexpr const char* noTargetMessage = "No render target";

            // Чувствительности мышиного ввода (настраиваемые константы,
            // подобраны под типовой вьюпорт ~800x600)
            constexpr float rotateSensitivityDegreesPerPixel = 0.3f;
            constexpr float panScalePerPixel = 0.0015f;
            constexpr float zoomScalePerWheelNotch = 0.1f;

            // Тулбар инструментов (иконки W/E/R поверх изображения)
            constexpr float toolbarIconSizePx = 18.0f;
            constexpr float toolbarMargin = 10.0f;
            constexpr float toolbarBackgroundAlpha = 0.55f;
            constexpr float toolbarRounding = 6.0f;
            constexpr const char* moveTooltip = "Move (W)";
            constexpr const char* rotateTooltip = "Rotate (E)";
            constexpr const char* scaleTooltip = "Scale (R)";
        }

        ViewportPanel::ViewportPanel()
            : renderTarget(nullptr)
            , camera(nullptr)
            , iconFont(nullptr)
            , gizmoModeRef(nullptr)
            , toolbarMin(0.0f, 0.0f)
            , toolbarMax(0.0f, 0.0f)
            , toolbarRectValid(false)
            , toolbarHovered(false)
            , rotating(false)
            , panning(false)
            , lastViewportWidth(0.0f)
            , lastViewportHeight(0.0f)
            , cursorOverViewport(false)
            , clickedThisFrame(false)
            , clickNdcX(0.0f)
            , clickNdcY(0.0f)
            , cursorNdcX(0.0f)
            , cursorNdcY(0.0f)
            , cameraRotationEnabled(true)
        {
        }

        void ViewportPanel::setIconFont(_In_opt ImFont* font)
        {
            this->iconFont = font;
        }

        void ViewportPanel::setGizmoModeRef(_In_opt GizmoMode* mode)
        {
            this->gizmoModeRef = mode;
        }

        void ViewportPanel::setRenderTarget(_In_opt blib::graphics::IRenderTarget* target)
        {
            this->renderTarget = target;
        }

        void ViewportPanel::setCamera(_In_opt blib::graphics::OrbitCamera* cam)
        {
            this->camera = cam;
        }

        float ViewportPanel::getLastViewportWidth() const
        {
            return this->lastViewportWidth;
        }

        float ViewportPanel::getLastViewportHeight() const
        {
            return this->lastViewportHeight;
        }

        bool ViewportPanel::takeViewportClick(_Out float& outNdcX, _Out float& outNdcY)
        {
            if (!this->clickedThisFrame)
            {
                return false;
            }

            this->clickedThisFrame = false;
            outNdcX = this->clickNdcX;
            outNdcY = this->clickNdcY;
            return true;
        }

        void ViewportPanel::getCursorNdc(_Out float& outNdcX, _Out float& outNdcY) const
        {
            outNdcX = this->cursorNdcX;
            outNdcY = this->cursorNdcY;
        }

        void ViewportPanel::handleCameraInput()
        {
            if (__blib_unlikely(!this->camera))
            {
                return;
            }

            ImGuiIO& io = ImGui::GetIO();

            // Захват драга: только если нажатие произошло на самом
            // изображении (а не на заголовке/в другом окне).
            // IsItemClicked валиден — изображение всё ещё текущий item.
            // Предикат каркаса (курсор над стрелкой gizmo) отменяет
            // захват: клик уходит манипулятору, а не камере
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
                !(this->rotationBlockPredicate && this->rotationBlockPredicate()))
            {
                this->rotating = true;
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            {
                this->panning = true;
            }

            // ЛКМ-драг (начатый на изображении) — вращение вокруг цели.
            // Экранная ось Y растёт вниз, поэтому вертикальный угол
            // инвертирован: движение мыши вверх (dy < 0) поднимает камеру.
            // В gizmo-режиме (зажата G, есть выбор) вращение отключено —
            // драг двигает выбранную сущность (см. EditorApplication)
            if (this->cameraRotationEnabled &&
                this->rotating && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                this->camera->rotate(
                    io.MouseDelta.x * rotateSensitivityDegreesPerPixel,
                    -io.MouseDelta.y * rotateSensitivityDegreesPerPixel);
            }

            // СКМ-драг (начатый на изображении) — панорама. Масштаб
            // пропорционален дистанции: вблизи цели мир смещается медленнее
            if (this->panning && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            {
                const float panScale = this->camera->getDistance() * panScalePerPixel;
                this->camera->pan(
                    io.MouseDelta.x * panScale,
                    io.MouseDelta.y * panScale);
            }

            // Колёсико — зум: вперёд (wheel > 0) — приближение.
            // Не привязано к драгу, достаточно наведения на вьюпорт
            if (io.MouseWheel != 0.0f)
            {
                this->camera->zoom(-io.MouseWheel * this->camera->getDistance() * zoomScalePerWheelNotch);
            }

            // Углы/дистанция изменились — пересчитываем view-матрицу
            this->camera->update();
        }

        void ViewportPanel::draw()
        {
            // Окно вьюпорта зафиксировано: позицию/размер задаёт
            // вызывающий (SetNextWindowPos/Size), пользователь не может
            // двигать его заголовком — см. комментарий в заголовке
            ImGui::Begin(panelTitle, nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

            if (__blib_unlikely(!this->renderTarget))
            {
                ImGui::TextDisabled(noTargetMessage);
                ImGui::End();
                return;
            }

            // Текстура текущего кадра FBO (тот же буфер, в который
            // сцена была отрисована в этом кадре). Тип контекста
            // приватный в IRenderTarget — берём через auto
            const auto& rtCtx = this->renderTarget->getContext();
            const GLuint textureId = rtCtx.frameTextures[rtCtx.currentFrameBufferIndex].getContext().textureID;

            // Сохраняем пропорции кадра (FBO имеет свой фиксированный
            // аспект, панель — свой): изображение вписывается в
            // доступную область без растяжения (letterbox).
            // Вызывающий обычно подгоняет рендер-таргет под этот
            // размер — тогда аспекты совпадают и letterbox вырождается
            const ImVec2 availableSize = ImGui::GetContentRegionAvail();

            // Запоминаем доступный размер: по нему вызывающий
            // ресайзит FBO (1:1 пиксель)
            this->lastViewportWidth = availableSize.x;
            this->lastViewportHeight = availableSize.y;

            const float frameAspect = static_cast<float>(rtCtx.viewportWidth) / static_cast<float>(rtCtx.viewportHeight);
            const float panelAspect = (availableSize.y > 0.0f) ? (availableSize.x / availableSize.y) : 1.0f;

            ImVec2 imageSize = availableSize;
            if (panelAspect > frameAspect)
            {
                // Панель шире кадра — ограничиваем ширину
                imageSize.x = availableSize.y * frameAspect;
            }
            else
            {
                // Панель уже/выше — ограничиваем высоту
                imageSize.y = availableSize.x / frameAspect;
            }

            // Центрируем изображение внутри панели
            const ImVec2 imagePos(
                ImGui::GetCursorScreenPos().x + (availableSize.x - imageSize.x) * 0.5f,
                ImGui::GetCursorScreenPos().y + (availableSize.y - imageSize.y) * 0.5f);
            ImGui::SetCursorScreenPos(imagePos);

            // FBO хранит кадр вверх ногами (GL-координаты), поэтому
            // UV разворачиваем: ImGui считает (0,0) верхним левым углом.
            // ImTextureID в ImGui 1.92 — ImU64: GLuint кладём значением
            ImGui::Image(
                static_cast<ImTextureID>(textureId),
                imageSize,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));

            // Курсор над тулбаром (прямоугольник измерен в прошлом
            // кадре): клик/зум по кнопкам не должны уходить камере
            // и ray-picking'у
            this->toolbarHovered = this->toolbarRectValid &&
                ImGui::IsMouseHoveringRect(this->toolbarMin, this->toolbarMax);

            // Клик по изображению (не драг): фиксируем NDC курсора
            // внутри изображения для ray-picking каркаса (см.
            // takeViewportClick). Драг по-прежнему вращает камеру
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !this->toolbarHovered)
            {
                this->clickedThisFrame = true;
                const ImVec2 mousePos = ImGui::GetMousePos();
                this->clickNdcX = (mousePos.x - imagePos.x) / imageSize.x * 2.0f - 1.0f;
                this->clickNdcY = 1.0f - (mousePos.y - imagePos.y) / imageSize.y * 2.0f;
            }

            // Текущий NDC курсора (для gizmo-осей каркаса): обновляется
            // каждый кадр, даже если курсор не над изображением
            {
                const ImVec2 mousePos = ImGui::GetMousePos();
                this->cursorNdcX = (mousePos.x - imagePos.x) / imageSize.x * 2.0f - 1.0f;
                this->cursorNdcY = 1.0f - (mousePos.y - imagePos.y) / imageSize.y * 2.0f;
            }

            // Отпускание кнопки снимает захват драга в ЛЮБОМ месте —
            // даже если курсор уже не над вьюпортом (иначе флаг
            // «зависнет» до следующего наведения)
            ImGuiIO& io = ImGui::GetIO();
            if (!io.MouseDown[ImGuiMouseButton_Left])
            {
                this->rotating = false;
            }
            if (!io.MouseDown[ImGuiMouseButton_Middle])
            {
                this->panning = false;
            }

            // Ввод камеры — только пока курсор над изображением и НЕ
            // над тулбаром (клик по кнопкам инструментов не крутит
            // камеру; драг, начатый на изображении, продолжается и над
            // тулбаром — флаги захвата снимаются только отпусканием)
            this->cursorOverViewport = ImGui::IsItemHovered();
            if (this->cursorOverViewport && !this->toolbarHovered)
            {
                this->handleCameraInput();
            }

            // Тулбар инструментов — поверх изображения (после ввода:
            // кнопки не влияют на состояние камеры этого кадра)
            this->drawToolbar(imagePos);

            ImGui::End();
        }

        void ViewportPanel::drawToolbar(_In_ const ImVec2& imagePos)
        {
            this->toolbarRectValid = false;

            if (__blib_unlikely(this->iconFont == nullptr || this->gizmoModeRef == nullptr))
            {
                return;
            }

            const ImGuiStyle& style = ImGui::GetStyle();

            // Габариты подложки: три квадратные кнопки с отступами
            const buint32 buttonCount = 3;
            const float buttonSide = toolbarIconSizePx + style.FramePadding.y * 2.0f;
            const float barWidth =
                static_cast<float>(buttonCount) * buttonSide +
                (static_cast<float>(buttonCount) + 1.0f) * style.ItemSpacing.x;
            const float barHeight = buttonSide + style.ItemSpacing.y * 2.0f;

            const ImVec2 barMin(imagePos.x + toolbarMargin, imagePos.y + toolbarMargin);
            const ImVec2 barMax(barMin.x + barWidth, barMin.y + barHeight);

            // Полупрозрачная тёмная подложка — кнопки читаются на любом
            // кадре сцены (как у референсных эдиторов)
            ImU32 backgroundColor = ImGui::GetColorU32(ImGuiCol_WindowBg);
            backgroundColor = (backgroundColor & 0x00FFFFFF) |
                (static_cast<ImU32>(toolbarBackgroundAlpha * 255.0f) << 24);
            ImGui::GetWindowDrawList()->AddRectFilled(
                barMin, barMax, backgroundColor, toolbarRounding);

            ImGui::SetCursorScreenPos(ImVec2(
                barMin.x + style.ItemSpacing.x,
                barMin.y + style.ItemSpacing.y));

            if (iconButton(this->iconFont, icons::translate,
                *this->gizmoModeRef == GizmoMode::Translate, toolbarIconSizePx, moveTooltip))
            {
                *this->gizmoModeRef = GizmoMode::Translate;
            }
            ImGui::SameLine();
            if (iconButton(this->iconFont, icons::rotate,
                *this->gizmoModeRef == GizmoMode::Rotate, toolbarIconSizePx, rotateTooltip))
            {
                *this->gizmoModeRef = GizmoMode::Rotate;
            }
            ImGui::SameLine();
            if (iconButton(this->iconFont, icons::scale,
                *this->gizmoModeRef == GizmoMode::Scale, toolbarIconSizePx, scaleTooltip))
            {
                *this->gizmoModeRef = GizmoMode::Scale;
            }

            // Прямоугольник для блокировок следующего кадра
            this->toolbarMin = barMin;
            this->toolbarMax = barMax;
            this->toolbarRectValid = true;
        }

    } // namespace editor
} // namespace beng
