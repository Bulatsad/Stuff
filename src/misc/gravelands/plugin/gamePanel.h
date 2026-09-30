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
     * показывает кадр игры, как Game view в Unity.
     *
     * Два источника кадра:
     * - PIE-сессия запущена — кадр headless-клиента
     *   (PieSession::getClientColorTextureId);
     * - сессия не запущена — превью сцены эдитора из активной
     *   камеры (beng.Camera): хост рендерит сцену в свой FBO и
     *   передаёт текстуру через setPreviewTexture (downscale до
     *   размера вкладки — GPU-фильтрация ImGui::Image).
     *
     * Текстуры рендерятся в GL-контексте эдитора в том же кадре,
     * панель лишь сэмплит их через ImGui::Image (UV развёрнуты:
     * FBO хранит кадр вверх ногами, как во вьюпорте каркаса).
     *
     * Сессия не запущена и активной камеры нет — заглушка «No
     * active camera» (текстуры превью уничтожены shutdown'ом хоста).
     *
     * Данные: не владеет сессией — только указатель (сессия живёт в
     * GravelandsEditorHost::impl и переживает панель).
     */
    class GamePanel : public beng::editor::ICenterTabView
    {
    private:
        PieSession* pieSession;

        // Состояние превью из активной камеры (заполняет хост в
        // onSceneDidUpdate): текстура валидна только пока hasCamera
        // (GL-id принадлежит FBO хоста и переживает кадры)
        buint64 previewTextureId;
        bool hasCamera;
        float previewAspect;

    public:
        explicit GamePanel(_In_opt PieSession* session = nullptr);

        /**
         * Привязать PIE-сессию (nullptr — панель рисует заглушку).
         */
        void setSession(_In_opt PieSession* session) { this->pieSession = session; }

        /**
         * Кадр превью из активной камеры сцены эдитора (сессия не
         * запущена). textureId == 0 / hasCamera == false — камеры нет:
         * панель покажет заглушку. aspect — ширина/высота кадра камеры
         * (letterbox).
         */
        void setPreviewTexture(buint64 textureId, bool hasCamera, float aspect)
        {
            this->previewTextureId = textureId;
            this->hasCamera = hasCamera;
            this->previewAspect = aspect;
        }

        /**
         * Контент вкладки в текущее (центральное) окно ImGui: кадр
         * игры с сохранением пропорций (letterbox) или заглушка.
         */
        void drawContents() override;

        const char* getTabName() const override { return "Game"; }
    };

} // namespace gravelands
