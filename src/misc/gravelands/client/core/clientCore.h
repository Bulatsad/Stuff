#pragma once

#include <gravelands/common/config.h>

#include <beng/config.h>

#include <blib/utilmacro.h>

namespace gravelands
{
    /**
     * ClientCore — клиентское ядро Gravelands.
     * 
     * Назначение:
     * - Владеет окном, рендер-таргетом, изокамерой и презентацией
     *   (пост-пасс, ImGui-оверлей, консоль);
     * - Мир (ECS-сцена: тайлы, сфера, деревья, танцор, свет) живёт в
     *   общем `gravelands::World` (gravelands-world) — тот же мир
     *   правит эдитор (см. ARCHITECTURE.md, «Эдитор»);
     * - Один кадр = ввод + обновление камеры/света + симуляция мира
     *   + отрисовка (переменный dt);
     * 
     * Паттерн «lib + тонкий exe»: ClientCore даёт frame-API
     * (initialize/tick/shutdown) и НЕ владеет главным циклом —
     * цикл крутит тонкий exe (main.cpp). Этим же API эдитор сможет
     * хостить игру in-process (Play mode).
     */
    class ClientCore
    {
    public:
        ClientCore();
        ~ClientCore();

        // Ядро некопируемо и неперемещаемо (владеет графическими ресурсами)
        ClientCore(const ClientCore&) = delete;
        ClientCore& operator=(const ClientCore&) = delete;
        ClientCore(ClientCore&&) = delete;
        ClientCore& operator=(ClientCore&&) = delete;

        /**
         * Инициализировать ядро: окно, рендер-таргет, камеру, мир
         * (gravelands::World), сеть, ImGui. Вызывать один раз перед циклом.
         *
         * @param imguiEnabled Создавать ImGui-контекст/оверлей/консоль.
         *        false — режим PIE: клиент хостится в процессе эдитора,
         *        где ImGui-контекст уже есть (свой контекст клиенту
         *        создавать нельзя — он сломал бы кадр эдитора);
         *        оверлей/консоль клиента в этом режиме недоступны.
         * @return true при успехе (пока всегда, зарезервировано под будущие сбои)
         */
        bool initialize(bool imguiEnabled = true);

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
        /**
         * Обновление изометрической камеры: WASD двигает цель по земле,
         * Add/Subtract — зум. В сетевом режиме (подключены к серверу)
         * камера следует за зеркалом игрового юнита из снапшотов.
         * Вызывается из tick() каждый кадр.
         */
        void updateCamera(float deltaTime);

        /**
         * Полупрозрачный оверлей в углу: подсказка по клавишам
         * и текущие параметры света (ImGui, без ввода). Из tick().
         */
        void drawOverlay();

        /**
         * Опрос сети + применение снапшотов (интерполяция зеркал
         * юнитов). Из tick().
         */
        void updateNetworkState();

        /**
         * Применение интерполяции к зеркалам юнитов (между двумя
         * последними снапшотами). Из updateNetworkState().
         */
        void applyNetworkInterpolation();

        /**
         * Отправка команды игрока (WASD-вектор) при изменении.
         * Из tick() (только при подключённом сервере).
         */
        void sendMovementCommand();

        /**
         * Поиск зеркала серверного юнита (invalidEntity — нет).
         */
        beng::EntityID findMirror(buint64 serverEntityId);

        /**
         * Создание зеркала серверного юнита (сфера-плейсхолдер) в
         * клиентской сцене. invalidEntity — лимит зеркал исчерпан.
         */
        beng::EntityID createMirror(buint64 serverEntityId);

    private:
        // Pimpl: скрывает графические объекты blib (окно/таргет/камера)
        // от заголовка. Память выделяется через GlobalAllocator
        // (проектное правило: никаких new/delete и smart pointers).
        struct ClientCoreImpl;
        ClientCoreImpl* impl;
    };

} // namespace gravelands
