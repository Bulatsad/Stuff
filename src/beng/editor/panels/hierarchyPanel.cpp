#include <beng/editor/panels/hierarchyPanel.h>

#include <imgui/imgui.h>

#include <blib/graphics/bone.h>
#include <blib/graphics/hierarchal.h>

namespace beng
{
    namespace editor
    {
        namespace
        {
            constexpr const char* panelTitle = "Hierarchy";
            constexpr const char* noSkeletonMessage = "No skeleton";
        }

        HierarchyPanel::HierarchyPanel()
            : skelet(nullptr)
            , selectedBone(nullptr)
        {
        }

        void HierarchyPanel::setSkelet(_In_opt const blib::graphics::Skelet* skel)
        {
            this->skelet = skel;
            // Старый указатель кости мог умереть вместе со скелетом
            this->selectedBone = nullptr;
        }

        const blib::graphics::Bone* HierarchyPanel::getSelectedBone() const
        {
            return this->selectedBone;
        }

        void HierarchyPanel::drawBoneNode(_In const blib::graphics::Bone* bone)
        {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
            if (bone->getChilds().empty())
            {
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }
            if (bone == this->selectedBone)
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            const bool nodeOpen = ImGui::TreeNodeEx(bone->name.c_str(), flags);
            if (ImGui::IsItemClicked())
            {
                this->selectedBone = bone;
            }

            if (nodeOpen)
            {
                for (const blib::graphics::IHierarchal* child : bone->getChilds())
                {
                    this->drawBoneNode(static_cast<const blib::graphics::Bone*>(child));
                }
                if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
                {
                    ImGui::TreePop();
                }
            }
        }

        void HierarchyPanel::draw()
        {
            ImGui::Begin(panelTitle);

            if (__blib_unlikely(!this->skelet || !this->skelet->root))
            {
                ImGui::TextDisabled(noSkeletonMessage);
                ImGui::End();
                return;
            }

            this->drawBoneNode(this->skelet->root);

            ImGui::End();
        }

    } // namespace editor
} // namespace beng
