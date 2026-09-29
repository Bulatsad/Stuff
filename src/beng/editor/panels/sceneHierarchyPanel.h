#pragma once

#include <beng/config.h>
#include <beng/editor/editorIcons.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/utilmacro.h>

namespace beng
{
    class Scene;

    namespace editor
    {
        class CommandHistory;
        /**
         * SceneHierarchyPanel — иерархия сущностей сцены эдитора.
         *
         * Назначение:
         * - Показывает список сущностей сцены (EntityID) с вложенным
         *   списком их компонентов (по стабильным именам типов);
         * - Иконка сущности — по приоритетному компоненту (маппинг
         *   editorIcons.h; неизвестные игровые типы — куб-фолбэк);
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
            // История команд (каркас): кнопки Create/Delete пишут
            // создание/удаление сущностей (nullptr — кнопки скрыты)
            CommandHistory* commandHistory;
            // Иконочный шрифт каркаса (nullptr — иконок нет)
            ImFont* iconFont;

            // Иконка сущности по приоритетному компоненту (type-erased:
            // перебор компонентов + маппинг имён editorIcons.h)
            ComponentIcon resolveEntityIcon(EntityID entityId) const;

            // Записать выбор (в хранилище каркаса или локально)
            void selectEntity(EntityID entityId);

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
             * Привязать историю команд (каркас): включает кнопки
             * Create/Delete сущностей с записью в undo/redo.
             * nullptr — кнопки скрыты.
             */
            void setCommandHistory(_In_opt CommandHistory* history);

            /**
             * Привязать иконочный шрифт каркаса (nullptr — иконки
             * не рисуются).
             */
            void setIconFont(_In_opt ImFont* font);

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
