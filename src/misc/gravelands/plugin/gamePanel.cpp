#include <gravelands/plugin/gamePanel.h>

#include <gravelands/common/config.h>

#include <imgui/imgui.h>

namespace gravelands
{
    namespace
    {
        // Заглушки: камеры в сцене нет (превью), сессия не запущена
        constexpr const char* noCameraMessage = "No active camera";
        constexpr const char* gameStoppedMessage = "Game view - press Play to start";
    }

    GamePanel::GamePanel(_In_opt PieSession* session)
        : pieSession(session)
        , previewTextureId(0)
        , hasCamera(false)
        , previewAspect(
            static_cast<float>(pieWindowWidth) / static_cast<float>(pieWindowHeight))
    {
    }

    void GamePanel::drawContents()
    {
        // Источник кадра: запущенная сессия — клиент, иначе — превью
        // из активной камеры сцены эдитора
        const bool running = (this->pieSession != nullptr) && this->pieSession->isRunning();
        const buint64 textureId = running
            ? this->pieSession->getClientColorTextureId()
            : (this->hasCamera ? this->previewTextureId : 0);

        const ImVec2 availableSize = ImGui::GetContentRegionAvail();
        if (textureId == 0)
        {
            // Превью без камеры — своя заглушка; «не запущено» остаётся
            // страховкой (у запущенной сессии текстура всегда есть)
            const char* message = (!running && !this->hasCamera) ? noCameraMessage : gameStoppedMessage;
            const ImVec2 textSize = ImGui::CalcTextSize(message);
            ImGui::SetCursorPos(ImVec2(
                ImGui::GetCursorPosX() + (availableSize.x - textSize.x) * 0.5f,
                ImGui::GetCursorPosY() + (availableSize.y - textSize.y) * 0.5f));
            ImGui::TextDisabled("%s", message);
            return;
        }

        // Пропорции кадра: клиент — фиксированный размер PIE-FBO
        // (окна у headless-клиента нет), превью — разрешение камеры
        const float frameAspect = running
            ? (static_cast<float>(pieWindowWidth) / static_cast<float>(pieWindowHeight))
            : this->previewAspect;
        const float panelAspect =
            (availableSize.y > 0.0f) ? (availableSize.x / availableSize.y) : frameAspect;

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

        // Центрируем изображение внутри вкладки
        const ImVec2 imagePos(
            ImGui::GetCursorScreenPos().x + (availableSize.x - imageSize.x) * 0.5f,
            ImGui::GetCursorScreenPos().y + (availableSize.y - imageSize.y) * 0.5f);
        ImGui::SetCursorScreenPos(imagePos);

        // FBO хранит кадр вверх ногами (GL-координаты) — UV развёрнуты
        // (как во ViewportPanel каркаса); ImTextureID в ImGui 1.92 —
        // ImU64: GLuint кладём значением. GPU-фильтрация даёт
        // downscale кадра камеры до размера вкладки
        ImGui::Image(
            static_cast<ImTextureID>(textureId),
            imageSize,
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f));
    }

} // namespace gravelands
