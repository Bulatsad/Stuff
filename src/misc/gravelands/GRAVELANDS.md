# GRAVELANDS — референсная игра (диаблоид)

> Слой: `game` (`src/misc/gravelands`). Шпаргалка по устройству игры: таргеты, камера, тайлы, таймстеп.
> Не дублирует правила проекта (`AGENTS.md`) и roadmap (`ARCHITECTURE.md`) — только ссылается.
> **Обновлять при любом изменении кода gravelands** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-10-01

---

## Назначение и границы

- **Gravelands — диаблоид** в духе «Path of Exile 2 с графикой Stronghold»; стилистический ориентир клиента — Hades 2 (см. ARCHITECTURE.md и план NPR-пайплайна в TODO).
- Таргеты (паттерн «lib + тонкий exe», каждый exe — тонкая `main()` над core-библиотекой):

| Таргет | Тип | Содержимое |
|--------|-----|------------|
| `gravelands-common` | library (STATIC) | общие определения: константы, кодек команд ввода (`protocol.h` — `PlayerCommand`, payload 2 байта), компоненты (`UnitComponent` — серверный кеш ввода, НЕ реплицируется) |
| `gravelands-world` | library | **мир**: `World` (ECS-сцена + контент: тайлы, сфера, деревья, тени, свет, танцор, камера-сущность `beng.Camera`), системы, консольные команды `scene_save`/`scene_load`, дебаг-свет клавишами; рендер-таргет выдаёт хост. Общий для клиента и эдитора (плагин-модель) |
| `gravelands-plugin` | library (STATIC/SHARED) | **плагин игры для единого эдитора**: `GravelandsEditorHost` поверх `EditorApplication` (мир в сцене эдитора) + игровой модуль — точка входа `bengGetGameModule()` (контракт `GameModuleFunctions`, beng/editor/gameModule.h; единственный экспорт gravelands.dll, гашение парной фабрикой — тип хоста известен только плагину); **PIE**: `PieSession` (Play/Pause/Stop в верхней полосе) + `GamePanel` (вкладка Game: кадр PIE-клиента или превью из активной камеры) |
| `gravelands-server-core` + `gravelands-server` | library + exe | игровая сторона авторитетного сервера: `ServerCore` — тонкая обёртка над движковым `beng::server::ServerApplication` + `GravelandsServerGame` (`IServerGame`: регистрация `UnitComponent` + системы `TransformSystem`/`MovementSystem`, юнит игрока в `onClientJoin`, кодек команд в `onClientCommand`). Фикс. тикрейт/сеть/снапшоты/репликация — движок (см. SERVER.md) |
| `gravelands-client-core` + `gravelands-client` | library + exe | представление: `ClientCore` — тонкая обёртка над движковым `beng::client::ClientApplication` (оболочка: окно/FBO/камера/пост-пасс/ImGui/сеть/client-side prediction) + `GravelandsClientGame` (`IClientGame`: мир gravelands-world, отладочные клавиши, оверлей, визуал зеркал юнитов, кодек WASD-команд `buildPlayerCommand` + формула интеграции ввода `applyPlayerCommand` для движкового предикшна, follow-камера); **local-server mode** — одиночный запуск клиента сам хостит сервер in-process |

- Слои: клиент/сервер зависят от beng/blib; `gravelands-common` — только от blib-int/beng-core.
- Что НЕ в этом доке: правила кодирования (AGENTS.md), план развития (ARCHITECTURE.md), детали ECS (BENG.md), детали рендера (GRAPHICS.md).

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Общие константы (тикрейт, окно, имя игры) | `common/config.h` |
| **Мир** (сцена, контент, scene_save/load, дебаг-свет) | `world/world.h/.cpp` |
| Сетка тайлов (квадраты на XZ) | `world/isometricTileset.h/.cpp` |
| Плагин игры для эдитора (хост + игровой модуль + PIE) | `plugin/gravelandsEditorHost.h/.cpp`, `plugin/pieSession.h/.cpp`, `plugin/gamePanel.h/.cpp` |
| Клиентское ядро (тонкая обёртка над `ClientApplication`) | `client/core/clientCore.h/.cpp` |
| Игровая сторона клиента (`IClientGame`: ввод/зеркала/команды/оверлей) | `client/core/gravelandsClientGame.h/.cpp` |
| Серверное ядро (тонкая обёртка над `ServerApplication`) | `server/core/serverCore.h/.cpp` |
| Игровая сторона сервера (`IServerGame`: типы/системы/юниты/команды) | `server/core/gravelandsServerGame.h/.cpp` |
| Перемещение юнитов (сервер) | `server/core/movementSystem.h/.cpp` |
| Кодек команд ввода (`PlayerCommand`, payload 2 байта) | `common/protocol.h` |
| Игровой компонент юнита | `common/unitComponent.h/.cpp` |
| Тонкий exe клиента / сервера | `client/main/main.cpp`, `server/main/main.cpp` |
| CMake-композиция подпроектов | `CMakeLists.txt` (корень gravelands) |

---

## Инварианты и поток данных

### Таймстеп (гибридный)

