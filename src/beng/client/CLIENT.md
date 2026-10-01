# beng-client — клиентская оболочка движка

> Слой: `beng` (таргет `beng-client`, каталог `src/beng/client`). Шпаргалка по клиентскому ядру (`ClientApplication` + `IClientGame`).
> Не дублирует правила проекта (`AGENTS.md`) и roadmap (`ARCHITECTURE.md`) — только ссылается.
> **Обновлять при любом изменении кода beng-client** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-10-01

---

## Назначение и границы

- **beng-client** — игра-агностичная КЛИЕНТСКАЯ оболочка движка: окно, рендер-таргет, изокамера, пост-пасс, ECS-сцена с базовым пайплайном (Transform → Animation → Render), ImGui-презентация (оверлей/консоль), сеть (`ReplicationClient` из beng-server — клиент линкует beng-server, цикла нет) и **client-side prediction игрока** (движковая фича оболочки, см. ниже «Предикшн игрока»).
- **Игра подключается композицией** (паттерн `IServerGame` сервера): интерфейс `IClientGame` даёт типы, системы, контент, ввод, оверлей, кодек команд (в т.ч. `buildPlayerCommand`/`applyPlayerCommand` для предикшна); оболочка игровых концепций не знает.
- Взаимодействие с эдитором — через те же ядра: PIE хостит `ClientCore` headless-режимом (кадр в FBO в GL-контексте эдитора — см. GRAPHICS.md «Владение GL»).
- Что НЕ здесь: серверное ядро/репликация — SERVER.md; ECS-ядро/рефлексия — BENG.md; контент игры — gravelands (GRAVELANDS.md).

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Хук-интерфейс игры (`IClientGame`) | `iClientGame.h` |
| Ядро (frame-API, local-server, предикшн-блок, getters) | `clientApplication.h/.cpp` |
| Чистая логика предикшна игрока (`ClientPrediction`) | `clientPrediction.h` |
| Компоненты/системы рендер-ECS | `components/*`, `systems/*` (BENG.md) |
| Адаптер камеры (CameraComponent → ICamera) | `componentCameraAdapter.h/.cpp` |

## Инварианты и поток данных

### Жизненный цикл (frame-API «lib + тонкий exe»)

```
ClientApplication::initialize(game, params)
  → окно (оконное или headless) → FBO → камера (дефолты/активная beng.Camera)
  → пайплайн сцены (Transform/Animation/Render)
  → game.onClientInitialize(scene, *this)   // типы, системы, контент, connect
  → console-команды графики (hotreload) → syncCameraFromScene → пост-пасс → ImGui
startLocalServer(serverGame, port)          // опционально: local-server mode
tick() × N → shutdown()
```

### Порядок кадра (`ClientApplication::tick`) — ИНВАРИАНТ

```
time.tick → dt; simDt = консоль открыта ? 0 : dt (пауза на тильде)
window.update → Keyboard.update
консоль: тильда/Escape (Escape закрывает окно); F5 — hotreload (консоль закрыта)
localServer.tick()                          // авторитетная симуляция ПЕРЕД сетью клиента
replicationClient.poll(scene)               // снапшоты → зеркало → интерполяция
  + переходы сессии: Welcome → game.onSessionReady(tickRate, playerEntity)
                      разрыв → game.onSessionLost() (+ сброс предикшна/dedup)
game.onInput(simDt)                         // свои клавиши (гейт: !isConsoleOpen())
[предикшн игрока — движковый блок, см. ниже] // команда → реконсиляция → интеграция → перекрытие
game.onNetworkUpdate()                      // слив событий зеркал
game.onSceneWillUpdate(simDt)               // камера/свет до симуляции
renderTarget.clear → scene.update(simDt)    // системы, рендер в FBO
game.onSceneDidUpdate(simDt)
пост-пасс (или блит) → [headless: return]
ImGui-кадр: game.onUi() → консоль → render → swap
```

