#pragma once

#include <gravelands/plugin/pieSession.h>

#include <beng/config.h>
#include <beng/editor/panels/iPanel.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace gravelands
{
    /**
     * GamePanel — вкладка «Game» центральной области эдитора
     * (контракт ICenterTabView, рядом с вкладкой «Scene» каркаса):
     * показывает кадр PIE-клиента — кадр игры, как Game view в Unity.
     *
     * Кадр берётся из FBO headless-клиента PIE (getClientColorTextureId):
     * рендер идёт в GL-контексте эдитора в том же кадре, панель лишь
     * сэмплит текстуру через ImGui::Image (UV развёрнуты: FBO хранит
     * кадр вверх ногами, как во вьюпорте каркаса).
     *
     * Сессия не запущена (или панель без сессии) — заглушка
     * «не запущено»: текстуры клиента уничтожены его shutdown().
     *
     * Данные: не владеет сессией — только указатель (сессия живёт в
     * GravelandsEditorHost::impl и переживает панель).
     */
    class GamePanel : public beng::editor::ICenterTabView
    {
    private:
        PieSession* pieSession;

    public:
        explicit GamePanel(_In_opt PieSession* session = nullptr);

        /**
         * Привязать PIE-сессию (nullptr — панель рисует заглушку).
         */
        void setSession(_In_opt PieSession* session) { this->pieSession = session; }

        /**
         * Контент вкладки в текущее (центральное) окно ImGui: кадр
         * игры с сохранением пропорций (letterbox) или заглушка.
         */
        void drawContents() override;

        const char* getTabName() const override { return "Game"; }
    };

} // namespace gravelands
