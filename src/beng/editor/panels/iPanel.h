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
            virtual void draw() = 0;

            /**
             * Имя панели (заголовок окна, debug-вывод).
             */
            virtual const char* getName() const = 0;
        };

    } // namespace editor
} // namespace beng
