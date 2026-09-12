#include <beng/editor/panels/dialogWindow.h>

#include <cfloat>

#include <imgui/imgui.h>

namespace beng
{
    namespace editor
    {
        namespace
        {
            // Дефолтная подпись кнопки отмены (если вызывающий не задал)
            constexpr const char* defaultCancelLabel = "Cancel";

            // Ширина кнопок диалога (обе кнопки одного размера)
            constexpr float dialogButtonWidth = 120.0f;

            // Минимальная ширина попапа. TextWrapped в окне с
            // AlwaysAutoResize без ограничения ширины схлопывает окно
            // в узкий столбец (текст переносится по слову, кнопки
            // уезжают за край) — feedback между переносом текста и
            // автоподгонкой размера. Кнопки занимают ~280 px (2 x 120
            // + отступ), остальное — запас на поля
            constexpr float dialogMinWidth = 360.0f;

            // Невидимое окно-хост попапа (см. draw()): OpenPopup и
            // BeginPopupModal читают g.CurrentWindow (ID-стек), поэтому
            // вне окон вызываться не могут. Имя с "##" — невидимо в
            // списках и не конфликтует с пользовательскими окнами
            constexpr const char* dialogHostWindowName = "##DialogWindowPopupHost";

            // Флаги окна-хоста: без декораций/фона/сохранения — окно
            // только держит ID-контекст и не участвует в вводе
            constexpr ImGuiWindowFlags dialogHostWindowFlags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoInputs;

            // Позиция окна-хоста далеко за пределами экрана (оно
            // ничего не рисует, но обязано не перехватывать курсор)
            constexpr float dialogHostWindowPosition = 10000.0f;
        }

        DialogWindow::DialogWindow()
            : openRequested(false)
            , title()
            , message()
            , confirmLabel()
            , cancelLabel()
            , onConfirm()
            , onCancel()
        {
        }

        void DialogWindow::open(
            _In const std::string& title_,
            _In const std::string& message_,
            _In const std::string& confirmLabel_,
            _In std::function<void()> onConfirm_,
            _In_opt std::function<void()> onCancel_,
            _In_opt const std::string& cancelLabel_)
        {
            // Переоткрытие поверх открытого диалога: состояние просто
            // заменяется, старые колбэки не вызываются
            this->title = title_;
            this->message = message_;
            this->confirmLabel = confirmLabel_;
            this->cancelLabel = cancelLabel_.empty() ? defaultCancelLabel : cancelLabel_;
            this->onConfirm = std::move(onConfirm_);
            this->onCancel = std::move(onCancel_);
            this->openRequested = true;
        }

        void DialogWindow::close()
        {
            // Немедленный сброс состояния без колбэков. Следующий
            // draw() пропустит BeginPopupModal (заголовок пуст), а
            // ImGui сам закроет осиротевший попап в конце кадра —
            // CloseCurrentPopup здесь не нужен (он валиден только
            // внутри scope попапа)
            this->title.clear();
            this->message.clear();
            this->confirmLabel.clear();
            this->cancelLabel.clear();
            this->onConfirm = nullptr;
            this->onCancel = nullptr;
            this->openRequested = false;
        }

        bool DialogWindow::isOpen() const
        {
            return !this->title.empty();
        }

        const char* DialogWindow::getName() const
        {
            return this->title.c_str();
        }

        void DialogWindow::draw()
        {
            if (this->title.empty())
            {
                return;
            }

            // ImGui-попапы требуют живого parent-окна: OpenPopup и
            // BeginPopupModal читают g.CurrentWindow (ID-стек) и на
            // корневом уровне (вне окон) это неопределённое поведение.
            // Диалог вызывается именно вне окон (между панелей),
            // поэтому весь цикл жизни попапа хостится в невидимом
            // окне-без-декораций
            ImGui::SetNextWindowPos(ImVec2(dialogHostWindowPosition, dialogHostWindowPosition), ImGuiCond_Always);
            ImGui::Begin(dialogHostWindowName, nullptr, dialogHostWindowFlags);

            // Запрос открытия обрабатывается до BeginPopupModal того же
            // кадра — канонический паттерн ImGui
            if (this->openRequested)
            {
                ImGui::OpenPopup(this->title.c_str());
                this->openRequested = false;
            }

            // Центрирование по главному вьюпорту при появлении
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

            // Минимальная ширина — защита от схлопывания AutoResize-окна
            // с TextWrapped (см. комментарий к dialogMinWidth)
            ImGui::SetNextWindowSizeConstraints(ImVec2(dialogMinWidth, 0.0f), ImVec2(FLT_MAX, FLT_MAX));

            bool popupOpen = true;
            if (ImGui::BeginPopupModal(this->title.c_str(), &popupOpen, ImGuiWindowFlags_AlwaysAutoResize))
            {
                // ВАЖНО: после каждого закрывающего действия контент
                // НЕ рисуется (флаг finished). finishWith* очищает
                // строки контента, и их отрисовка дала бы
                // TextWrapped("")/Button("") в корне окна — пустой
                // label даёт id == window->ID и IM_ASSERT в Debug-сборке
                // ImGui (регрессия покрыта тестами группы dialogWindow)
                bool finished = false;

                // Крестик в заголовке: закрытие без подтверждения = отмена
                if (!popupOpen)
                {
                    this->finishWithCancel();
                    finished = true;
                }
                // Escape внутри модалки = отмена (как в системных диалогах)
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                {
                    this->finishWithCancel();
                    finished = true;
                }

                if (!finished)
                {
                    ImGui::TextWrapped("%s", this->message.c_str());

                    // Кнопки в правом нижнем углу: подтверждение левее отмены
                    ImGui::Spacing();
                    const ImGuiStyle& style = ImGui::GetStyle();
                    const float buttonsAreaWidth = dialogButtonWidth * 2.0f + style.ItemSpacing.x;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonsAreaWidth);

                    if (ImGui::Button(this->confirmLabel.c_str(), ImVec2(dialogButtonWidth, 0.0f)))
                    {
                        this->finishWithConfirm();
                        finished = true;
                    }
                    if (!finished)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button(this->cancelLabel.c_str(), ImVec2(dialogButtonWidth, 0.0f)))
                        {
                            this->finishWithCancel();
                            finished = true;
                        }
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::End();
        }

        void DialogWindow::finishWithConfirm()
        {
            // Порядок важен для reentrancy: состояние сбрасывается ДО
            // вызова колбэка, чтобы колбэк мог сразу открыть новый
            // диалог (open() перезапишет чистое состояние)
            std::function<void()> callback = std::move(this->onConfirm);
            this->title.clear();
            this->message.clear();
            this->confirmLabel.clear();
            this->cancelLabel.clear();
            this->onCancel = nullptr;
            this->openRequested = false;
            ImGui::CloseCurrentPopup();

            if (callback)
            {
                callback();
            }
        }

        void DialogWindow::finishWithCancel()
        {
            std::function<void()> callback = std::move(this->onCancel);
            this->title.clear();
            this->message.clear();
            this->confirmLabel.clear();
            this->cancelLabel.clear();
            this->onConfirm = nullptr;
            this->openRequested = false;
            ImGui::CloseCurrentPopup();

            if (callback)
            {
                callback();
            }
        }

    } // namespace editor
} // namespace beng
