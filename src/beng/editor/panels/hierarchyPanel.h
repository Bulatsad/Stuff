#pragma once

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/graphics/skelet.h>

namespace beng
{
    namespace editor
    {
        /**
         * HierarchyPanel — иерархическое дерево костей скелета.
         *
         * Назначение:
         * - Показывает дерево костей (родитель/дети) через ImGui-узлы;
         * - Клик по узлу выбирает кость (getSelectedBone() — для
         *   подсветки в 3D-вьюпорте).
         *
         * Данные: не владеет скелетом — указатель выставляется
         * вызывающим при загрузке модели. При выгрузке модели
         * обязательно вызвать clearSkeleton()/setSkelet(nullptr):
         * выбранная кость иначе станет висячим указателем.
         */
        class __beng_api HierarchyPanel : public beng::editor::IPanel
        {
        private:
            const blib::graphics::Skelet* skelet;
            const blib::graphics::Bone* selectedBone;

            // Рекурсивная отрисовка ветки дерева костей
            void drawBoneNode(_In const blib::graphics::Bone* bone);

        public:
            HierarchyPanel();

            /**
             * Привязать скелет (nullptr допустим — панель покажет
             * заглушку «no skeleton»). Выбор сбрасывается.
             */
            void setSkelet(_In_opt const blib::graphics::Skelet* skel);

            /**
             * Выбранная кость (nullptr — ничего не выбрано).
             */
            const blib::graphics::Bone* getSelectedBone() const;

            void draw() override;
            const char* getName() const override { return "Hierarchy"; }
        };

    } // namespace editor
} // namespace beng
