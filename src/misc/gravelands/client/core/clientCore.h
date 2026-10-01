#pragma once

#include <gravelands/client/core/gravelandsClientGame.h>
#include <gravelands/server/core/gravelandsServerGame.h>

#include <beng/client/clientApplication.h>

#include <blib/blibint.h>

namespace gravelands
{
    /**
     * ClientCore — клиентское ядро Gravelands: тонкая обёртка над
     * движковым beng::client::ClientApplication + игровой стороной
     * GravelandsClientGame.
     *
     * Оболочка (окно, рендер-таргет, изокамера, пост-пасс, ImGui,
     * сеть, интерполяция зеркал, client-side prediction игрока) —
     * движок (см. CLIENT.md); игра даёт мир, ввод, оверлей, визуал
     * зеркал, кодек команд и формулу интеграции ввода для предикшна.
     *
     * Local-server mode (одиночная игра, см. GRAVELANDS.md): при
     * инициализации ядро пробует поднять in-process сервер
     * (ClientApplication::startLocalServer + GravelandsServerGame);
     * порт занят (внешний/PIE-сервер уже слушает) — сервер молча
     * пропускается, клиент подключится к внешнему. Сетевой путь в
     * обоих случаях — настоящий loopback TCP.
     *
     * Паттерн «lib + тонкий exe»: ClientCore даёт frame-API
     * (initialize/tick/shutdown) и НЕ владеет главным циклом —
     * цикл крутит тонкий exe (main.cpp). Этим же API эдитор хостит
     * игру in-process (Play mode).
     */
    class ClientCore
    {
    public:
        ClientCore() = default;
        ~ClientCore() = default;

        // Ядро некопируемо и неперемещаемо (владеет графическими ресурсами)
        ClientCore(const ClientCore&) = delete;
        ClientCore& operator=(const ClientCore&) = delete;
        ClientCore(ClientCore&&) = delete;
        ClientCore& operator=(ClientCore&&) = delete;

        /**
         * Инициализировать ядро: окно, рендер-таргет, камеру, мир
         * (gravelands::World), сеть, ImGui + попытку local-server.
         * Вызывать один раз перед циклом.
         *
         * @param imguiEnabled Создавать ImGui-контекст/оверлей/консоль
         *        + собственное ОС-окно. false — режим PIE (headless):
         *        окно клиента НЕ создаётся (RenderWindow без контекста),
         *        клиент рендерит в свой FBO в GL-контексте эдитора;
         *        кадр показывает Game-панель эдитора (см. GRAVELANDS.md,
         *        «PIE»). Свой ImGui-контекст клиенту создавать нельзя —
         *        он сломал бы кадр эдитора; оверлей/консоль недоступны.
         * @return true при успехе (пока всегда, зарезервировано под будущие сбои)
         */
        bool initialize(bool imguiEnabled = true);

        /**
         * Идентификатор GL-текстуры текущего кадра клиента (цвет
         * FBO) — вход Game-панели эдитора (ImGui::Image). 0 — ядро
         * не инициализировано. Валиден внутри кадра: FBO переживает
         * кадры, но после shutdown() текстуры уничтожены.
         */
        buint64 getColorTextureId() const;

        /**
         * Один кадр: ввод, обновление, отрисовка.
         * Вызывается из цикла тонкого exe каждый кадр.
         */
        void tick();

        /**
         * Корректно остановить ядро и освободить ресурсы.
         */
        void shutdown();

        /**
         * Открыто ли окно (условие продолжения цикла в тонком exe).
         */
        bool isRunning() const;

    private:
        // Игры раньше ядра в списке членов — разрушаются ПОСЛЕ него
        // (ClientApplication держит указатели на IClientGame/IServerGame)
        GravelandsClientGame game;
        GravelandsServerGame localServerGame;
        beng::client::ClientApplication application;
    };

} // namespace gravelands
