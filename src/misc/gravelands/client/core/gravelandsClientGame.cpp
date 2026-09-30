#include <gravelands/client/core/gravelandsClientGame.h>

#include <beng/client/clientApplication.h>
#include <beng/client/components/ambientLightComponent.h>
#include <beng/client/components/directionalLightComponent.h>
#include <beng/client/components/meshRenderComponent.h>
#include <beng/components/transform.h>
#include <beng/core/componentPool.h>
#include <beng/core/replicationCodec.h>

#include <blib/core/console/console.h>
#include <blib/graphics/color.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/sphere.h>

#include <imgui/imgui.h>

#include <cmath>

namespace gravelands
{
    namespace
    {
        // Скорость перемещения цели камеры по миру (WASD), мир. ед./с
        constexpr float cameraMoveSpeed = 60.0f;

        // Экспоненциальное сглаживание цели камеры при следовании за
        // зеркалом игрока (1/с): гасит остаточную дрожь интерполяции
        // снапшотов — жёсткая привязка передавала бы её камере
        constexpr float cameraFollowSmoothing = 12.0f;

        // Скорость зума (Add/Subtract), изменение дистанции в ед./с
        constexpr float cameraZoomSpeed = 120.0f;

        // Отступ и прозрачность оверлея-подсказки (полупрозрачный текст
        // в углу — вместо отладочной панели, см. фазу 4)
        constexpr float overlayPadding = 10.0f;
        constexpr float overlayBackgroundAlpha = 0.35f;

        // Радианы → градусы (для обратного расчёта углов света в оверлее)
        constexpr float lightRadToDeg = 57.29577951f;

        // Текст-заглушка оверлея, когда компонент света в сцене
        // отсутствует (после загрузки чужих сцен)
        constexpr const char* overlayValueMissing = "-";

        // ========== Визуал зеркал юнитов ==========

        // Визуал сетевого юнита: радиус и сегменты сферы-плейсхолдера
        constexpr buint32 networkUnitSegments = 24;
        constexpr buint8 networkUnitColorR = 200;
        constexpr buint8 networkUnitColorG = 160;
        constexpr buint8 networkUnitColorB = 110;
        constexpr buint8 networkUnitColorA = 255;

        // Текст статуса сети в оверлее
        constexpr const char* networkConnectedLabel = "connected";
        constexpr const char* networkOfflineLabel = "offline";
    }

    GravelandsClientGame::GravelandsClientGame()
        : application(nullptr)
        , scene(nullptr)
        , world()
        , playerNetworkEntity(beng::invalidEntity)
        , localPlayerRemoved(false)
        , cameraFollowActive(false)
        , predictedPosition(0.0f, 0.0f, 0.0f)
        , predictionActive(false)
        , predictionCommand{ 0, 0 }
        , lastSentCommand{ 0, 0 }
        , lastSentCommandValid(false)
        , simDeltaTime(0.0f)
    {
    }

    const char* GravelandsClientGame::getGameName() const
    {
        return gameTitle;
    }

    void GravelandsClientGame::onClientInitialize(_In beng::Scene& scene,
        _In beng::client::ClientApplication& application)
    {
        this->application = &application;
        this->scene = &scene;

        // Мир (gravelands-world): привязка к сцене клиента (типы,
        // системы тени/света), контент, консольные команды
        // scene_save/scene_load. Рендер-таргет мира — FBO оболочки
        this->world.initialize(scene);
        this->world.setRenderTarget(&application.getRenderTarget());
        this->world.registerConsoleCommands();

        // Контент: тайлы, сфера, деревья, тени, свет, камера-сущность +
        // скелетная модель с анимацией (Mixamo-FBX)
        this->world.setupWorld();
        this->world.loadDancerModel();

        // Подключение к серверу НЕ здесь: его делает ClientCore::initialize
        // ПОСЛЕ попытки local-server (startLocalServer сам коннектит
        // клиента к поднятому серверу, а при внешнем — connect порта) —
        // иначе первый connect улетел бы в пустоту (сервера ещё нет) и
        // залогировал ложный «connect failed»
    }

