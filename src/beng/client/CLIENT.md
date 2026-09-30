# beng-client — клиентская оболочка движка

> Слой: `beng` (таргет `beng-client`, каталог `src/beng/client`). Шпаргалка по клиентскому ядру (`ClientApplication` + `IClientGame`).
> Не дублирует правила проекта (`AGENTS.md`) и roadmap (`ARCHITECTURE.md`) — только ссылается.
> **Обновлять при любом изменении кода beng-client** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-09-30

---

## Назначение и границы

- **beng-client** — игра-агностичная КЛИЕНТСКАЯ оболочка движка: окно, рендер-таргет, изокамера, пост-пасс, ECS-сцена с базовым пайплайном (Transform → Animation → Render), ImGui-презентация (оверлей/консоль) и сеть (`ReplicationClient` из beng-server — клиент линкует beng-server, цикла нет).
- **Игра подключается композицией** (паттерн `IServerGame` сервера): интерфейс `IClientGame` даёт типы, системы, контент, ввод, оверлей, кодек команд; оболочка игровых концепций не знает.
- Взаимодействие с эдитором — через те же ядра: PIE хостит `ClientCore` headless-режимом (кадр в FBO в GL-контексте эдитора — см. GRAPHICS.md «Владение GL»).
- Что НЕ здесь: серверное ядро/репликация — SERVER.md; ECS-ядро/рефлексия — BENG.md; контент игры — gravelands (GRAVELANDS.md).

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Хук-интерфейс игры (`IClientGame`) | `iClientGame.h` |
| Ядро (frame-API, local-server, getters) | `clientApplication.h/.cpp` |
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
                      разрыв → game.onSessionLost()
game.onInput(simDt)                         // свои клавиши (гейт: !isConsoleOpen())
game.onNetworkUpdate()                      // слив событий зеркал, команды, предикшн
game.onSceneWillUpdate(simDt)               // камера/свет до симуляции
renderTarget.clear → scene.update(simDt)    // системы, рендер в FBO
game.onSceneDidUpdate(simDt)
пост-пасс (или блит) → [headless: return]
ImGui-кадр: game.onUi() → консоль → render → swap
```

- Игра перекрывает зеркало ПОСЛЕ poll (предикшн/визуал) — рендер читает финальные значения.
- В headless-режиме (PIE) ImGui-кадра нет, кадр остаётся в FBO (Game-панель эдитора).

### Порядок разрушения (КРИТИЧНО — GL-контекст)

- Члены impl объявлены: `window → renderTarget → camera → postProcess → scene → системы → … → replicationClient → consoleWindow`; разрушение обратное: **системы раньше сцены** (Scene хранит сырые указатели), **сцена раньше окна/таргета** (меши освобождают GL-ресурсы при живом контексте).
- `shutdown()`: `game.onShutdown()` (мир/сеть живы) → `replicationClient.shutdown()` → local-server shutdown+deallocate → ImGui teardown + снятие WndProc-хука → явный dtor impl + deallocate (GlobalAllocator).

### Сеть и зеркала

- Зеркала юнитов создаёт/двигает ДВИЖОК (`ReplicationClientState`): сущности с СЕРВЕРНЫМИ ID, интерполяция позиций (см. SERVER.md «Клиентская интерполяция»).
- Игра сливает **события зеркал** `takeSpawnEvents`/`takeDestroyEvents` в `onNetworkUpdate`: спавн → повесить визуал (addComponent<MeshRenderComponent>), destroy → сущность уже удалена движком, событие — уведомление.
- **ID-пространства:** контент клиента (создан ДО сессии) обязан жить в низком диапазоне (< `serverEntityIdBase`); серверные сущности — с высокой базы (см. SERVER.md).

### Local-server mode (опция движка)

- `startLocalServer(IServerGame&, port)`: пробник занятости порта (bind `TcpListener`-пробником — штатный фолбэк не логирует ERROR) → свободен: `ServerApplication` in-process + `replicationClient.connect(port)`; занят: false, клиент коннектится к внешнему серверу. Один и тот же сетевой путь (настоящий loopback TCP) для одиночной игры, внешнего сервера и PIE.

## Подводные камни / известные баги

- **MSVC и тернарник в init-списке:** конструкция `window(cond ? RenderWindow(...) : RenderWindow())` НЕ элидируется MSVC — временный `RenderWindow` копируется (указатель ctx) и СРАЗУ уничтожается: `~RenderWindow` удаляет GL-контекст, и первый же `InitGraphicsApi` (рендер-таргет) падает `wglGetProcAddress = NULL` («procedure not found», error 127) на `glGenBuffers`. Лечится только фабрикой «return prvalue» (`createClientWindow`) — пара `return T(...)` элидируется гарантированно (C++17). Диагностировалось: `wglGetCurrentContext()` == NULL в конструкторе рендер-таргета при живом контексте сразу после конструктора окна.
- **WndProc-хук ImGui** — глобальный `static` на процесс (паттерн model_viewer): не конфликтовать с хуком эдитора (у него свой); снимать ДО `ImGui::DestroyContext`.
- **Headless-режим:** `RenderWindow` без контекста — `makeCurrent`/blit no-op с guard'ами; кадр живёт в FBO; `isRunning` = флаг жизни (PIE его не использует).
- **Интерполяция зеркала и предикшн игры:** renderMirror пишет в сцену на каждом poll — игра обязана перекрывать зеркало игрока ПОСЛЕ poll (хук `onNetworkUpdate`), иначе следующей poll затрёт предикшн.
- **Пост-пасс в headless выключен** (`postEnabled = imguiEnabled`) — экономия целого прохода в PIE.
- **Сцена клиента занята контентом игры:** спавн зеркала с ID из низкого диапазона (сервер без `serverEntityIdBase`) отклонится («collides, spawn skipped») — см. ID-пространства выше.

## TODO

- [ ] Предикшн как движковая фича оболочки (сейчас — игра, gravelands).
- [ ] Настройки оболочки через параметры игры (FOV/дистанция камеры по умолчанию).
- [ ] Мультиоконный режим оболочки (второй вьюпорт) — задел в `RenderWindow::makeCurrent`.

## Связанные доки

- `src/beng/server/SERVER.md` — сетевой слой, репликация, интерполяция зеркала.
- `src/beng/BENG.md` — ECS-ядро, рендер-ECS beng-client, рефлексия.
- `ARCHITECTURE.md` — слои, эдитор (PIE), roadmap (п.3).
- `src/blib/graphics/GRAPHICS.md` — окно/FBO/пост-пасс, владение GL.
- `src/misc/gravelands/GRAVELANDS.md` — игра-потребитель оболочки (миграция фазы 3).
- `GLOSSARY.md` — ClientApplication, IClientGame, local-server mode, событие зеркала.