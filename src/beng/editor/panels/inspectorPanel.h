#pragma once

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/utilmacro.h>

namespace beng
{
    class Scene;
    class IComponent;
    class IComponentField;

    namespace editor
    {
        class SceneHierarchyPanel;

        /**
         * InspectorPanel — инспектор выбранной сущности эдитора.
         *
         * Назначение:
         * - Показывает компоненты выбранной сущности (источник выбора —
         *   SceneHierarchyPanel) и их поля через РЕФЛЕКСИЮ
         *   (ComponentTypeDescriptor из сцены) — эдитор не знает
         *   конкретных типов компонентов (см. ARCHITECTURE.md, «Эдитор»);
         * - Поля рисуются по FieldValue::Kind (числа, флаги, вектора);
         *   изменение применяется полем (IComponentField::setValue);
         * - Компоненты без рефлексии показываются только по имени типа.
         *
         * Данные: не владеет ни сценой, ни панелью иерархии — указатели
         * выставляются вызывающим; оба nullptr = заглушка.
         */
        class __beng_api InspectorPanel : public beng::editor::IPanel
        {
        private:
            Scene* scene;
            const SceneHierarchyPanel* hierarchyPanel;

            // Отрисовка одного поля выбранного компонента (по kind)
            void drawField(_In const IComponentField* field, _In IComponent& component);

        public:
            InspectorPanel();

            /**
             * Привязать сцену (nullptr допустим — панель покажет
             * заглушку).
             */
            void setScene(_In_opt Scene* scene);

            /**
             * Привязать панель иерархии — источник выбора сущности
             * (nullptr — выбора нет).
             */
            void setHierarchyPanel(_In_opt const SceneHierarchyPanel* panel);

            void draw() __blib_override;
            const char* getName() const __blib_override { return "Inspector"; }
        };

    } // namespace editor
} // namespace beng