    void GravelandsClientGame::onInput(float deltaTime)
    {
        // dt этого кадра нужен onNetworkUpdate (команды/предсказание)
        this->simDeltaTime = deltaTime;

        // Отладочные клавиши работают ТОЛЬКО при закрытой консоли:
        // глобальный Keyboard не знает про фокус ImGui, и при печати
        // нажатия «протекали» бы в игру (M/O/N/P)
        if (this->application->isConsoleOpen())
        {
            return;
        }

        // N — отладочная раскраска нормалями (проверка проброса
        // нормали из VBO во фрагментный шейдер, фаза 2)
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::N))
        {
            this->application->getRenderTarget().rc.showNormals =
                !this->application->getRenderTarget().rc.showNormals;
        }

        // P — включение/выключение пост-пасса (фаза 6, сравнение до/после)
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::P))
        {
            this->application->setPostProcessEnabled(!this->application->isPostProcessEnabled());
        }

        // M — переключение сферы unlit/toon (сравнение до/после света).
        // Сфера — в мире: доступ через tryGetComponent (после
        // scene_load ID может устареть — не fatal)
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::M))
        {
            beng::MeshRenderComponent* sphereMesh =
                this->scene->tryGetComponent<beng::MeshRenderComponent>(this->world.getSphereEntity());
            if (sphereMesh != nullptr)
            {
                blib::graphics::Material& sphereMaterial = sphereMesh->getMesh().material;
                sphereMaterial.shadingMode =
                    sphereMaterial.shadingMode == blib::graphics::ShadingMode::Toon
                    ? blib::graphics::ShadingMode::Unlit
                    : blib::graphics::ShadingMode::Toon;
            }
        }

        // O — включение/выключение контура сферы (inverted hull, фаза 8)
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::O))
        {
            beng::MeshRenderComponent* sphereMesh =
                this->scene->tryGetComponent<beng::MeshRenderComponent>(this->world.getSphereEntity());
            if (sphereMesh != nullptr)
            {
                blib::graphics::Material& sphereMaterial = sphereMesh->getMesh().material;
                sphereMaterial.outlineEnabled = !sphereMaterial.outlineEnabled;
            }
        }
    }

    void GravelandsClientGame::onNetworkUpdate()
    {
        beng::ReplicationClient& replicationClient = this->application->getReplicationClient();
        beng::ReplicationClientState& mirror = replicationClient.getMirror();

        // События спавна зеркал: движок создал сущность с Transform —
        // игра вешает визуал. Первый спавн за сессию = сеть жива:
        // локальная сфера-заглушка больше не нужна
        beng::EntityID spawnEvents[beng::maxSnapshotEntities];
        const buint32 spawnCount = mirror.takeSpawnEvents(spawnEvents, beng::maxSnapshotEntities);
        for (buint32 i = 0; i < spawnCount; ++i)
        {
            if (!this->localPlayerRemoved)
            {
                this->world.removeLocalPlayerEntity();
                this->localPlayerRemoved = true;
            }
            this->attachMirrorVisual(spawnEvents[i]);
        }

        // События уничтожения: сущности уже удалены движком вместе с
        // визуалом — игра лишь сливает события (диагностика)
        beng::EntityID destroyEvents[beng::maxSnapshotEntities];
        mirror.takeDestroyEvents(destroyEvents, beng::maxSnapshotEntities);

        // Команда игрока (WASD) — при открытой сессии; она же питает
        // локальное предсказание (см. updatePlayerPrediction). При
        // открытой консоли не шлём — буквы команд не должны двигать
        // юнит (глобальный Keyboard не видит фокус ImGui)
        if (replicationClient.isSessionReady() && !this->application->isConsoleOpen())
        {
            this->sendMovementCommand();
        }

        // Реконсиляция по интерполированной позиции зеркала игрока +
        // локальное предсказание (движение начинается мгновенно)
        this->reconcilePlayerPrediction();
        this->updatePlayerPrediction(this->simDeltaTime);
    }

    void GravelandsClientGame::onSceneWillUpdate(float deltaTime)
    {
        this->updateCamera(deltaTime);

        // Отладочное управление светом (стрелки/[ ]/PageUp/PageDown) —
        // в мире (крутит компоненты света в его сцене)
        this->world.updateLight(deltaTime);
    }

    void GravelandsClientGame::onSceneDidUpdate(float deltaTime)
    {
        (void)deltaTime;
    }

    void GravelandsClientGame::onUi()
    {
        this->drawOverlay();
    }

    void GravelandsClientGame::onSessionReady(buint32 tickRate, beng::EntityID playerEntity)
    {
        this->playerNetworkEntity = playerEntity;

        __blib_log_info("%s client: session ready (tick rate %u, player entity %llu)",
            gameTitle, tickRate, static_cast<unsigned long long>(playerEntity));
    }

    void GravelandsClientGame::onSessionLost()
    {
        // Разрыв сессии: предсказание гасится (при следующем
        // подключении стартует заново от первого снапшота), камера
        // возвращается в офлайн-режим WASD
        this->playerNetworkEntity = beng::invalidEntity;
        this->predictionActive = false;
        this->predictionCommand = PlayerCommand{ 0, 0 };
        this->cameraFollowActive = false;

        __blib_log_info("%s client: session lost", gameTitle);
    }

    void GravelandsClientGame::onShutdown()
    {
        // Мир разрушается с игрой (член класса) — явных действий нет
    }

    void GravelandsClientGame::updateCamera(float deltaTime)
    {
        blib::graphics::IsometricCamera& camera = this->application->getCamera();

        // Зум — в обоих режимах: Add — приближение (дистанция
        // уменьшается), Subtract — отдаление
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Add))
        {
            camera.zoom(-cameraZoomSpeed * deltaTime);
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Subtract))
        {
            camera.zoom(cameraZoomSpeed * deltaTime);
        }

        // Сетевой режим: камера следует за зеркалом игрового юнита
        // (WASD уходит в команды серверу — см. sendMovementCommand)
        if (this->application->getReplicationClient().isSessionReady())
        {
            if (this->playerNetworkEntity != beng::invalidEntity)
            {
                beng::TransformComponent* mirrorTransform =
                    this->scene->tryGetComponent<beng::TransformComponent>(this->playerNetworkEntity);
                if (mirrorTransform != nullptr)
                {
                    const blib::math::Vector<float, 3> mirrorPosition = mirrorTransform->getWorldPosition();
                    if (!this->cameraFollowActive)
                    {
                        // Первое следование (старт сессии, переход
                        // офлайн→сеть): снап-установка — без полёта
                        // камеры через карту к далёкому зеркалу
                        camera.setTarget(mirrorPosition);
                        this->cameraFollowActive = true;
                    }
                    else
                    {
                        // Экспоненциальное сглаживание цели: alpha —
                        // frame-rate независимый фактор (1 - e^(-k*dt))
                        const float alpha = 1.0f - std::exp(-cameraFollowSmoothing * deltaTime);
                        const blib::math::Vector<float, 3> smoothed =
                            camera.getTarget() + (mirrorPosition - camera.getTarget()) * alpha;
                        camera.setTarget(smoothed);
                    }
                    camera.update();
                    return;
                }
            }
            // Зеркала игрока ещё нет (первый снапшот не пришёл) —
            // камера остаётся на месте
            camera.update();
            return;
        }

        // Офлайн-режим: WASD двигает цель камеры в плоскости земли
        // Горизонтальное направление взгляда (от камеры к цели, без Y):
        // движение WASD сдвигает цель камеры в плоскости земли, W —
        // «вверх экрана», D — «вправо экрана» (ось right = forward x up)
        blib::math::Vector<float, 3> lookDirection = camera.getTarget() - camera.getPosition();
        lookDirection.y = 0.0f;

        // lookDirection нулевой только при цели ровно под камерой —
        // при фиксированном наклоне это невозможно
        blib::math::Vector<float, 3> forward = blib::math::normalize(lookDirection);
        blib::math::Vector<float, 3> right = blib::math::normalize(
            blib::math::cross(forward, blib::math::Vector<float, 3>(0.0f, 1.0f, 0.0f)));

        blib::math::Vector<float, 3> movement(0.0f, 0.0f, 0.0f);
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::W))
        {
            movement = movement + forward;
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::S))
        {
            movement = movement - forward;
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::D))
        {
            movement = movement + right;
        }
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::A))
        {
            movement = movement - right;
        }

        // Движение без нормализации суммы: диагональ быстрее — приемлемо
        // для отладочного управления камерой
        camera.moveTarget(movement * (cameraMoveSpeed * deltaTime));

        camera.update();
    }

    void GravelandsClientGame::sendMovementCommand()
    {
        // WASD-вектор: D/A — ось X, W/S — ось Z (W — «от камеры»,
        // к центру мира при стартовом ракурсе)
        PlayerCommand command;
        command.moveX = static_cast<bint8>(
            (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::D) ? 1 : 0) -
            (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::A) ? 1 : 0));
        command.moveZ = static_cast<bint8>(
            (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::W) ? 1 : 0) -
            (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::S) ? 1 : 0));

        // Dedup: слать только при изменении вектора
        if (!this->lastSentCommandValid ||
            command.moveX != this->lastSentCommand.moveX ||
            command.moveZ != this->lastSentCommand.moveZ)
        {
            buint8 payload[commandPayloadSize];
            if (encodeCommandPayload(payload, commandPayloadSize, command) == commandPayloadSize)
            {
                this->application->getReplicationClient().sendCommand(payload, commandPayloadSize);
            }
            this->lastSentCommand = command;
            this->lastSentCommandValid = true;
        }

        // Текущий ввод питает локальное предсказание (каждый кадр)
        this->predictionCommand = command;
    }

    void GravelandsClientGame::reconcilePlayerPrediction()
    {
        if (!this->application->getReplicationClient().isSessionReady() ||
            this->playerNetworkEntity == beng::invalidEntity)
        {
            return; // сессии нет — игрока не знаем
        }

        // Позиция игрока в зеркале (интерполированная движком)
        beng::TransformComponent* mirrorTransform =
            this->scene->tryGetComponent<beng::TransformComponent>(this->playerNetworkEntity);
        if (mirrorTransform == nullptr)
        {
            return;
        }
        const blib::math::Vector<float, 3> serverPosition = mirrorTransform->getLocalPosition();

        if (!this->predictionActive)
        {
            // Первый снапшот игрока: предсказание стартует от зеркала
            // (скачка при старте сессии нет)
            this->predictedPosition = serverPosition;
            this->predictionActive = true;
            return;
        }

        // Штатное расхождение = скорость × латентность команды (сервер
        // воспроизводит те же команды, просто отстаёт на кадр) — ему
        // доверяем: постоянная коррекция дала бы видимый rubber-band
        // на остановке. Снап — только при реальной рассинхронизации
        const blib::math::Vector<float, 3> error = this->predictedPosition - serverPosition;
        const float errorLength = blib::math::length(error);
        if (errorLength > predictionSnapDistance)
        {
            __blib_log_debug("network: prediction snapped to server (divergence %.1f)",
                static_cast<double>(errorLength));
            this->predictedPosition = serverPosition;
        }
    }

    void GravelandsClientGame::updatePlayerPrediction(float deltaTime)
    {
        if (!this->application->getReplicationClient().isSessionReady())
        {
            // Разрыв сессии — предсказание гасится; при следующем
            // подключении стартует заново от первого снапшота
            this->predictionActive = false;
            this->predictionCommand = PlayerCommand{ 0, 0 };
            return;
        }
        if (!this->predictionActive)
        {
            return; // первый снапшот ещё не пришёл
        }

        // Интеграция ввода — формула 1:1 с MovementSystem сервера
        // (нормализованная диагональ, playerMoveSpeed, кламп worldBounds):
        // предсказанная траектория совпадает с серверной, просто
        // начинается раньше (сервер применяет команды с лагом кадра)
        const bint8 moveX = this->predictionCommand.moveX;
        const bint8 moveZ = this->predictionCommand.moveZ;
        if (moveX != 0 || moveZ != 0)
        {
            blib::math::Vector<float, 3> direction(
                static_cast<float>(moveX), 0.0f, static_cast<float>(moveZ));
            direction = blib::math::normalize(direction);

            blib::math::Vector<float, 3> position = this->predictedPosition;
            position = position + direction * (playerMoveSpeed * deltaTime);
            if (position.x > worldBounds) position.x = worldBounds;
            if (position.x < -worldBounds) position.x = -worldBounds;
            if (position.z > worldBounds) position.z = worldBounds;
            if (position.z < -worldBounds) position.z = -worldBounds;
            this->predictedPosition = position;
        }

        // Зеркало игрока рендерится по предсказанию — интерполированная
        // позиция (движковая) для игрока перекрывается
        beng::TransformComponent* mirrorTransform =
            this->scene->tryGetComponent<beng::TransformComponent>(this->playerNetworkEntity);
        if (mirrorTransform != nullptr)
        {
            mirrorTransform->setLocalPosition(this->predictedPosition);
        }
    }

    void GravelandsClientGame::drawOverlay()
    {
        // Полупрозрачный текст в углу: подсказка по клавишам и текущие
        // параметры света. Без рамок и заголовка, ввод не перехватывает
        ImGui::SetNextWindowPos(ImVec2(overlayPadding, overlayPadding), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(overlayBackgroundAlpha);

        const ImGuiWindowFlags overlayFlags =
            ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_NoNav
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::Begin("overlay", nullptr, overlayFlags);

        ImGui::TextUnformatted("WASD move | +/- zoom | Esc quit | ` console");
        ImGui::TextUnformatted("F5 hotreload | N normals | M toon/unlit");
        ImGui::TextUnformatted("Arrows: light dir | [ ]: light intensity");
        ImGui::TextUnformatted("PgUp/PgDn: ambient | P: post | O: outline");
        ImGui::TextUnformatted("console: scene_save [name] | scene_load [name]");
        ImGui::Separator();

        // Свет — компоненты сцены мира (см. directionalLightComponent.h):
        // показать текущие значения; компонента нет (чужой файл сцены
        // без света) — прочерк
        {
            beng::ComponentPool<beng::DirectionalLightComponent>* directionalPool =
                this->scene->tryGetComponentPool<beng::DirectionalLightComponent>();
            if (directionalPool != nullptr)
            {
                for (auto it = directionalPool->begin(); it != directionalPool->end(); ++it)
                {
                    const beng::DirectionalLightComponent& lightComp = *it;
                    const blib::math::Vector<float, 3>& dir = lightComp.getDirection();
                    const float dirY = (dir.y < -1.0f) ? -1.0f : ((dir.y > 1.0f) ? 1.0f : dir.y);
                    const float azimuthDeg = blib::math::atan2(dir.z, dir.x) * lightRadToDeg;
                    const float elevationDeg = -std::asin(dirY) * lightRadToDeg;

                    ImGui::Text("light azimuth: %.0f deg", static_cast<double>(azimuthDeg));
                    ImGui::Text("light elevation: %.0f deg", static_cast<double>(elevationDeg));
                    ImGui::Text("light intensity: %.2f", static_cast<double>(lightComp.getIntensity()));
                    break;
                }
            }
            else
            {
                ImGui::Text("light: %s", overlayValueMissing);
            }

            beng::ComponentPool<beng::AmbientLightComponent>* ambientPool =
                this->scene->tryGetComponentPool<beng::AmbientLightComponent>();
            if (ambientPool != nullptr)
            {
                for (auto it = ambientPool->begin(); it != ambientPool->end(); ++it)
                {
                    ImGui::Text("ambient intensity: %.2f", static_cast<double>(it->getIntensity()));
                    break;
                }
            }
            else
            {
                ImGui::Text("ambient: %s", overlayValueMissing);
            }
        }

        {
            // tryGetComponent: после scene_load ссылка может устареть
            beng::MeshRenderComponent* sphereMesh =
                this->scene->tryGetComponent<beng::MeshRenderComponent>(this->world.getSphereEntity());
            if (sphereMesh != nullptr)
            {
                const blib::graphics::Material& sphereMaterial = sphereMesh->getMesh().material;
                ImGui::Text("sphere shading: %s",
                    sphereMaterial.shadingMode == blib::graphics::ShadingMode::Toon ? "toon" : "unlit");
                ImGui::Text("sphere outline: %s", sphereMaterial.outlineEnabled ? "on" : "off");
            }
        }

        ImGui::Text("normals view: %s",
            this->application->getRenderTarget().rc.showNormals ? "on" : "off");
        ImGui::Text("post-process: %s",
            this->application->isPostProcessEnabled() ? "on" : "off");
        ImGui::Text("dancer model: %s",
            this->world.getDancerEntity() != beng::invalidEntity ? "loaded" : "not loaded");
        ImGui::Text("camera distance: %.0f",
            static_cast<double>(this->application->getCamera().getDistance()));
        ImGui::Text("network: %s",
            this->application->getReplicationClient().isSessionReady()
                ? networkConnectedLabel : networkOfflineLabel);

        ImGui::End();
    }

    void GravelandsClientGame::attachMirrorVisual(beng::EntityID entityId)
    {
        // Сфера-плейсхолдер юнита (toon, как локальная сфера-персонаж)
        // на зеркальную сущность, созданную движком (Transform уже есть)
        blib::graphics::Sphere sphere;
        sphere.createSpere(
            playerVisualRadius, networkUnitSegments,
            blib::graphics::Color(
                networkUnitColorR, networkUnitColorG, networkUnitColorB, networkUnitColorA));

        beng::MeshRenderComponent& meshComponent = this->scene->addComponent<beng::MeshRenderComponent>(
            entityId, sphere.takeMesh(), beng::RenderLayer::Opaque);
        meshComponent.getMesh().material.shadingMode = blib::graphics::ShadingMode::Toon;
    }

} // namespace gravelands
