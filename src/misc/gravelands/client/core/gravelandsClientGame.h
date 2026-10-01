#pragma once

#include <gravelands/common/config.h>
#include <gravelands/common/protocol.h>
#include <gravelands/world/world.h>

#include <beng/client/iClientGame.h>
#include <beng/core/scene.h>

#include <blib/core/math/vector.h>

namespace gravelands
{
    /**
     * GravelandsClientGame — игровая сторона клиента Gravelands
     * (реализация IClientGame, композиция с движковым
     * beng::client::ClientApplication — см. ARCHITECTURE.md).
     *
     * Оболочка (окно, рендер, камера, ImGui, сеть, интерполяция
     * зеркал, client-side prediction игрока) — движок; здесь живёт
     * КОНКРЕТНАЯ игра:
     * - мир gravelands-world (тайлы, свет, танцор) в сцене клиента;
     * - отладочные клавиши (M/O/N/P, свет) и оверлей-подсказка;
     * - сетевая игра: визуал зеркал юнитов (сферы), WASD-команды
     *   (buildPlayerCommand — кодек ввода), формула интеграции ввода
     *   для движкового предикшна (applyPlayerCommand, 1:1 с
     *   MovementSystem сервера), follow-камера.
     *
     * Зеркала юнитов создаёт/двигает ДВИЖОК (ReplicationClientState,
     * интерполяция и предикшн — см. SERVER.md/CLIENT.md); игра сливает
     * события спавна/уничтожения в onNetworkUpdate и вешает/снимает
     * визуал.
     */
    class GravelandsClientGame : public beng::client::IClientGame
    {
    public:
        GravelandsClientGame();

        const char* getGameName() const __blib_override;
        void onClientInitialize(_In beng::Scene& scene, _In beng::client::ClientApplication& application) __blib_override;
        void onInput(float deltaTime) __blib_override;
        buint32 buildPlayerCommand(_Out buint8* out, buint32 capacity) __blib_override;
        void applyPlayerCommand(_In_Out blib::math::Vector<float, 3>& position,
            _In const buint8* payload, buint32 payloadSize, float deltaTime) __blib_override;
        void onNetworkUpdate() __blib_override;
        void onSceneWillUpdate(float deltaTime) __blib_override;
        void onSceneDidUpdate(float deltaTime) __blib_override;
        void onUi() __blib_override;
        void onSessionReady(buint32 tickRate, beng::EntityID playerEntity) __blib_override;
        void onSessionLost() __blib_override;
        void onShutdown() __blib_override;

    private:
        /**
         * Обновление изометрической камеры: WASD двигает цель по земле,
         * Add/Subtract — зум. В сетевом режиме камера следует за
         * зеркалом игрового юнита (экспоненциальное сглаживание).
         * Из onSceneWillUpdate.
         */
        void updateCamera(float deltaTime);

        /**
         * Полупрозрачный оверлей в углу: подсказка по клавишам,
         * параметры света, статус сети (ImGui, без ввода).
         */
        void drawOverlay();

        /**
         * Повесить визуал (сфера-плейсхолдер) на зеркальную сущность,
         * созданную движком (событие спавна).
         */
        void attachMirrorVisual(beng::EntityID entityId);

    private:
        // Ядро клиента (оболочка движка) и его сцена — ссылки из
        // onClientInitialize (живут дольше игры)
        beng::client::ClientApplication* application;
        beng::Scene* scene;

        // Мир Gravelands (системы тени/света + контент): общий с
        // эдитором, см. gravelands-world
        gravelands::World world;

        // EntityID игрока на сервере (из Welcome; invalidEntity — нет)
        beng::EntityID playerNetworkEntity;

        // Локальная сфера-заглушка убрана (первое зеркало пришло)
        bool localPlayerRemoved;

        // Следование камеры за зеркалом игрока: флаг «цель захвачена» —
        // при первом появлении зеркала камера встаёт сразу (без полёта
        // через карту), дальше — экспоненциальное сглаживание
        bool cameraFollowActive;
    };

} // namespace gravelands
