#pragma once

#include <beng/config.h>
#include <beng/editor/editorIcons.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/utilmacro.h>

namespace beng
{
    class Scene;
    class IComponent;
    class IComponentField;

    namespace editor
    {
        class CommandHistory;
        /**
         * InspectorPanel — инспектор выбранной сущности эдитора.
         *
         * Назначение:
         * - Показывает компоненты выбранной сущности и их поля через
         *   РЕФЛЕКСИЮ (ComponentTypeDescriptor из сцены) — эдитор не
         *   знает конкретных типов компонентов (см. ARCHITECTURE.md,
         *   «Эдитор»);
         * - Поля рисуются по FieldValue::Kind (числа, флаги, вектора);
         *   изменение применяется полем (IComponentField::setValue);
         * - Компоненты без рефлексии показываются только по имени типа;
         * - Иконки заголовков компонентов — по маппингу editorIcons.h
         *   (неизвестные игровые типы — без иконки).
         *
         * Данные: не владеет ни сценой, ни выбором — указатели
         * выставляются вызывающим; nullptr = заглушка. Выбор читается
         * из хранилища каркаса (selection эдитора): панель — читатель.
         */
        class __beng_api InspectorPanel : public beng::editor::IPanel
        {
        private:
            Scene* scene;
            // Источник выбора (поле selectedEntity каркаса; панель НЕ
            // владеет). nullptr — выбора нет (заглушка)
            const EntityID* selectionSource;
            // История команд (каркас): правки полей, удаление/
            // добавление компонентов пишутся в undo/redo (nullptr —
            // правки без истории)
            CommandHistory* commandHistory;
            // Иконочный шрифт каркаса (nullptr — иконок нет)
            ImFont* iconFont;

            // Отрисовка одного поля выбранного компонента (по kind);
            // изменение применяется полю и пишется в историю команд
            void drawField(
                _In const IComponentField* field, _In IComponent& component,
                EntityID entityId, ComponentType typeId, buint32 fieldIndex);

        public:
            InspectorPanel();

            /**
             * Привязать сцену (nullptr допустим — панель покажет
             * заглушку).
             */
            void setScene(_In_opt Scene* scene);

            /**
             * Привязать источник выбора сущности — поле selection
             * каркаса (nullptr — выбора нет).
             */
            void setSelectionSource(_In_opt const EntityID* selectedEntity);

            /**
             * Привязать историю команд (каркас): правки полей и
             * добавление/удаление компонентов пишутся в undo/redo.
             * nullptr — правки без истории.
             */
            void setCommandHistory(_In_opt CommandHistory* history);

            /**
             * Привязать иконочный шрифт каркаса (nullptr — иконки
             * не рисуются).
             */
            void setIconFont(_In_opt ImFont* font);

            void draw() __blib_override;
            const char* getName() const __blib_override { return "Inspector"; }
        };

    } // namespace editor
} // namespace beng
