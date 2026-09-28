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
         * - Клик по сущности выбирает её — источник выбора эдитора
         *   (selection живёт в каркасе EditorApplication: панель пишет
         *   в его поле через setSelectionRef);
         * - Работает type-erased (Scene::getEntityId/hasComponent) —
         *   не знает конкретных типов компонентов (см. ARCHITECTURE.md,
         *   «Эдитор»).
         *
         * Данные: не владеет сценой и выбором — указатели выставляются
         * вызывающим (setScene/setSelectionRef); nullptr = заглушка/
         * локальный фолбэк. Устаревший выбор (сущность удалена/сцена
         * перезагружена) сбрасывается в draw() по живому Transform
         * (инвариант сцены: у любой сущности есть Transform — typeId 0).
         */
        class __beng_api SceneHierarchyPanel : public beng::editor::IPanel
        {
        private:
            Scene* scene;
            // Хранилище выбора (поле каркаса; панель НЕ владеет).
            // nullptr — панель хранит выбор локально (автономный режим)
            EntityID* selectionRef;
            // Локальный фолбэк выбора (без selectionRef)
            EntityID localSelection;

        public:
            SceneHierarchyPanel();

            /**
             * Привязать сцену (nullptr допустим — панель покажет
             * заглушку). Выбор сбрасывается.
             */
            void setScene(_In_opt Scene* scene);

            /**
             * Привязать хранилище выбора (поле selectedEntity каркаса):
             * клик пишет туда, подсветка читает оттуда. nullptr —
             * автономный режим (локальный выбор панели).
             */
            void setSelectionRef(_In_opt EntityID* ref);

            /**
             * Выбранная сущность (invalidEntity — ничего не выбрано).
             * Эффективный выбор: selectionRef ?? локальный.
             */
            EntityID getSelectedEntity() const;

            void draw() __blib_override;
            const char* getName() const __blib_override { return "Scene Hierarchy"; }
        };

    } // namespace editor
} // namespace beng
