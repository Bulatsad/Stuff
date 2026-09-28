#pragma once

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/utilmacro.h>

namespace beng
{
    class Scene;

    namespace editor
    {
        /**
         * SceneHierarchyPanel — иерархия сущностей сцены эдитора.
         *
         * Назначение:
         * - Показывает список сущностей сцены (EntityID) с вложенным
         *   списком их компонентов (по стабильным именам типов);
         * - Клик по сущности выбирает её (getSelectedEntity() — источник
         *   выбора для InspectorPanel и будущего gizmo);
         * - Работает type-erased (Scene::getEntityId/hasComponent) —
         *   не знает конкретных типов компонентов (см. ARCHITECTURE.md,
         *   «Эдитор»).
         *
         * Данные: не владеет сценой — указатель выставляется вызывающим
         * (setScene); при сбросе сцены выбор сбрасывается. Устаревший
         * выбор (сущность удалена/сцена перезагружена) сбрасывается в
         * draw() по живому Transform (инвариант сцены: у любой сущности
         * есть Transform — typeId 0).
         */
        class __beng_api SceneHierarchyPanel : public beng::editor::IPanel
        {
        private:
            Scene* scene;
            EntityID selectedEntity;

        public:
            SceneHierarchyPanel();

            /**
             * Привязать сцену (nullptr допустим — панель покажет
             * заглушку). Выбор сбрасывается.
             */
            void setScene(_In_opt Scene* scene);

            /**
             * Выбранная сущность (invalidEntity — ничего не выбрано).
             */
            EntityID getSelectedEntity() const { return this->selectedEntity; }

            void draw() __blib_override;
            const char* getName() const __blib_override { return "Scene Hierarchy"; }
        };

    } // namespace editor
} // namespace beng
