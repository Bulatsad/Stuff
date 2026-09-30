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
     * зеркал) — движок; здесь живёт КОНКРЕТНАЯ игра:
     * - мир gravelands-world (тайлы, свет, танцор) в сцене клиента;
     * - отладочные клавиши (M/O/N/P, свет) и оверлей-подсказка;
     * - сетевая игра: визуал зеркал юнитов (сферы), WASD-команды,
     *   client-side prediction игрока (формула 1:1 с MovementSystem
     *   сервера), follow-камера.
     *
     * Зеркала юнитов создаёт/двигает ДВИЖОК (ReplicationClientState,
     * интерполяция — см. SERVER.md); игра сливает события спавна/
     * уничтожения в onNetworkUpdate и вешает/снимает визуал.
     */
    class GravelandsClientGame : public beng::client::IClientGame
    {
    public:
        GravelandsClientGame();

        const char* getGameName() const __blib_override;
        void onClientInitialize(_In beng::Scene& scene, _In beng::client::ClientApplication& application) __blib_override;
        void onInput(float deltaTime) __blib_override;
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
         * Отправка команды игрока (WASD-вектор) при изменении.
         * Из onNetworkUpdate (только при открытой сессии).
         */
        void sendMovementCommand();

        /**
         * Интеграция локального ввода в предсказанную позицию игрока
         * (формула 1:1 с MovementSystem сервера) и применение её к
         * зеркалу игрока. Из onNetworkUpdate.
         */
        void updatePlayerPrediction(float deltaTime);

        /**
         * Реконсиляция client-side prediction: сверка предсказанной
         * позиции с позицией игрока в зеркале (интерполированной).
         * Доверяем предсказанию (сервер воспроизводит те же команды
         * с лагом); снап — только при расхождении больше
         * predictionSnapDistance. Из onNetworkUpdate.
         */
        void reconcilePlayerPrediction();

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

        // ===== Client-side prediction игрока =====
        // Предсказанная позиция + текущий локальный ввод. Ввод
        // интегрируется локально той же формулой, что MovementSystem
        // сервера (нормализованная диагональ × playerMoveSpeed, кламп
        // worldBounds) — движение начинается МГНОВЕННО, сеть и кадровое
        // время влияют только на реконсиляцию
        blib::math::Vector<float, 3> predictedPosition;
        bool predictionActive;
        PlayerCommand predictionCommand;

        // Последняя отправленная команда (dedup — слать при изменении)
        PlayerCommand lastSentCommand;
        bool lastSentCommandValid;

        // dt кадра симуляции (onInput → onNetworkUpdate этого кадра)
        float simDeltaTime;
    };

} // namespace gravelands
