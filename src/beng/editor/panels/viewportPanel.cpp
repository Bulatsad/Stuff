#include <beng/editor/panels/viewportPanel.h>

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
        }

        ViewportPanel::ViewportPanel()
            : renderTarget(nullptr)
            , camera(nullptr)
            , rotating(false)
            , panning(false)
            , lastViewportWidth(0.0f)
            , lastViewportHeight(0.0f)
        {
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

        void ViewportPanel::handleCameraInput()
        {
            if (__blib_unlikely(!this->camera))
            {
                return;
            }

            ImGuiIO& io = ImGui::GetIO();

            // Захват драга: только если нажатие произошло на самом
            // изображении (а не на заголовке/в другом окне).
            // IsItemClicked валиден — изображение всё ещё текущий item
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                this->rotating = true;
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            {
                this->panning = true;
            }

            // ЛКМ-драг (начатый на изображении) — вращение вокруг цели.
            // Экранная ось Y растёт вниз, поэтому вертикальный угол
            // инвертирован: движение мыши вверх (dy < 0) поднимает камеру
            if (this->rotating && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
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

            // Ввод камеры — только пока курсор над изображением
            if (ImGui::IsItemHovered())
            {
                this->handleCameraInput();
            }

            ImGui::End();
        }

    } // namespace editor
} // namespace beng