- Игра перекрывает зеркало ПОСЛЕ poll (визуал — в `onNetworkUpdate`); зеркало ИГРОКА перекрывает предикшн-блок оболочки — рендер читает финальные значения.
- В headless-режиме (PIE) ImGui-кадра нет, кадр остаётся в FBO (Game-панель эдитора).

### Порядок разрушения (КРИТИЧНО — GL-контекст)

- Члены impl объявлены: `window → renderTarget → camera → postProcess → scene → системы → … → replicationClient → consoleWindow`; разрушение обратное: **системы раньше сцены** (Scene хранит сырые указатели), **сцена раньше окна/таргета** (меши освобождают GL-ресурсы при живом контексте).
- `shutdown()`: `game.onShutdown()` (мир/сеть живы) → `replicationClient.shutdown()` → local-server shutdown+deallocate → ImGui teardown + снятие WndProc-хука → явный dtor impl + deallocate (GlobalAllocator).

### Сеть и зеркала

- Зеркала юнитов создаёт/двигает ДВИЖОК (`ReplicationClientState`): сущности с СЕРВЕРНЫМИ ID, интерполяция позиций (см. SERVER.md «Клиентская интерполяция»).
- Игра сливает **события зеркал** `takeSpawnEvents`/`takeDestroyEvents` в `onNetworkUpdate`: спавн → повесить визуал (addComponent<MeshRenderComponent>), destroy → сущность уже удалена движком, событие — уведомление.
- **ID-пространства:** контент клиента (создан ДО сессии) обязан жить в низком диапазоне (< `serverEntityIdBase`); серверные сущности — с высокой базы (см. SERVER.md).

### Предикшн игрока (движковая фича оболочки, 2026-10-01)

- **Оболочка владеет полным циклом** client-side prediction игрока (вынесен из gravelands): гейт — `params.predictionEnabled` && сессия открыта && консоль закрыта; блок в `tick()` между `onInput` и `onNetworkUpdate`.
- **Игра даёт только две вещи** (хуки `IClientGame`):
  - `buildPlayerCommand(out, capacity) → размер` — кодек текущего ввода (тот же формат, что принимает `IServerGame::onClientCommand`); 0 — команды нет;
  - `applyPlayerCommand(position, payload, size, dt)` — формула интеграции ввода 1:1 с серверной симуляцией (мутирует позицию: направление × скорость × dt, кламп границ) — предсказанная траектория совпадает с серверной, просто начинается раньше.
- **Порядок блока:** 1) `buildPlayerCommand` → отправка с **dedup по байтам** (повторяющиеся команды не плодят пакеты); 2) **реконсиляция** против НОВЕЙШЕГО СЕРВЕРНОГО сэмпла позиции (`ReplicationClientState::getLatestFieldSample(entityId, "beng.Transform", "position")` — резолв по стабильным именам; интерполированное значение сцены отстаёт на `interpolationDelayTicks` и для сверки не годится); 3) `applyPlayerCommand` + запись результата в зеркало.
- **Чистая логика — `clientPrediction.h` (`ClientPrediction`):** `reconcile(serverSampleValid, serverPosition, snapDistance)` — первый сэмпл: старт от серверной позиции; расхождение > `params.predictionSnapDistance` (порог настраивает игра): снап + debug-лог `prediction snapped to server`; сэмпла нет — предсказание продолжается. Штатный лаг (v·латентность) доверяем предсказанию — постоянная коррекция дала бы rubber-band. Покрыта группой тестов `client_prediction`.
- **Сброс:** разрыв сессии (`onSessionLost`-переход в `tick()`) — `prediction.reset()` + сброс dedup; следующее подключение стартует от первого снапшота. Зеркала игрока ещё нет (снапшот не пришёл/сцена сброшена) — предсказание ждёт, ничего не пишет.

### Local-server mode (опция движка)

- `startLocalServer(IServerGame&, port)`: пробник занятости порта (bind `TcpListener`-пробником — штатный фолбэк не логирует ERROR) → свободен: `ServerApplication` in-process + `replicationClient.connect(port)`; занят: false, клиент коннектится к внешнему серверу. Один и тот же сетевой путь (настоящий loopback TCP) для одиночной игры, внешнего сервера и PIE.

