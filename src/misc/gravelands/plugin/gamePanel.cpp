#include <gravelands/plugin/gamePanel.h>

#include <gravelands/common/config.h>

#include <imgui/imgui.h>

namespace gravelands
{
    namespace
    {
        // Заглушка, когда сессии нет или она не запущена
        constexpr const char* gameStoppedMessage = "Game view - press Play to start";
    }

    GamePanel::GamePanel(_In_opt PieSession* session)
        : pieSession(session)
    {
    }

    void GamePanel::drawContents()
    {
        const buint64 textureId =
            (this->pieSession != nullptr) ? this->pieSession->getClientColorTextureId() : 0;

        // Сессии нет / не запущена / клиент не инициализирован —
        // заглушка по центру области (текстуры FBO уничтожены
        // shutdown'ом клиента)
        const ImVec2 availableSize = ImGui::GetContentRegionAvail();
        if (textureId == 0)
        {
            const ImVec2 textSize = ImGui::CalcTextSize(gameStoppedMessage);
            ImGui::SetCursorPos(ImVec2(
                ImGui::GetCursorPosX() + (availableSize.x - textSize.x) * 0.5f,
                ImGui::GetCursorPosY() + (availableSize.y - textSize.y) * 0.5f));
            ImGui::TextDisabled("%s", gameStoppedMessage);
            return;
        }

        // Пропорции кадра — фиксированный размер PIE-FBO (окна у
        // headless-клиента нет): letterbox как во вкладке «Scene»
        constexpr float frameAspect =
            static_cast<float>(pieWindowWidth) / static_cast<float>(pieWindowHeight);
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
        // ImU64: GLuint кладём значением
        ImGui::Image(
            static_cast<ImTextureID>(textureId),
            imageSize,
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f));
    }

} // namespace gravelands
