#pragma once

#include <gravelands/common/config.h>

namespace gravelands
{
    /**
     * ClientCore — клиентское ядро Gravelands.
     * 
     * Назначение:
     * - Владеет окном, рендер-таргетом, камерой и тайловым миром
     * - Один кадр = ввод + обновление + отрисовка (переменный dt)
     * - Рендерит через blib-graphics напрямую; рендер-ECS (beng-client)
     *   придёт позже — см. ARCHITECTURE.md
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
         * Инициализировать ядро: окно, рендер-таргет, камеру, тайлы.
         * Вызывать один раз перед циклом.
         * 
         * @return true при успехе (пока всегда, зарезервировано под будущие сбои)
         */
        bool initialize();

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
        // Pimpl: скрывает графические объекты blib (окно/таргет/камера)
        // от заголовка. Память выделяется через GlobalAllocator
        // (проектное правило: никаких new/delete и smart pointers).
        struct ClientCoreImpl;
        ClientCoreImpl* impl;
    };

} // namespace gravelands
