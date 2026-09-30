#pragma once

#include <beng/config.h>

namespace beng
{
    namespace editor
    {
        /**
         * IPanel — базовый интерфейс панели эдитора.
         *
         * Контракт:
         * - Панель рисует СВОЁ ImGui-окно внутри ImGui-кадра
         *   (вызывающий готовит NewFrame, зовёт draw(), затем
         *   рендерит draw data);
         * - Позиция/размер окна — ответственность вызывающего
         *   (SetNextWindowPos/Size перед draw());
         * - Панель не владеет данными, которые показывает, —
         *   получает их через set*() и хранит указатели.
         *
         * В будущем на этом интерфейсе вырастет докинг-система
         * beng-editor: панели регистрируются в EditorApplication,
         * который раскладывает их по зонам.
         */
        class __beng_api IPanel
        {
        public:
            virtual ~IPanel() = default;

            /**
             * Отрисовать панель (вызывается каждый кадр внутри
             * ImGui-кадра).
             */
            virtual void draw() __blib_pure_virtual_function;

            /**
             * Имя панели (заголовок окна, debug-вывод).
             */
            virtual const char* getName() const __blib_pure_virtual_function;
        };

        /**
         * ICenterTabView — вкладка центральной области эдитора (как
         * Scene/Game в Unity).
         *
         * В отличие от IPanel, вкладка НЕ рисует собственное окно: её
         * контент каркас рисует внутрь общего центрального окна с
         * таб-баром (первая вкладка — всегда вьюпорт «Scene»).
         *
         * Контракт:
         * - drawContents() вызывается внутри BeginTabItem/EndTabItem
         *   уже активной вкладки (текущее окно — центральное);
         * - каркас не владеет вкладкой: она обязана жить, пока
         *   зарегистрирована (указатель);
         * - getTabName() — стабильная строка (id вкладки в таб-баре).
         */
        class __beng_api ICenterTabView
        {
        public:
            virtual ~ICenterTabView() = default;

            /**
             * Отрисовать контент вкладки в текущее (центральное)
             * окно ImGui.
             */
            virtual void drawContents() __blib_pure_virtual_function;

            /**
             * Имя вкладки в таб-баре центральной области.
             */
            virtual const char* getTabName() const __blib_pure_virtual_function;
        };

    } // namespace editor
} // namespace beng
