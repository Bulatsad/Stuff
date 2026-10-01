#pragma once

#include <beng/config.h>
#include <beng/core/scene.h>
#include <beng/server/replicationClient.h>

#include <blib/graphics/isometricCamera.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/renderWindow.h>

#include <blib/blibint.h>
#include <blib/utilmacro.h>

namespace beng
{
    namespace server
    {
        class IServerGame;
        class ServerApplication;
    }

    namespace client
    {
        class IClientGame;

        /**
         * Параметры запуска клиентского ядра (значения игры —
         * размеры окна/FBO, заголовок, режим презентации).
         */
        struct __beng_api ClientApplicationParams
        {
            buint32 width;         // ширина окна (в PIE — ширина FBO)
            buint32 height;        // высота окна (в PIE — высота FBO)
            const char* title;     // заголовок окна (PIE — не используется)
            bool imguiEnabled;     // false = headless (PIE): без ОС-окна,
                                   // своего GL-контекста и ImGui — кадр
                                   // остаётся в FBO клиента
            bool predictionEnabled;      // движковый client-side prediction
                                         // игрока (IClientGame::
                                         // buildPlayerCommand/applyPlayerCommand)
            float predictionSnapDistance; // порог реконсиляции предикшна
                                         // (мир. ед., см. clientPrediction.h)
        };

        /**
         * ClientApplication — клиентское ядро движка (beng-client):
         * оболочка игры без игровых концепций. Владеет окном,
         * рендер-таргетом, изокамерой, пост-пассом, ECS-сценой с
         * базовым пайплайном (Transform → Animation → Render),
         * ImGui-презентацией и сетью (ReplicationClient).
         *
         * Игра подключается интерфейсом IClientGame (композиция, как
         * IServerGame у сервера): типы, системы, контент, ввод,
         * оверлей, кодек команд — игра; оболочка игра-агностична.
         *
         * Client-side prediction ИГРОКА — движковая фича оболочки
         * (clientPrediction.h + ReplicationClientState::
         * getLatestFieldSample): игра даёт построение команды ввода и
         * формулу интеграции (IClientGame::buildPlayerCommand/
         * applyPlayerCommand), оболочка шлёт команду с dedup,
         * реконсилирует предсказание со свежайшим серверным сэмплом и
         * перекрывает зеркало игрока (детали — CLIENT.md).
         *
         * Frame-API (паттерн «lib + тонкий exe», см. ARCHITECTURE.md):
         * initialize/tick/shutdown — циклом владеет тонкий exe (или
         * эдитор в PIE). Порядок кадра — см. CLIENT.md.
         *
         * Local-server mode (опция движка, см. CLIENT.md): при
         * startLocalServer ядро хостит ServerApplication in-process —
         * сервер тикает перед сетью клиента в каждом кадре; сетевой
         * путь идентичен продакшену (реальный loopback TCP).
         */
        class __beng_api ClientApplication
        {
        public:
            ClientApplication();
            ~ClientApplication();

            ClientApplication(const ClientApplication&) = delete;
            ClientApplication& operator=(const ClientApplication&) = delete;
            ClientApplication(ClientApplication&&) = delete;
            ClientApplication& operator=(ClientApplication&&) = delete;

            /**
             * Инициализация оболочки: окно/таргет/камера, пайплайн
             * сцены, игра (IClientGame), пост-пасс, ImGui.
             * @return true при успехе (зарезервировано под будущие сбои)
             */
            bool initialize(_In IClientGame& game, _In const ClientApplicationParams& params);

            /**
             * Поднять in-process сервер (local-server mode) и подключить
             * клиента к нему (loopback). Внешний сервер на порту уже
             * слушает → false (порт занят) — клиент подключится к нему
             * обычным connect() через getReplicationClient().
             * @return false — сервер уже запущен ядром или порт занят
             */
            bool startLocalServer(_In beng::server::IServerGame& serverGame, buint32 port);

            /**
             * Один кадр: окно → ввод → сеть (локальный сервер + клиент)
             * → игра → симуляция → рендер → ImGui (см. CLIENT.md).
             */
            void tick();

            /**
             * Корректное гашение (игра → сеть → локальный сервер →
             * ImGui → графические ресурсы).
             */
            void shutdown();

            /**
             * Условие продолжения цикла тонкого exe (окно открыто;
             * в headless-режиме — пока не вызван shutdown).
             */
            bool isRunning() const;

            /**
             * Идентификатор GL-текстуры текущего кадра (цвет FBO) —
             * вход Game-панели эдитора. 0 — ядро не инициализировано.
             */
            buint64 getColorTextureId() const;

            /**
             * ECS-сцена клиента (зеркала сети живут здесь же).
             */
            beng::Scene& getScene();

            /**
             * Рендер-таргет (FBO) клиента.
             */
            blib::graphics::IRenderTarget& getRenderTarget();

            /**
             * Изометрическая камера (ракурс игры).
             */
            blib::graphics::IsometricCamera& getCamera();

            /**
             * Окно (headless-режим — без контекста, см. GRAPHICS.md).
             */
            blib::graphics::RenderWindow& getWindow();

            /**
             * Сетевой клиент репликации: connect/sendCommand/зеркало
             * (события спавна/уничтожения — getMirror().take*Events).
             */
            beng::ReplicationClient& getReplicationClient();

            /**
             * Пост-пасс включён? (PIE: всегда false — экономия прохода)
             */
            bool isPostProcessEnabled() const;

            /**
             * Включить/выключить пост-пасс (debug-клавиши игры).
             */
            void setPostProcessEnabled(bool enabled);

            /**
             * Открыта ли консоль (тильда): игра гейтит свои горячие
             * клавиши и ввод, чтобы нажатия при печати не «протекали»
             * в игру (глобальный Keyboard не знает про фокус ImGui).
             */
            bool isConsoleOpen() const;

        private:
            // Pimpl: скрывает графические объекты blib и внутренности
            // кадра от заголовка (паттерн ClientCore). Память — через
            // GlobalAllocator (правило проекта).
            struct ClientApplicationImpl;
            ClientApplicationImpl* impl;

            /**
             * Старт камеры от активной камеры сцены (beng.Camera):
             * FOV/near/far — из компонента, ракурс — из трансформа
             * сущности (то, что видно в Game-превью эдитора). Камеры
             * нет — дефолты оболочки. Из initialize().
             */
            void syncCameraFromScene(_Out float& outNearDistance, _Out float& outFarDistance);
        };

    } // namespace client
} // namespace beng