- **Сервер** — авторитет: `serverCore::tick()` меряет реальное dt (`beng::Time`), копит в аккумулятор и шагает `scene.update(serverFixedDelta)` фиксированными тиками 60 Гц (`serverTickRate`/`serverFixedDelta` из `common/config.h`). Несколько тиков за кадр — норм (догон), неиспользованный остаток живёт в аккумуляторе.
- **Клиент** — переменный dt кадра (`beng::Time::getDeltaTime()`), рендер каждый кадр.
- Сетевой цикл реализован (2026-09-28) — см. ниже «Сетевой цикл».

### Сетевой цикл (команды → тики → движковая репликация → интерполяция)

- **Сеть — движковая** (фаза 3 генерализации): сервер — `beng::server::ServerApplication` (фикс. тикрейт из `serverTickRate`, сетевой цикл poll → команды → тики → снапшоты), репликация — рефлексивная (Transform position/scale; провод/лимиты — SERVER.md). Собственной рукописной сети у игры больше нет: `NetworkServer`/`NetworkClient`/`MessageFramer` удалены. Игра пишет только кодек КОМАНД (`common/protocol.h`): `PlayerCommand {moveX, moveZ}` → payload 2 байта (сырые i8), движок возит его непрозрачным (`sendCommand`/`onClientCommand`).
- **Транспорт** — TCP (loopback; PIE и local-server — тот же код-путь), неблокирующие сокеты + опрос в кадрах (без потоков); Nagle выключен движком (`setTcpNoDelay`).
- **Игровая сторона сервера** (`GravelandsServerGame : IServerGame`): `onServerInitialize` — регистрация `UnitComponent` + системы `TransformSystem`/`MovementSystem` (guard `isRegisteredComponentType` — перезапуск PIE безопасен); `onWorldBuild` — пусто (статики у серверного мира нет; ID сущностей — с движковой базы `serverEntityIdBase`, см. SERVER.md «ID-пространства»); `onClientJoin` — сущность юнита (Transform: позиция `playerStartX/Z` — реплицируется; `UnitComponent`: isPlayer=true — входной кеш, НЕ реплицируется, клиенту не нужен); `onClientLeave` — уничтожение юнита (клиентам уезжает destroy-патчем); `onClientCommand` — декод `PlayerCommand` → moveX/moveZ юнита клиента (сервер не доверяет: команды только своему юниту, битый payload отбрасывается). `MovementSystem` — система игры, без изменений (диагональ нормализована, кламп `worldBounds`).
- **Игровая сторона клиента** (`GravelandsClientGame : IClientGame`): оболочка (`ClientApplication`, см. CLIENT.md) владеет сетью, интерполяцией И предикшном; хук `onNetworkUpdate` вызывается ПОСЛЕ poll (снапшоты применены/интерполированы) и ПОСЛЕ предикшн-блока оболочки: **слив событий зеркал** (`takeSpawnEvents`/`takeDestroyEvents` — сущности зеркал создаёт/удаляет ДВИЖОК с СЕРВЕРНЫМИ ID) → спавн: повесить визуал-сферу на зеркало + первый спавн удаляет локальную сферу-заглушку (`world.removeLocalPlayerEntity()`); **команды и предикшн — движковые**: WASD → `buildPlayerCommand` (кодек `protocol.h`: `PlayerCommand` → payload 2 байта; оболочка шлёт при изменении вектора, dedup по байтам, гейт консоли — оболочка), `applyPlayerCommand` — интеграция ввода ТОЙ ЖЕ формулой, что `MovementSystem` (нормализованная диагональ × `playerMoveSpeed`, кламп `worldBounds`) — зеркало игрока рендерится по предсказанной позиции (интерполированная позиция движка перекрывается), отклик мгновенный; реконсиляция — движковая, снап только при расхождении предсказания со свежайшим серверным сэмплом > `predictionSnapDistance` (см. CLIENT.md «Предикшн игрока»). Follow-камера — в `onSceneWillUpdate` (снап-установка при первом зеркале игрока, затем экспоненциальное сглаживание `cameraFollowSmoothing`).
- **Интерполяция — движковая** (`ReplicationClientState::renderMirror`, лаг `interpolationDelayTicks` = 2 тика, lerp по времени приёма — SERVER.md): позиции зеркал плавные, игровой интерполяции нет.
- **Офлайн-режим:** сервер недоступен — WASD двигает камеру, локальная сфера-персонаж остаётся; при первом зеркале удаляется. Сессия открылась → `onSessionReady` (тикрейт + EntityID игрока), разорвалась → `onSessionLost` (предсказание гасится, камера возвращается в офлайн-режим).
- **Local-server mode (одиночная игра):** `ClientCore::initialize` после подъёма оболочки зовёт `ClientApplication::startLocalServer(GravelandsServerGame, serverDefaultPort)`: порт свободен — in-process сервер тикает ПЕРЕД сетью клиента в каждом кадре оболочки; порт занят (внешний сервер/PIE) — молчаливый скип (проба порта bind'ом пробника — штатный фолбэк не логирует ERROR), клиент коннектится к внешнему. Один сетевой путь для одиночной игры, пары сервер+клиент и PIE.
- **Ограничения (MVP):** статический контент мира — процедурный на клиенте (Welcome-синхронизация полного мира — TODO); переподключение после потери сервера — нет (остаётся офлайн-режим); предикшн — движковый (2026-10-01, см. CLIENT.md).

### PIE (Play In Editor)

- **`PieSession`** (плагин): in-process хостинг `ServerCore` + `ClientCore`; `start(port)` — сервер слушает порт, затем клиент подключается (loopback TCP — настоящий сетевой путь); `tick()` — оба ядра; `stop()` — клиент → сервер. **Перезапуск безопасен** (Play → Stop → Play): сцена сервера сбрасывается движком (`WorldManager::buildWorld` → `Scene::reset`, регистрация типов/систем игры — с guard'ом в `GravelandsServerGame::onServerInitialize`), слушающий сокет пересоздаётся (`NetworkServer` движка → `TcpListener::open` после `shutdown`); клиент каждый раз создаётся заново. Local-server пробник клиента в PIE находит порт занятым PIE-сервером — молчаливый скип, клиент коннектится к PIE-серверу (см. «Local-server mode»).
- **Кнопки Play/Pause/Stop** в верхней полосе эдитора (`GravelandsEditorHost::onUi`; иконки-квадраты с тултипами, иконочный шрифт — каркас; полоса — под меню-баром каркаса File/Edit/Help, тема/шрифт — каркасные, см. BENG.md); кадр сессии — из `onSceneWillUpdate` (до отрисовки сцены эдитора); при остановке эдитора сессия гасится первой.
- **Пауза (Pause)**: `PieSession::setPaused` — не тикают ни сервер, ни клиент (симуляция и сеть замирают, последний кадр остаётся в FBO — Game-панель показывает замерший кадр). Кнопка подсвечена, пока пауза; Play всегда сбрасывает паузу.
- **Вкладка «Game» центральной области** (Scene | Game, как в Unity): `GamePanel` (ICenterTabView, плагин) рисует кадр игры из двух источников — (1) **PIE запущен**: FBO headless-клиента (`ClientCore::getColorTextureId`); (2) **сессия не запущена**: превью сцены эдитора из **активной камеры** (`beng.Camera`) — см. «Превью Game из активной камеры» ниже. Оба — `ImGui::Image` с letterbox, UV развёрнуты как во вьюпорте; активной камеры нет — заглушка «No active camera». Play автопереключает на Game, Stop/Escape — обратно на Scene (`setActiveCenterTab`).
- **Escape останавливает PIE** (`GravelandsEditorHost::onEscapePressed`): сессия запущена — стоп и возврат на вкладку Scene; иначе нажатие не перехватывается (каркас закроет приложение). Консоль при этом закрывается первой (штатный порядок каркаса).
- **Клиент в PIE — headless** (`ClientCore::initialize(false)`): ОС-окно НЕ создаётся (`RenderWindow` без контекста), ImGui-контекст не создаётся (он у эдитора — второй сломал бы кадр); клиент рендерит в свой FBO в GL-контексте эдитора, кадр показывает Game-вкладка. Оверлей/консоль/оконные клавиши клиента (Escape/тильда) в PIE выключены — Escape принадлежит эдитору. Ввод игры и горячие клавиши эдитора конфликтуют на общей клавиатуре — на время PIE каркас отключает редакторский ввод (`EditorApplication::setEditorInputEnabled(false)`).
- **Один GL-контекст:** и эдитор, и PIE-клиент рендерят в него (FBO клиента создан в контексте эдитора). Двойного контекста/multi-window в PIE больше нет; **Play по-прежнему отложен на начало кадра** (GL-ресурсы клиента создаются до отрисовки сцены эдитора, не в середине ImGui-кадра; клиент мог бы остаться оконным — см. GRAPHICS.md «Владение GL»).
- **Превью Game из активной камеры (без Play)** — «что увидим при запуске игры»: сцена эдитора рендерится в собственный FBO хоста (`GravelandsEditorHostImpl::gameTarget`, `IRenderTarget` под разрешение камеры `pixelWidth/Height`, ленивый ресайз) через `ComponentCameraAdapter` (beng-client: ICamera из `CameraComponent` + `TransformComponent`) и СОБСТВЕННЫЕ `RenderSystem` + `LightSystem` хоста (проход рисует только свет+меши — симуляция уже отработала в `scene.update()` каркаса; дублировать системы сцены нельзя). Проход — в `onSceneDidUpdate`, только когда вкладка Game активна и PIE не идёт; после прохода хост возвращает FBO вьюпорта (`IRenderTarget::bind()` — следующий шаг кадра, `drawGizmo`, рисует в текущий GL-бинд). Downscale кадра камеры до размера вкладки — GPU-фильтрация `ImGui::Image`. Камеры в сцене нет — заглушка «No active camera».
- **Камера-сущность (`beng.Camera`)** создаётся в `World::setupWorld()` (контент общий для клиента и эдитора): параметры — дефолты компонента (FOV 60°, near 0.1, far 1000, 1280×720, active), позиция (0, 70, −140) смотрит на центр мира; правка — Inspector (рефлексия: fov/near/far/разрешение/active) и gizmo (W/E/R). **Инвариант «одна активная камера»** — `CameraSystem` (beng-client, вешается миром; см. BENG.md): последняя включённая побеждает, остальные гаснут на кадре. Камера сериализуется со сценой (`scene_save`/`scene_load`).
- **Старт клиента от активной камеры:** оболочка (`ClientApplication::initialize`, ПОСЛЕ контента мира из `onClientInitialize`) читает активную `CameraComponent` СВОЕЙ сцены (`syncCameraFromScene`, см. CLIENT.md): FOV/near/far — из компонента (пост-пасс настраивается теми же near/far), yaw/pitch изокамеры — из направления взгляда сущности, целевая точка — перед камерой на текущей дистанции (позиция клиентской камеры == трансформу компонента; взгляд совпадает). Камеры нет — фолбэк на дефолты оболочки. Follow за игроком после старта — как раньше (вынос в игровые скрипты — TODO).
- **Перф PIE:** FBO клиента — `pieWindowWidth/Height` = 1280×720 (окна нет), пост-пасс в PIE выключен (`postEnabled` инициализируется `imguiEnabled`). Диагностика — debug-лог среднего кадрового времени PIE раз в секунду (`GravelandsEditorHost::onSceneWillUpdate`) и счётчик слитых снапшотов за кадр при догоняющих пачках (`network: drained N snapshots`). Сеть в PIE кадрово-зависима: команды/снапшоты движутся с частотой кадров эдитора — при тяжёлом кадре видимую задержку компенсирует client-side prediction игрока (см. выше).
- Порт — `serverDefaultPort` (занят внешним сервером — PIE-старт провалится с логом; параметр порта — TODO).

### Мир (World, gravelands-world)

- **`World` — контент + системы игры** (см. `world/world.h`): привязывается к ECS-сцене ХОСТА (`initialize(scene)`) и **сценой не владеет** — эдитор правит сцену каркаса `EditorApplication` (сериализация/Hierarchy/Inspector), клиент — свою сцену. Регистрирует 7 движковых типов beng-client с guard'ом (`isRegisteredComponentType` — каркас эдитора уже регистрировал их) и вешает ТОЛЬКО свои системы (Camera → BlobShadow → Light); базовый рендер-пайплайн (Transform → Animation → Render) вешает ХОСТ (эдитор — каркас, клиент — сам). Рендер-таргет мира — `setRenderTarget` (FBO хоста, применяется LightSystem; RenderSystem хоста таргетится самим хостом).
- **ИНВАРИАНТ — единый ECS-рендер:** алгоритм кадра «создать сцену → добавить объекты на сцену → отрисовать сцену». Вся отрисовка мира — ТОЛЬКО через `World::update(dt)` (`RenderSystem` beng-client, см. BENG.md); прямых `renderTarget.draw(...)` в хостах нет. Пост-пасс — после сцены, над FBO (презентация, не объект мира).
- **Мир — сущности со слоями рендера** (`World::setupWorld()` + `World::loadDancerModel()` в `initialize()` хоста):

| Объект | Компоненты | Слой RenderLayer |
|--------|-----------|------------------|
| Тайлы (сетка 10×10 квадратов на XZ, шахматная текстура) | `Transform` + `MeshRenderComponent` (ref на слот кеша `gravelands.tiles`; `IsometricTileset::buildMeshInto` собирает прямо в слот) | `Ground` |
| Тени (сфера r=30, танцор r=8, деревья r=16; радиальный градиент, подъём 0.5) | `Transform` + `MeshRenderComponent` (`BlobShadow::takeMesh()`) | `Shadow` |
| Привязка тени танцора к root-motion (строго под моделью) | `BlobShadowComponent` + `BlobShadowSystem` (кость `mixamorig:Hips`) | — |
| Деревья (3 квада 40×70, процедурная текстура, alpha-test, поворот к камере) | `Transform` + `MeshRenderComponent` (`SpritePlane::takeMesh()`) | `AlphaTested` |
| Сфера (r=25, шахматка, Toon + контур 0.6) | `Transform` + `MeshRenderComponent` (`Sphere::takeMesh()`) | `Opaque` |
| Танцор (`resources\Hip Hop Dancing.fbx`, масштаб 0.1, позиция (0,0,60)) | `Transform` + `SkinnedMeshComponent` (ref на слот кеша сцены: `loadFromFile(path, scene.getResources())`) + `AnimatorComponent` | `Opaque` (скиннинг) |
| Свет: направленный (азимут 30°/элевация 55°/интенсивность 0.9 → `direction`) | `DirectionalLightComponent` (`beng.DirectionalLight`, движковый — см. BENG.md) | — |
| Свет: эмбиент (дефолты компонента) | `AmbientLightComponent` (`beng.AmbientLight`) | — |
| Камера-сущность (позиция (0, 70, −140) смотрит на центр; дефолты `beng.Camera`: FOV 60°, 1280×720, active) | `Transform` + `CameraComponent` (`beng.Camera`, движковый — см. BENG.md) | — |

- **Тень танцора** следует за root-motion анимации и лежит **строго под моделью** (без смещения от источника света — blob-тень по определению «прижимает» объект к земле): `BlobShadowSystem` читает позицию кости таза (`mixamorig:Hips`, fallback «Hips»/подстрока) в model-space, переводит в мир и ставит тень на `(x, groundOffset, z)`. Стрелки света на тень не влияют (свет действует на toon-освещение).
- **Отладочное управление светом (в мире):** `World::updateLight(dt)` — стрелки крутят азимут/элевацию направленного света, `[`/`]` — его интенсивность, PageUp/PageDown — эмбиент. Крутит компоненты сцены напрямую (через пул, без хранения EntityID), в `RenderContext` их применяет `LightSystem` внутри `scene.update()` (см. BENG.md «beng-client»).
- **Деревья-билборды:** мир не знает камеру — поворот плоскостей зашит на стандартный ракурс изокамеры (`treeBillboardYawDegrees = 45°`, камера клиента смотрит с +X+Z). TODO: хост будет передавать направление своей камеры (у эдитора — орбитальной). Конвенция поворота — КВАТЕРНИОН (`TransformComponent`): `yaw = atan2(dir.x, dir.z)`; у Euler-`rotateY` (blib-graphics) знак противоположный — см. CORE.md «Грабли math».

### Клиент (ClientCore + GravelandsClientGame)

- **`ClientCore` — тонкая обёртка** над движковым `beng::client::ClientApplication` + `GravelandsClientGame`: `initialize(imguiEnabled)` собирает `ClientApplicationParams` (оконный — `windowWidth/Height` + title, PIE — `pieWindowWidth/Height`) → `application.initialize(game, params)` → `startLocalServer(...)`. Окно/FBO/камера/пост-пасс/ImGui/сеть/порядок кадра/разрушение GL-ресурсов — оболочка движка (см. CLIENT.md); pimpl клиента удалён.
- **`GravelandsClientGame : IClientGame`** — вся игровая логика клиента: `onClientInitialize` — `world.initialize(scene)`, `setRenderTarget` (FBO оболочки), `registerConsoleCommands`, `setupWorld` + `loadDancerModel`, `connect(serverDefaultPort)`; `onInput` — отладочные клавиши M/O/N/P (гейт `isConsoleOpen()`); `buildPlayerCommand`/`applyPlayerCommand` — кодек WASD-команд и формула интеграции ввода для движкового предикшна (см. «Сетевой цикл»); `onNetworkUpdate` — слив событий зеркал; `onSceneWillUpdate` — follow-камера + `world.updateLight`; `onUi` — оверлей; `onSessionReady`/`onSessionLost` — состояние сессии.
- **Камера** (`blib::graphics::IsometricCamera` оболочки, доступ — `application.getCamera()`): ракурс фиксирован (дефолты: pitch 55°, yaw 45°, FOV 30° — лёгкая перспектива), но **при старте оболочка принимает активную камеру сцены** (`beng.Camera`: FOV/near/far + позиция/направление — `ClientApplication::syncCameraFromScene`, см. CLIENT.md); WASD двигает `target` камеры в плоскости земли (офлайн), `Add`/`Subtract` — зум. После изменений обязателен `camera.update()`.
- **Порядок кадра** — движковый (`ClientApplication::tick`, см. CLIENT.md): окно → ввод → local-server → poll сети (+`onSessionReady/Lost`) → `onInput` → `onNetworkUpdate` → `onSceneWillUpdate` → `scene.update` (рендер) → `onSceneDidUpdate` → пост-пасс/блит → [headless: стоп] → ImGui (`onUi` + консоль) → swap. **Пауза на консоли:** оболочка передаёт симуляции нулевой dt при открытой тильда-консоли — мир замерзает, рендер рисует замершее состояние. В PIE (headless) кадр заканчивается после пост-пасса — кадр остаётся в FBO клиента (Game-вкладка эдитора), ImGui/презентации у клиента нет.
- **Отладочное управление (клавиши):** `N` — раскраска нормалями (флаг `renderTarget.rc.showNormals` оболочки); `M` — сфера unlit/toon; `O` — контур сферы (через `world.getSphereEntity()`); **стрелки**/`[`/`]`/PageUp/PageDown — свет (в мире, см. выше); **F5** — `hotreload` (оболочка, `registerGraphicsConsoleCommands()`); `P` — пост-пасс (`setPostProcessEnabled` оболочки); **тильда `~`** — консоль (Escape закрывает сперва её). **Отладочные клавиши работают только при закрытой консоли** (глобальный `Keyboard` не знает про фокус ImGui — печать в консоли не должна «протекать» в игру; `Escape`/тильда — оболочка, работают всегда). В PIE-режиме Escape/тильда у эдитора (Escape останавливает PIE); отладочные клавиши работают.
- **Консоль (тильда):** ImGui-окно `blib::graphics::console::ConsoleWindow` внизу экрана (оболочка; ядро — `blib::console::Console`: история, Tab-дополнение, `hotreload`). Команды сцены регистрирует мир (`World::registerConsoleCommands()`): `scene_save [имя]` — запись мира в JSON (дефолт `gravelands_scene.json`); `scene_load [имя]` — загрузка мира из JSON. См. ниже «Сохранение/загрузка сцены».
- **Оверлей** (`GravelandsClientGame::onUi`) — полупрозрачное ImGui-окно в углу (без рамок/ввода): параметры света/камеры + список клавиш + статус сети (`isSessionReady`); свет читается из пулов сцены мира (компонента нет — «-»). Паттерн вьювера: WndProc-хук оболочки + сцена → back-буфер (пост-пасс или `blitToBackbuffer`), ImGui поверх, `swapBuffers`.
- **Пути контента** резолвятся из cwd → каталога exe → подъёмом по родителям (`resolveContentPath` — в мире).

### Сохранение/загрузка сцены (консоль)

- `scene_save`/`scene_load` — проверка `Scene::save`/`load` (формат и контракты — `sceneSaveFormat.h` и BENG.md «Сохранение/загрузка сцены»). Команды и вся логика — в `World` (gravelands-world): консоль — любой хост с миром. Путь — первый аргумент (без аргумента — `gravelands_scene.json` в cwd).
- **`scene_load` сбрасывает сцену** (`World::resetScene()` → `Scene::reset()`): данные сцены сносятся на месте (сущности, пулы, кеш ресурсов), реестр типов и системы сохраняются — сцена снова пуста, `load()` её принимает, привязки хоста к сцене не рвутся (сцена принадлежит хосту и НЕ пересоздаётся).
- **Что восстанавливается:** тайлы — полный меш из файла (owned, ключ кеша `gravelands.tiles` не пишется); сфера/деревья/тени — меши с текстурами; танцор — полное содержимое модели + путь, в `onLoaded` перезагружается через `scene.getResources()` (файл на диске) и **встроенное сохранённое содержимое переприменяется к слоту** (материалы NPR/Toon, клипы — бит-в-бит; `reCommit` обновляет дайджест слота — см. RESOURCE_MANAGER.md), `AnimatorComponent::onLoaded` восстанавливает плейбек (клип/время/play, см. BENG.md); **свет (направленный + эмбиент)** — компоненты, восстанавливаются как есть, `LightSystem` применяет их в следующем же кадре. После успешного load вызывается `scene.verify()` — строгая проверка round-trip (результат в консоли).
- **Неудачный load** (битый файл/неизвестный тип) → откат на дефолтный процедурный мир (`World::setupWorld()` + `World::loadDancerModel()`), игра остаётся играбельной.
- **Ссылки `WorldImpl` на сущности** (`sphereEntity`, `dancerEntity`, массивы деревьев/теней) после load сбрасываются — ID в файле могут не совпасть; отладочные клавиши `M`/`O` и оверлей работают через `tryGetComponent` (не fatal на устаревшем ID).

### Сервер (ServerCore + GravelandsServerGame)

- **`ServerCore` — тонкая обёртка** над движковым `beng::server::ServerApplication` + `GravelandsServerGame`: `initialize(port)` → `server.initialize(game, port)`; `tick/shutdown/isRunning/getScene` — прокси. Аккумулятор тиков, сетевой цикл, снапшоты, репликация, heartbeat — движок (см. SERVER.md).
- **`GravelandsServerGame : IServerGame`** — игровая сторона: `getTickRate` = `serverTickRate` (60 Гц), `getDefaultPort` = `serverDefaultPort`; типы/системы — в `onServerInitialize` (guard для PIE-перезапуска); юниты игроков — `onClientJoin`/`onClientLeave`; кодек команд — `onClientCommand` (см. «Сетевой цикл»). `MovementSystem` — система игры (приоритет 0, после `TransformSystem` −100).

---

## Подводные камни / известные баги

- **Кеш ресурсов (2026-09-24):** тайлы и танцор грузятся через `scene.getResources()` (`blib::resource::ResourceManager`, см. RESOURCE_MANAGER.md) — сцена живёт в `World`: тайлы — процедурная сборка в слот + `commit`; танцор — Assimp-загрузка в слот + `commit`, повторная загрузка берёт общий слот. Сфера/деревья/тени остаются owned-мешами (`takeMesh`) — их типы (`Sphere`/`SpritePlane`/`BlobShadow`) не ISaveLoadable. Порядок жизни: сцена (и кеш в ней) разрушаются раньше окна — ref'ы компонентов отпускаются в `Scene::clear()` до деструктора кеша.
- **Порядок выгрузки GL:** в оболочке движка (`ClientApplicationImpl`) сцена/мир объявлены ПОСЛЕ окна/таргета и разрушаются РАНЬШЕ них — GL-контекст на момент освобождения ресурсов мешей жив (см. CLIENT.md и GRAPHICS.md «Владение GL»). Новые графические члены добавлять по контракту оболочки.
- **Изокамера:** `moveTarget` двигает цель в мировых координатах — движение по диагонали (W+D) быстрее одиночного; для геймплея нужна нормализация в `updateCamera`. Дистанция кламплена `[5, 100000]`, pitch `[1, 89]` — вырождение `lookAt` исключено.
- **Шахматная текстура тайлов** — 1 пиксель на клетку с LINEAR-фильтрацией: на границах ячеек лёгкое «просачивание» соседнего цвета. Терпимо для отладки; при появлении настоящих тайловых текстур заменить.
- **Сервер:** heartbeat печатает в консоль каждый тик первой секунды — шумно на старте (терпимо, но держать в уме при подключении сетевого слоя).
- **Одиночная игра — один процесс:** local-server mode поднимает авторитетный сервер in-process (см. «Сетевой цикл»); внешний сервер на порту — клиент автоматически подключается к нему (пробник порта). Двух процессов для одиночной игры не нужно.
- **Коллизия ID сущностей:** контент клиента (создан до сессии) живёт в низком диапазоне ID, серверные сущности — с движковой базы `serverEntityIdBase` (см. SERVER.md «ID-пространства») — не создавать на сервере сущности через ручные низкие ID.

---

## TODO

- [x] **NPR/Hades-пайплайн, фазы 1–9**: изокамера + тайлы на XZ; нормали в VBO/шейдеры + сфера; шейдерный минимум (владение GL-ресурсов, кеш uniform-локаций, hotreload-команда, относительные пути); мягкий toon-свет (DirectionalLight/AmbientLight, smoothstep-ramp, rim, emission) + оверлей и управление светом клавишами; рисованные плоскости (SpritePlane, unlit + alpha-test, процедурные «деревья»); пост-пасс (depth-fog, grading, виньетка, `P` — вкл/выкл); blob-тени (BlobShadow, блендинг без записи глубины); контуры (inverted hull, `O` — вкл/выкл); реальная скелетная модель (beng-client ECS, Mixamo-FBX с анимацией, свет + тень + контур).
- [x] **Единый ECS-рендер**: весь мир — сущности (`MeshRenderComponent` + `RenderLayer` в beng-client), отрисовка только через `scene.update()`/`RenderSystem`; прямых `renderTarget.draw(...)` в клиенте нет (инвариант закреплён в BENG.md и ARCHITECTURE.md).
- [x] **Свет в ECS** (2026-09-25): свет — движковые компоненты beng-client (`DirectionalLightComponent`/`AmbientLightComponent`, сериализуемы со сценой), применение в `RenderContext` — `LightSystem` (приоритет 90); отладочные клавиши крутят компоненты напрямую, эмбиент добавлен в управление (PageUp/PageDown) и оверлей. Дефолтный свет — как раньше (внешне ничего не поменялось).
- [ ] Полупрозрачность + сортировка, MSAA/FXAA, sRGB-конвейер (GRAPHICS.md TODO) — отдельными заходами, если понадобятся.
- [ ] Толщина контура должна масштабироваться от размера объекта/дистанции (сейчас фикс. мировые единицы).
- [x] **ResourceManager + миграция танцора и тайлов** (2026-09-24): кеш `ISaveLoadable` в blib-core (dedup по MD5 сериализованной формы, `ResourceRef`-refcount, слот в `Scene::getResources()`); тайлы собираются в слот кеша, танцор грузится через кеш с разделением слота. Сфера/деревья/тени — вне кеша (типы не ISaveLoadable); компоненты в dual-mode (ref ?? owned-фолбэк для verifyRoundTrip).
- [x] **Сохранение/загрузка сцены из консоли** (2026-09-25): `Scene::save`/`load` + `IComponent::onLoaded` (beng) — тильда-консоль, `scene_save`/`scene_load`, пересоздание сцены в `resetScene()`, плейбек аниматора восстанавливается бит-в-бит (`AnimatorComponent`), проверка `scene.verify()` после load (см. секцию «Сохранение/загрузка сцены»).
- [x] **Мир вынесен в `gravelands-world`** (2026-09-28): `World` (ECS-сцена + контент + scene_save/load + дебаг-свет) — общий для клиента и эдитора; `ClientCore` ужат до окна/камеры/ввода/пост-пасса/ImGui; деревья-билборды зашиты на стандартный ракурс (45°) — см. TODO ниже.
- [x] **Плагин Gravelands в эдиторе, этап 1** (2026-09-28): `gravelands-plugin` (GravelandsEditorHost + фабрика) линкуется в `beng-editor.exe` статически — единый эдитор правит мир Gravelands (мир — в каркасной сцене; панели Scene Hierarchy + Inspector, gizmo W/E/R, undo/redo и выбор кликом — каркасные; верхняя полоса — хост; ray-picking плагина через `onViewportClick`: ray-AABB мешей + сфера-фолбэк; `World::setSceneResetCallback` чистит историю команд на scene_load).
- [x] **Плагин Gravelands в эдиторе, этап 2 — DLL** (2026-09-28; доведён до запуска 2026-09-30): shared-сборка blib/beng (`*_build_dynamic`), `gravelands_plugin_type=SHARED` → gravelands.dll с единственной точкой входа (игровой модуль `bengGetGameModule`, контракт `GameModuleFunctions`), beng-editor.exe грузит её через LoadLibrary/GetProcAddress; оба режима собираются, проходят тесты и проверены запуском эдитора (см. ARCHITECTURE.md, «Эдитор»).
- [x] **Игровой модуль (контракт `GameModuleFunctions`, 2026-09-30)**: точка входа `bengGetGameModule()` (стабильный id игры `gameModuleName` = имя gravelands.dll = `--game=gravelands`, фабрики хоста, версия контракта); загрузчик эдитора игра-агностичен (дефайны CMake: `BENG_EDITOR_STATIC_GAME_HEADER`/`BENG_EDITOR_GAME_DLL`/`BENG_EDITOR_DEFAULT_GAME_NAME`); имя DLL починено (`OUTPUT_NAME` — эдитор искал несуществующий `gravelands.dll`); `initialize`/`shutdown` хоста — override виртуальных методов каркаса (до этого прятались — через базовый указатель звалась каркасная версия, impl хоста не создавался, AV на старте).
- [x] **Сетевой цикл + PIE** (2026-09-28): дочинен blib-network (NetworkError/WouldBlock/value-Address, группа тестов `network`); бинарный протокол + фреймер (группа `protocol`); `UnitComponent`; сервер: `NetworkServer` + `MovementSystem` + снапшоты; клиент: `NetworkClient` + интерполяция зеркал + офлайн-фолбэк; PIE: `PieSession` + Play/Stop в эдиторе (`setEditorInputEnabled` каркаса, клиент без своего ImGui).
- [x] **Сетевой feel (2026-09-29)**: Nagle выключен (`setTcpNoDelay` — отклик команд ~200 мс → мгновенный), интерполяция зеркал переведена с локального времени приёма на **tick number + кольцевой буфер с фиксированной задержкой рендера** (дрожь/телепорты при дышащем кадре ушли), камера следует за игроком с экспоненциальным сглаживанием. Тест `setTcpNoDelay` в группу blib `network`.
- [x] **Сетевой feel 2 (2026-09-29)**: слив ВСЕХ снапшотов за кадр (догоняющие пачки больше не рвут интерполяцию), **client-side prediction игрока** (ввод интегрируется локально формулой MovementSystem; реконсиляция — снап только при расхождении > `predictionSnapDistance`), тикрейт 30→60 Гц, PIE-перф: уменьшенное окно клиента (1280×720), пост-пасс выключен, debug-лог кадрового времени PIE.
- [x] **PIE в Game-вкладке (2026-09-30)**: клиент в PIE — headless (без ОС-окна и второго GL-контекста), кадр игры — во вкладке «Game» центральной области эдитора (Scene | Game, автопереключение Play/Stop/Escape); Pause (заморозка сервера и клиента) + Stop в верхней полосе; Escape останавливает PIE; перезапуск сессии починен (reset сцены сервера, переоткрытие слушающего сокета).
- [x] **Компонент камеры `beng.Camera` (2026-09-30)**: камера-сущность в мире (как Camera в Unity: Transform + FOV/near/far/разрешение/active), инвариант «одна активная камера» (`CameraSystem`), **Game-превью без Play** (сцена эдитора из активной камеры в FBO хоста, ресайз под разрешение камеры, downscale до вкладки), **старт клиента от активной камеры**, гизмо фрустума во вкладке Scene, иконка в иерархии/инспекторе, сериализация + тесты (группа `camera`).
- [x] **Миграция сети на движковую репликацию (фаза 3 генерализации, 2026-09-30):** `gravelands-server-core` → `beng::server::ServerApplication` + `GravelandsServerGame` (`IServerGame`); `gravelands-client-core` → `beng::client::ClientApplication` + `GravelandsClientGame` (`IClientGame`); `protocol.h` ужат до кодека команд (`PlayerCommand`, payload 2 байта); рукописные `NetworkServer`/`NetworkClient`/`MessageFramer`/снапшоты удалены. Зеркала юнитов и интерполяция — движковые (`ReplicationClientState` + флаг `FunctionField::interpolated`); события спавна/уничтожения зеркал — take-буферы; PIE работает без правок `PieSession` (пробник local-server находит порт занятым и молча подключается к PIE-серверу).
- [x] **Local-server mode (2026-09-30):** одиночный запуск клиента сам хостит авторитетный сервер in-process (`ClientApplication::startLocalServer` + `GravelandsServerGame`); внешний сервер — клиент подключается к нему (проба порта bind'ом, штатный фолбэк без ERROR-логов). Тесты: группа `protocol` — только кодек команд; группа `replication` — интерполяция зеркала/события.
- [ ] Welcome-синхронизация полного мира (статика из сервера вместо процедурной на клиенте).
- [ ] Мультиплеер (несколько клиентов, сессии), UDP-канал снапшотов, экстраполяция, переподключение.
- [x] Предикшн игрока — вынос в движок (2026-10-01): оболочка `ClientApplication` владеет полным циклом (команда с dedup → реконсиляция по свежайшему серверному сэмплу → интеграция → перекрытие зеркала); игра даёт `buildPlayerCommand`/`applyPlayerCommand` (см. CLIENT.md «Предикшн игрока»), группа тестов `client_prediction`.
- [ ] Спрайтовые персонажи (визуал юнитов вместо сфер-плейсхолдеров).
- [ ] Билборды: мир берёт направление взгляда из активной камеры сцены (`beng.Camera`; сейчас зашит стандартный ракурс 45°).
- [ ] Normalize движения камеры по диагонали, подгон скорости к дистанции зума.
- [ ] Камера клиента: follow-логика — в игровые скрипты, управляющие `beng.Camera` (клиентский `IsometricCamera` — временное решение; старт уже идёт от активной камеры).

---

## Связанные доки

- `AGENTS.md` — правила проекта и конвенции.
- `ARCHITECTURE.md` — слои, требования к beng, референсная игра, roadmap.
- `src/beng/BENG.md` — ECS-ядро, компоненты/системы, паттерны сцен.
- `src/beng/client/CLIENT.md` — клиентская оболочка (`ClientApplication`/`IClientGame`).
- `src/beng/server/SERVER.md` — сетевой слой, репликация, интерполяция зеркала.
- `src/blib/graphics/GRAPHICS.md` — рендер, камеры (IsometricCamera), меши, шейдеры.
