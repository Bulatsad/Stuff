#include <beng/editor/panels/sceneHierarchyPanel.h>

#include <beng/core/scene.h>

#include <imgui/imgui.h>

namespace beng
{
    namespace editor
    {
        namespace
        {
            constexpr const char* panelTitle = "Scene Hierarchy";
            constexpr const char* noSceneMessage = "No scene";

            // Transform — typeId 0 (инвариант сцены): проверка живости
            // выбранной сущности (Transform есть у любой живой)
            constexpr ComponentType transformTypeId = 0;

            // Префикс строки сущности в дереве
            constexpr const char* entityLabelPrefix = "Entity ";

            // Флаги узла сущности: стрелка раскрытия + подсветка выбора
            constexpr ImGuiTreeNodeFlags entityNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;

            // Тип-заглушка для неназванного типа (не должно случаться)
            constexpr const char* unknownTypeName = "?";
        }

        SceneHierarchyPanel::SceneHierarchyPanel()
            : scene(nullptr)
            , selectedEntity(invalidEntity)
        {
        }

        void SceneHierarchyPanel::setScene(_In_opt Scene* scene)
        {
            this->scene = scene;
            // Сцена сменилась — выбор из старой сцены невалиден
            this->selectedEntity = invalidEntity;
        }

        void SceneHierarchyPanel::draw()
        {
            ImGui::Begin(panelTitle);

            if (__blib_unlikely(this->scene == nullptr))
            {
                ImGui::TextDisabled(noSceneMessage);
                ImGui::End();
                return;
            }

            // Выбор протух (сущность удалена/сцена перезагружена) —
            // сбросить: у живой сущности всегда есть Transform (typeId 0)
            if (this->selectedEntity != invalidEntity &&
                this->scene->tryGetComponent(this->selectedEntity, transformTypeId) == nullptr)
            {
                this->selectedEntity = invalidEntity;
            }

            // Плоский список сущностей (иерархия Transform — позже):
            // узел сущности + вложенные имена её компонентов
            const buint32 entityCount = this->scene->getEntityCount();
            for (buint32 i = 0; i < entityCount; ++i)
            {
                const EntityID id = this->scene->getEntityId(i);
                if (__blib_unlikely(id == invalidEntity))
                {
                    continue;
                }

                const bool isSelected = (id == this->selectedEntity);

                // ID ImGui: EntityID (buint64) сужается до ImGuiID —
                // ID сцен последовательны и в жизни процесса не
                // выходят за диапазон ImGuiID
                ImGui::PushID(static_cast<ImGuiID>(id));

                ImGuiTreeNodeFlags flags = entityNodeFlags;
                if (isSelected)
                {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                const bool nodeOpen = ImGui::TreeNodeEx(
                    "##entity", flags, "%s%llu", entityLabelPrefix,
                    static_cast<unsigned long long>(id));
                if (ImGui::IsItemClicked())
                {
                    this->selectedEntity = id;
                }

                if (nodeOpen)
                {
                    const buint32 typeCount = this->scene->getComponentTypeCount();
                    for (buint32 t = 0; t < typeCount; ++t)
                    {
                        const ComponentType typeId = static_cast<ComponentType>(t);
                        if (!this->scene->hasComponent(id, typeId))
                        {
                            continue;
                        }

                        const char* typeName = this->scene->getComponentTypeName(typeId);
                        ImGui::BulletText("%s", typeName != nullptr ? typeName : unknownTypeName);
                    }
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            ImGui::End();
        }

    } // namespace editor
} // namespace beng
