#pragma once

#include <functional>
#include <string>

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/utilmacro.h>

namespace beng
{
    namespace editor
    {
        /**
         * DialogWindow — переиспользуемое модальное окно-диалог
         * (ImGui popup modal) для beng-editor.
         *
         * Назначение: вопрос «продолжить/отменить» (как диалоги при
         * исключениях): заголовок + сообщение + две кнопки
         * (подтверждение и отмена) с колбэками.
         *
         * Контракт:
         * - Как и у прочих панелей, вызывающий зовёт draw() каждый
         *   кадр внутри ImGui-кадра; пока диалог открыт — он
         *   блокирует ввод остальных окон (поведение ImGui-модалки);
         * - ОТСТУПЛЕНИЕ от IPanel: позицию диалог задаёт сам
         *   (центрирование по главному вьюпорту при появлении) —
         *   это popup, а не докируемая панель;
         * - ID попапа = заголовок: два открытых диалога обязаны
         *   иметь разные заголовки;
         * - Диалог не владеет данными, ради которых показан:
         *   действия выносятся в колбэки вызывающего;
         * - Колбэки вызываются синхронно в draw() (т.е. внутри
         *   главного цикла); внутри колбэка можно сразу открыть
         *   новый диалог (reentrancy учтена: состояние сбрасывается
         *   до вызова колбэка);
         * - Закрытие крестиком/по Escape эквивалентно отмене
         *   (вызов onCancel, если задан).
         */
        class __beng_api DialogWindow : public beng::editor::IPanel
        {
        private:
            // Запрос OpenPopup в ближайшем draw()
            bool openRequested;

            // Содержимое диалога (пустой заголовок = диалог закрыт)
            std::string title;
            std::string message;
            std::string confirmLabel;
            std::string cancelLabel;

            // Действия кнопок (пустой колбэк = кнопка просто закрывает)
            std::function<void()> onConfirm;
            std::function<void()> onCancel;

            // Общий финал кнопки/крестика: сброс состояния, затем колбэк
            void finishWithConfirm();
            void finishWithCancel();

        public:
            DialogWindow();

            DialogWindow(const DialogWindow&) = delete;
            DialogWindow& operator=(const DialogWindow&) = delete;

            // IPanel
            void draw() override;
            const char* getName() const override;

            /**
             * Показать диалог.
             *
             * @param title        Заголовок окна (ID попапа)
             * @param message      Текст сообщения (переносится по словам)
             * @param confirmLabel Подпись кнопки подтверждения
             * @param onConfirm    Действие кнопки подтверждения
             * @param onCancel     Действие отмены (крестик/Escape/кнопка);
             *                     nullptr — просто закрыть
             * @param cancelLabel  Подпись кнопки отмены; пустая строка —
             *                     дефолтная ("Cancel")
             */
            void open(
                _In const std::string& title,
                _In const std::string& message,
                _In const std::string& confirmLabel,
                _In std::function<void()> onConfirm,
                _In_opt std::function<void()> onCancel = nullptr,
                _In_opt const std::string& cancelLabel = std::string());

            /**
             * Закрыть диалог извне (без колбэков). Состояние сбрасывается
             * немедленно: следующий draw() пропустит попап, ImGui закроет
             * его в конце кадра.
             */
            void close();

            /**
             * Открыт ли диалог (по последнему состоянию).
             */
            bool isOpen() const;
        };

    } // namespace editor
} // namespace beng