## Подводные камни / известные баги

- **MSVC и тернарник в init-списке:** конструкция `window(cond ? RenderWindow(...) : RenderWindow())` НЕ элидируется MSVC — временный `RenderWindow` копируется (указатель ctx) и СРАЗУ уничтожается: `~RenderWindow` удаляет GL-контекст, и первый же `InitGraphicsApi` (рендер-таргет) падает `wglGetProcAddress = NULL` («procedure not found», error 127) на `glGenBuffers`. Лечится только фабрикой «return prvalue» (`createClientWindow`) — пара `return T(...)` элидируется гарантированно (C++17). Диагностировалось: `wglGetCurrentContext()` == NULL в конструкторе рендер-таргета при живом контексте сразу после конструктора окна.
- **WndProc-хук ImGui** — глобальный `static` на процесс (паттерн model_viewer): не конфликтовать с хуком эдитора (у него свой); снимать ДО `ImGui::DestroyContext`.
- **Headless-режим:** `RenderWindow` без контекста — `makeCurrent`/blit no-op с guard'ами; кадр живёт в FBO; `isRunning` = флаг жизни (PIE его не использует).
- **Интерполяция зеркала и предикшн оболочки:** renderMirror пишет в сцену на каждом poll — предикшн-блок ОБОЛОЧКИ перекрывает зеркало игрока ПОСЛЕ poll (в том же кадре, до `scene.update`); следующей poll интерполированное значение вернётся и снова перекроется предикшном — порядок не менять.
- **Реконсиляция — только по новейшему сэмплу:** сверка с интерполированной позицией сцены (прошлая схема) сравнивала бы предсказание с состоянием, отстающим на `interpolationDelayTicks` — лишний накопленный лаг в ошибке. `getLatestFieldSample` читает кольцо сэмплов напрямую.
- **Пост-пасс в headless выключен** (`postEnabled = imguiEnabled`) — экономия целого прохода в PIE.
- **Сцена клиента занята контентом игры:** спавн зеркала с ID из низкого диапазона (сервер без `serverEntityIdBase`) отклонится («collides, spawn skipped») — см. ID-пространства выше.
- **Жалобы на input lag — диагностика по порядку (заметка на будущее):** (1) `interpolationDelayTicks = 2` — постоянный лаг интерполяции (33 мс при 60 Гц; если зеркало игрока вдруг перестало перекрываться предикшном — смотреть порядок предикшн-блока vs poll); (2) реконсиляция — снап-лог «prediction snapped to server» и порог `predictionSnapDistance` (config.h gravelands → `ClientApplicationParams`); (3) отправка команд — dedup/гейты предикшн-блока `ClientApplication::tick` (сессия открыта, консоль закрыта, `predictionEnabled`); (4) `setTcpNoDelay` (Nagle) в blib-network — должен быть включён (см. NETWORK.md).

## TODO

- [x] Предикшн как движковая фича оболочки (2026-10-01) — см. «Предикшн игрока» выше.
- [ ] Настройки оболочки через параметры игры (FOV/дистанция камеры по умолчанию).
- [ ] Мультиоконный режим оболочки (второй вьюпорт) — задел в `RenderWindow::makeCurrent`.

## Связанные доки

- `src/beng/server/SERVER.md` — сетевой слой, репликация, интерполяция зеркала.
- `src/beng/BENG.md` — ECS-ядро, рендер-ECS beng-client, рефлексия.
- `ARCHITECTURE.md` — слои, эдитор (PIE), roadmap (п.3).
- `src/blib/graphics/GRAPHICS.md` — окно/FBO/пост-пасс, владение GL.
- `src/misc/gravelands/GRAVELANDS.md` — игра-потребитель оболочки (миграция фазы 3).
- `GLOSSARY.md` — ClientApplication, IClientGame, local-server mode, событие зеркала.