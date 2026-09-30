#pragma once

#include <beng/config.h>
#include <beng/core/replicationCodec.h>
#include <beng/core/replicationSchema.h>
#include <beng/core/scene.h>
#include <beng/core/time.h>
#include <beng/server/iServerGame.h>
#include <beng/server/networkServer.h>
#include <beng/server/replicationManager.h>
#include <beng/server/worldManager.h>

#include <blib/blibint.h>

namespace beng
{
    namespace server
    {
        /**
         * ServerApplication — авторитетное серверное ядро движка
         * (beng-server): headless, без рендера/звука/ввода.
         *
         * Игра-агностик: конкретная игра подключается интерфейсом
         * IServerGame (типы, системы, контент, игроки, кодек команд).
         *
         * Frame-API (паттерн «lib + тонкий exe», см. ARCHITECTURE.md):
         * initialize/tick/shutdown — циклом владеет тонкий exe (или
         * эдитор в PIE). Тикрейт — фиксированный (getTickRate игры):
         * реальное время копится в аккумулятор и шагает scene.update()
         * фиксированными шагами; после каждого тика клиентам шлются
         * снапшоты репликации.
         *
         * Сетевой цикл (см. SERVER.md):
         *   poll → команды (IServerGame::onClientCommand)
         *   → тики симуляции (scene.update: TransformSystem + системы игры)
         *   → снапшоты (ReplicationManager → NetworkServer).
         */
        class __beng_api ServerApplication
        {
        public:
            // Максимум тиков за один кадр (защита от спирали догона при
            // длительном зависании процесса — симуляция не убегает)
            static constexpr buint32 maxTicksPerFrame = 8;

            ServerApplication();
            ~ServerApplication();

            ServerApplication(const ServerApplication&) = delete;
            ServerApplication& operator=(const ServerApplication&) = delete;

            /**
             * Инициализация: регистрация типов/систем игры, схема
             * репликации, контент мира, сетевой слушатель.
             *
             * @param game Игра (интерфейс IServerGame)
             * @param port Порт слушателя; 0 — порт по умолчанию игры
             * @return false — сеть недоступна (порт занят и т.п.)
             */
            bool initialize(_In IServerGame& game, buint32 port = 0);

            /**
             * Один кадр ядра: опрос сети → команды → шаг симуляции
             * фиксированными тиками (аккумулятор) → снапшоты клиентам.
             */
            void tick();

            /**
             * Корректное гашение (сеть, сцена).
             */
            void shutdown();

            /**
             * Условие продолжения цикла тонкого exe.
             */
            bool isRunning() const { return this->running; }

            /**
             * Авторитетная сцена (диагностика/PIE).
             */
            beng::Scene& getScene() { return this->scene; }

            /**
             * Персистентность мира (save/load/reset).
             */
            WorldManager& getWorldManager() { return this->worldManager; }

            /**
             * Счётчик выполненных тиков симуляции.
             */
            buint64 getTickCounter() const { return this->tickCounter; }

        private:
            // Авторитетная сцена (Transform регистрируется сценой сама)
            beng::Scene scene;

            // Игра (хуки IServerGame)
            IServerGame* game;

            // Схема репликации (построена по сцене в initialize)
            ReplicationSchema schema;

            // Сетевой слой и серверная репликация
            NetworkServer networkServer;
            ReplicationManager replicationManager;

            // Персистентность мира
            WorldManager worldManager;

            // Время реального мира (dt между вызовами tick)
            beng::Time time;

            // Аккумулятор реального времени для фиксированных тиков
            bfloat accumulator;

            // Счётчик выполненных тиков
            buint64 tickCounter;

            // Флаг жизни ядра
            bool running;

            // Буферы кадра (без аллокаций)
            buint8 welcomeBuffer[maxReplicationPacketBytes];
            buint8 snapshotBuffer[maxReplicationPacketBytes];
            ReplicationEntityPatch snapshotPatches[maxSnapshotEntities];

            // Принять события сети и маршрутизировать их игре/репликации
            void processNetworkEvents();

            // Шаг симуляции: один фиксированный тик + снапшоты
            void stepSimulation(bfloat fixedDelta);
        };

    } // namespace server
} // namespace beng
