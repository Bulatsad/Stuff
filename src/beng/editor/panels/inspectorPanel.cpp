#include <beng/editor/panels/inspectorPanel.h>

#include <beng/core/componentReflection.h>
#include <beng/core/scene.h>
#include <beng/editor/panels/sceneHierarchyPanel.h>

#include <imgui/imgui.h>

namespace beng
{
    namespace editor
    {
        namespace
        {
            constexpr const char* panelTitle = "Inspector";
            constexpr const char* noSceneMessage = "No scene";
            constexpr const char* noSelectionMessage = "No entity selected";

            // Префикс строки сущности в заголовке инспектора
            constexpr const char* entityLabelPrefix = "Entity ";

            // Скорости драгов числовых полей (по вкусу редактора)
            constexpr float floatDragSpeed = 0.05f;
            constexpr float intDragSpeed = 1.0f;
            constexpr float vectorDragSpeed = 0.05f;

            // Заголовок-заглушка для типа без имени (не должно случаться)
            constexpr const char* unknownTypeName = "?";
        }

        InspectorPanel::InspectorPanel()
            : scene(nullptr)
            , hierarchyPanel(nullptr)
        {
        }

        void InspectorPanel::setScene(_In_opt Scene* scene)
        {
            this->scene = scene;
        }

        void InspectorPanel::setHierarchyPanel(_In_opt const SceneHierarchyPanel* panel)
        {
            this->hierarchyPanel = panel;
        }

        void InspectorPanel::drawField(_In const IComponentField* field, _In IComponent& component)
        {
            // Чтение текущего значения поля (тип значения — kind)
            FieldValue value;
            field->getValue(component, value);

            bool changed = false;
            switch (value.kind)
            {
                case FieldValue::Kind::Float:
                    changed = ImGui::DragFloat(field->getName(), &value.floatValue, floatDragSpeed);
                    break;
                case FieldValue::Kind::Int:
                    changed = ImGui::DragInt(field->getName(), &value.intValue, intDragSpeed);
                    break;
                case FieldValue::Kind::Bool:
                    changed = ImGui::Checkbox(field->getName(), &value.boolValue);
                    break;
                case FieldValue::Kind::Vector3:
                {
                    // ImGui::DragFloat3 работает с float[3]; Vector<> даёт
                    // доступ по индексу (operator[])
                    float v[3] = {
                        value.vector3Value[0],
                        value.vector3Value[1],
                        value.vector3Value[2]
                    };
                    changed = ImGui::DragFloat3(field->getName(), v, vectorDragSpeed);
                    if (changed)
                    {
                        value.vector3Value = blib::math::Vector<float, 3>(v[0], v[1], v[2]);
                    }
                    break;
                }
                default:
                    // Неизвестный тип значения — показываем только имя
                    // (редактирование не предусмотрено)
                    ImGui::TextUnformatted(field->getName());
                    break;
            }

            // Применение правки полем (контракт: kind совпадает с getKind)
            if (changed)
            {
                field->setValue(component, value);
            }
        }

        void InspectorPanel::draw()
        {
            ImGui::Begin(panelTitle);

            if (__blib_unlikely(this->scene == nullptr))
            {
                ImGui::TextDisabled(noSceneMessage);
                ImGui::End();
                return;
            }

            // Источник выбора — панель иерархии (связка панелей: выбор
            // живёт в SceneHierarchyPanel, пока нет selection-сервиса
            // каркаса)
            const EntityID selectedEntity =
                (this->hierarchyPanel != nullptr) ? this->hierarchyPanel->getSelectedEntity() : invalidEntity;

            if (__blib_unlikely(selectedEntity == invalidEntity))
            {
                ImGui::TextDisabled(noSelectionMessage);
                ImGui::End();
                return;
            }

            ImGui::Text("%s%llu", entityLabelPrefix, static_cast<unsigned long long>(selectedEntity));
            ImGui::Separator();

            // Компоненты выбранной сущности: по всем зарегистрированным
            // типам сцены (type-erased — см. Scene Reflection API)
            const buint32 typeCount = this->scene->getComponentTypeCount();
            for (buint32 t = 0; t < typeCount; ++t)
            {
                const ComponentType typeId = static_cast<ComponentType>(t);
                if (!this->scene->hasComponent(selectedEntity, typeId))
                {
                    continue;
                }

                const char* typeName = this->scene->getComponentTypeName(typeId);
                IComponent* component = this->scene->tryGetComponent(selectedEntity, typeId);
                if (__blib_unlikely(component == nullptr))
                {
                    continue;
                }

                // Секция компонента: заголовок = стабильное имя типа;
                // поля — только если у типа есть рефлексия
                const ComponentTypeDescriptor* descriptor = this->scene->tryGetComponentReflection(typeId);
                if (descriptor == nullptr || descriptor->getFieldCount() == 0)
                {
                    ImGui::TextUnformatted(typeName != nullptr ? typeName : unknownTypeName);
                    continue;
                }

                const bool headerOpen = ImGui::CollapsingHeader(
                    typeName != nullptr ? typeName : unknownTypeName,
                    ImGuiTreeNodeFlags_DefaultOpen);
                if (headerOpen)
                {
                    for (buint32 f = 0; f < descriptor->getFieldCount(); ++f)
                    {
                        const IComponentField* field = descriptor->getField(f);
                        if (__blib_unlikely(field == nullptr))
                        {
                            continue;
                        }
                        this->drawField(field, *component);
                    }
                }
            }

            ImGui::End();
        }

    } // namespace editor
} // namespace beng
