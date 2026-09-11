#include <beng/editor/panels/renderOptionsPanel.h>

#include <imgui/imgui.h>

namespace beng
{
    namespace editor
    {
        namespace
        {
            constexpr const char* panelTitle = "Render Options";
            constexpr const char* skeletonCheckboxLabel = "Show Skeleton";
            constexpr const char* textureCheckboxLabel = "Diffuse Texture";
            constexpr const char* wireframeCheckboxLabel = "Wireframe";
        }

        RenderOptionsPanel::RenderOptionsPanel()
            : showSkeleton(false)
            , showDiffuseTexture(true)
            , showWireframe(false)
        {
        }

        void RenderOptionsPanel::draw()
        {
            ImGui::Begin(panelTitle);

            ImGui::Checkbox(skeletonCheckboxLabel, &this->showSkeleton);
            ImGui::Checkbox(textureCheckboxLabel, &this->showDiffuseTexture);
            ImGui::Checkbox(wireframeCheckboxLabel, &this->showWireframe);

            ImGui::End();
        }

        bool RenderOptionsPanel::isSkeletonVisible() const
        {
            return this->showSkeleton;
        }

        bool RenderOptionsPanel::isDiffuseTextureVisible() const
        {
            return this->showDiffuseTexture;
        }

        bool RenderOptionsPanel::isWireframeVisible() const
        {
            return this->showWireframe;
        }

    } // namespace editor
} // namespace beng
