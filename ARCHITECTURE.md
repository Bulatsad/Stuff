# Архитектура проекта Stuff — слои, движок beng, референсная игра

> Документ закрепляет разделение ответственностей между слоями проекта
> (`blib` → `beng` → `game`), требования к движку beng и целевую
> архитектуру референсной игры (диаблоид). Статус: план, не код.
> Обновляется вместе с развитием кодовой базы.
> Сверено: 2026-10-01

---

## 🎯 Общие принципы

1. **Три слоя, строгая иерархия зависимостей.** Нижний слой не знает о верхнем.
2. **blib — кроссплатформенный по задумке.** Windows реализован полностью;
   Linux/macOS — заглушки, которые обязаны со временем стать реализациями.
   beng и game зависят от blib напрямую, без прослоек.
3. **Движок (beng) — среда выполнения игры, а не сама игра.** Всё, что относится
   к конкретной игре (правила, контент, геймплей), живёт в слое game.
4. **Каждый исполняемый файл — тонкая обёртка над core-библиотекой**
   (паттерн «lib + тонкий exe»). Core-библиотеки не владеют главным циклом.
5. **Эдитор — один на все игры** (плагин-модель, как Unity/Unreal): один
   `beng-editor.exe`, игра поставляется ему как библиотека/DLL, сам эдитор
   игру не знает. Никаких эдиторов на игру (типа `gravelands-editor`) нет.
6. **Производительность игры не зависит от выбора модели эдитора** —
   горячие циклы никогда не пересекают границу плагина.

---

## 📋 Слои

| Слой | Ответственность | Примеры (что здесь живёт) |
|------|-----------------|---------------------------|
| `blib` | Сервисы и платформенные примитивы. Ноль игровых концепций. | аллокаторы, математика, консоль/лог, OpenGL-обёртка, звук, сокеты |
| `beng` | Среда выполнения игры: цикл, ECS, модули, ресурсы, эдитор-каркас. Игра-агностик. | Application, Scene, RenderModule, Inspector-панели |
| `game` | Конкретная игра: правила, контент, геймплей. | UnitComponent, CombatSystem, статы монстров, уровни |

Правило чтения границ: если код спрашивает «**как** это сделать» — это blib;
«**как устроена** игра в целом» — это beng; «**что это за** игра» — это game.

### blib (уже существует)

| Модуль | Назначение |
|--------|------------|
| `blib-system` | память (аллокаторы), потоки и синхронизация |
| `blib-core` | math, console/лог, потоки (streams), алгоритмы (FFT, hash, сжатие), pdl |
| `blib-graphics` | OpenGL-обёртка: окно, шейдеры, меши, текстуры, камера, ImGui-интеграция |
| `blib-sound` | звуковые устройства, запись/воспроизведение (WinMM) |
| `blib-network` | сокеты TCP/UDP (winsock) |

**Зафиксировано:** `blib` кроссплатформенный по задумке; сегодня Windows-реализации,
на других платформах — заглушки. beng линкует blib-модули напрямую.

---

## 🔧 beng (Bulat Engine)

### Целевые таргеты

| Таргет | Назначение | Зависимости |
|--------|------------|-------------|
| `beng-core` | общее ядро: ECS, Application, тикрейт, интерфейсы модулей, рефлексия, репликация (схема/кодек/клиентское зеркало), сетевой фрейминг | blib-core, blib-system |
| `beng-client` | клиентская среда: рендер-ECS + **клиентская оболочка** `ClientApplication` (окно/FBO/камера/пост-пасс/ImGui/сеть) + `IClientGame` — **сделан (2026-09-30)**, см. `src/beng/client/CLIENT.md`; RenderModule/InputModule/AudioModule — будущее | beng-core, blib-graphics, beng-server (сетевой слой) |
| `beng-server` | headless-сервер: `ServerApplication` (тикрейт + сетевой цикл), `NetworkServer`, `ReplicationManager` (рефлексивная репликация), `WorldManager` (save/load), `ReplicationClient` (сетевой клиент репликации + интерполяция) — **сделан (2026-09-30)**, см. `src/beng/server/SERVER.md` | beng-core, blib-network |
| `beng-editor-core` | каркас эдитора: `EditorApplication` (окно/вьюпорт/панели/раскладка — есть) + докинг, Hierarchy, Inspector, gizmo, selection, undo/redo | beng-client, ImGui |
| `beng-editor` | ЕДИНЫЙ exe эдитора на все игры: тонкая `main()` над `beng-editor-core` (есть, пустая сцена); игра — плагин | beng-editor-core |

### Требования

#### must (контракт, на который можно рассчитывать)

- `beng-core` предоставляет ECS-ядро (`Scene`, `ComponentPool`, `ISystem` —
  уже есть) и `Application` с frame-API: `initialize()` / `tick(dt)` / `shutdown()`.
- **ECS-инвариант сцены:** сущность не может существовать без
  `TransformComponent` — сцена регистрирует его автоматически (typeId 0)
  и создаёт каждой сущности в `createEntity()`; удаление запрещено;
  сериализация сцены требует Transform у каждой сущности (файлы без
  него — `InvalidData`). Подробности — AGENTS.md и BENG.md.
- Симуляция идёт с **фиксированным тикрейтом**; рендер и ввод — с частотой кадра.
- `beng-core` предоставляет **рефлексию компонентов**: статический дескриптор
  типа (имя, список полей с доступом). Из неё растут Inspector, сериализация
  сцен, save/load мира и будущая сетевая репликация. **Сделано (2026-09-28):**
  `componentReflection.h` (FieldValue/IComponentField/FunctionField/
  ComponentTypeDescriptor, трейт `HasComponentReflection`) + type-erased
  сцены-API для эдитора — см. BENG.md, «Рефлексия компонентов».
- `beng-server` работает headless: без рендера, звука и ввода. **Сделан
  (2026-09-30)** — см. «beng-server» ниже и `src/beng/server/SERVER.md`.
- `beng-editor` — игра-агностик: работает только с `beng-core` типами
  и рефлексией, конкретных игровых типов не знает.

#### must not (жёсткие границы)

- В beng **нет ни одного компонента/системы геймплея конкретной игры**
  (Unit, Item, Skill, Health — всё в game-слое).
- В beng **нет вшитых путей к контенту** игры и имён игровых ассетов.
- beng **не владеет главным циклом** вызывающего процесса: всегда frame-API,
  цикл пишет тонкий exe или эдитор.
- beng **не использует C++ исключения**, `new/delete`, smart pointers,
  `printf`-семейство — действуют общие строгие правила проекта (см. AGENTS.md).

#### future (ориентиры, не обещания)

- UDP-канал для снапшотов (на старте — только TCP).
- Сетевая репликация компонентов на базе рефлексии. **Сделано
  (2026-09-30)** — рефлексивная репликация в beng-core, серверная
  сторона и сетевой клиент в beng-server, клиентская оболочка и
  движковая интерполяция — в beng-client (см. SERVER.md/CLIENT.md).
- 3D-контент с изометрической камерой — **начато**: `MeshRenderComponent` +
  `RenderLayer` реализованы, весь мир рисуется единым путём через
  `RenderSystem` (см. BENG.md, «beng-client»); свет в ECS — позже.
- Hot-reload игровой DLL в эдиторе (не на старте).

### beng-server (реализован, 2026-09-30)

- **`ServerApplication`** — игра-агностичное headless-ядро: frame-API,
  тикрейт-аккумулятор (`IServerGame::getTickRate`, `maxTicksPerFrame`),
  сетевой цикл (poll → команды → тики → снапшоты).
- **`NetworkServer`** — неблокирующие сокеты, слоты клиентов (массив,
  MVP 4), фрейминг (`ReplicationFramer`), очередь отправки с
  backpressure (дроп при переполнении → полный ресинк клиента).
- **`ReplicationManager`** — серверная сторона рефлексивной репликации:
  per-client зеркала, полные/дельта/destroy-снапшоты; хеш-сверка схем
  полей при Welcome.
- **`WorldManager`** — save/load мира (`Scene::save/load`) + пересборка
  контента хук-функцией игры.
- **`IServerGame`** — хук-интерфейс игры (типы/системы/контент/игроки/
  кодек команд — команды для движка непрозрачны).
- Детали, формат провода, лимиты и грабли — `src/beng/server/SERVER.md`.

### beng-client (реализован, 2026-09-30)

- **`ClientApplication`** — игра-агностичная клиентская оболочка:
  frame-API, окно/FBO/изокамера/пост-пасс, ECS-сцена с пайплайном
  Transform → Animation → Render, ImGui (оверлей/консоль, WndProc-хук),
  сеть (`ReplicationClient`), headless-режим (PIE: кадр в FBO в
  контексте эдитора), **local-server mode** (`startLocalServer` —
  in-process `ServerApplication` с пробой порта; один сетевой путь для
  одиночной игры/внешнего сервера/PIE).
- **`IClientGame`** — хук-интерфейс игры (композиция, как `IServerGame`):
  типы/системы/контент, ввод, оверлей, сетевой кадр (события зеркал),
  кодек команд и формула интеграции ввода для движкового предикшна
  (`buildPlayerCommand`/`applyPlayerCommand`), сессия
  (`onSessionReady`/`onSessionLost`).
- **Интерполяция зеркал — движковая** (beng-core `ReplicationClientState`
  + флаг `FunctionField::interpolated`): позиции снапшотов lerp'ятся с
  постоянным лагом (см. SERVER.md).
- **Client-side prediction игрока — движковый (2026-10-01):** оболочка
  `ClientApplication` владеет полным циклом (команда с dedup →
  реконсиляция по свежайшему серверному сэмплу →
  `IClientGame::applyPlayerCommand` → перекрытие зеркала), см. CLIENT.md.
- Детали, порядок кадра, разрушение GL-ресурсов, грабли —
  `src/beng/client/CLIENT.md`.

### Паттерн «lib + тонкий exe»

Каждый исполняемый файл проекта — тонкая обёртка над core-библиотекой:

```
gravelands-client.exe ──> gravelands-client-core ──> beng-client ──> beng-core ──> blib
gravelands-server.exe ──> gravelands-server-core ──> beng-server ──> beng-core ──> blib
beng-editor.exe ──> beng-editor-core (EditorApplication) + game plugin ──> beng-client ──> beng-core ──> blib
gravelands-common ──> beng-core ──> blib
```

- Core-библиотеки дают frame-API (`initialize` / `tick` / `shutdown`),
  **не владеют** while-циклом.
- Тонкий exe содержит только `main()`: создаёт приложение, крутит цикл,
  корректно гасит.
- **Эдитор — единый `beng-editor.exe` на все игры** (никаких
  `gravelands-editor` и пр.): `EditorApplication` (каркас beng-editor)
  + плагин игры (этап 1 — статический линк, этап 2 — DLL). Он же хостит
  игру in-process в PIE: сам вызывает `tick` ядер внутри своего ImGui-цикла.
- Игра названа **Gravelands** (диаблоид): таргеты именуются `gravelands-*`,
  namespace — `gravelands`, инклюды — `<gravelands/...>` (корень `src/misc`).

---

## 🎮 Референсная игра — Gravelands (диаблоид)

Условный «Path of Exile 2 с графикой Stronghold»: изометрическая ARPG,
клиент-сервер, серверный авторитет.

### Исполняемые файлы и библиотеки

| Модуль | Тип | Назначение |
|--------|-----|------------|
| `gravelands-common` | library | общие определения: константы (есть), позже — компоненты, пакеты протокола, формулы, статы |
| `gravelands-world` | library | мир: `World` — общая ECS-сцена (контент + системы + scene_save/load), рендер-таргет выдаёт хост; общий для клиента и эдитора (плагин) |
| `gravelands-plugin` | library (STATIC/SHARED) | плагин игры для единого эдитора: `GravelandsEditorHost` + фабрика `gravelandsCreateEditorHost()` (на DLL-этапе — экспорт gravelands.dll) |
| `gravelands-server-core` + `gravelands-server.exe` | library + exe | авторитетная симуляция: Movement, Combat, Loot, Session |
| `gravelands-client-core` + `gravelands-client.exe` | library + exe | представление: ввод, камера, интерполяция, рендер, UI |
| `beng-editor.exe` (единый на все игры) | exe | эдитор: правит сцены любой игры (плагин), Play mode (PIE) |

**Сервер — всегда отдельный процесс.** Даже одиночная игра: запускается
локальный `gravelands-server.exe`, клиент подключается по loopback. In-process
хостинг сервера существует только внутри эдитора (PIE) и только через
core-библиотеку — см. раздел про эдитор.

**Таймстеп — гибридный** (как FixedUpdate/Update в Unity):
сервер шагает симуляцию фиксированными тиками 60 Гц с аккумулятором
(`serverTickRate`/`serverFixedDelta` в `gravelands-common`), клиент рендерит
и читает ввод с переменным dt.

### Поток данных (сетевой цикл)

```
Клиент: ввод (мышь/клавиатура)
   └─> CommandPacket (TCP)
          └─> серверный тик (фиксированный тикрейт):
                системы симуляции над авторитетной Scene
          └─> SnapshotPacket (TCP) ──> клиент
                  └─> интерполяция ──> рендер (слой beng-client)
```

- Транспорт: **TCP для всего** (команды, снапшоты, чат, лут).
- Команды — это ввод игрока, не игровое состояние: клиент не симулирует,
  сервер не доверяет клиенту.
- Тики сервера и кадры клиента независимы; интерполяция сглаживает разницу.

### Раскладка по слоям (кто чем владеет)

| Сущность | Где живёт | Комментарий |
|----------|-----------|-------------|
| `Scene`, `Entity`, пулы, `ISystem` | beng-core | есть |
| `TransformComponent`, `TransformSystem` | beng-core | есть |
| `CameraComponent`, `MeshRenderComponent` (слои `RenderLayer`) | beng-client | рендер-представление; **вся отрисовка — только через Scene/RenderSystem** (см. BENG.md) |
| `ResourceManager` (кеш ISaveLoadable: меш/текстура/звук/данные) | blib-core | кеш по ключам, dedup по содержимому, refcount-доступ (сделан — см. RESOURCE_MANAGER.md) |
| `UnitComponent`, `InventoryComponent`, `SkillComponent` | gravelands-common | определения, не логика |
| `MovementSystem`, `CombatSystem`, `LootSystem` | gravelands-server-core | только сервер |
| `InputSystem`, `CameraFollowSystem`, `InterpolationSystem` | gravelands-client-core | только клиент |
| формулы урона, таблицы статов | gravelands-common | нужны серверу и UI клиента |
| тайловый мир (статика) | gravelands-common + gravelands-server-core | ECS — только динамика |

---

## 🖥️ Эдитор (один на все игры, плагин-модель)

Как в Unity/Unreal: **единый `beng-editor.exe`** создаёт и правит сцены любой
игры, игра подключается к нему как плагин. Никаких эдиторов на игру нет.

```
beng-editor.exe (единственный эдитор, аналог UnrealEditor.exe — готов,
                 оба режима доставки плагина; НЕ знает ни одной игры —
                 только контракт GameModuleFunctions, см. ниже)
   ├── EditorApplication (beng-editor-core.dll: окно, вьюпорт, панели, раскладка,
   │     сцена с движковыми типами, gizmo, undo/redo; хуки хоста on*();
   │     initialize/shutdown — ВИРТУАЛЬНЫЕ: плагин-контракт зовёт их через
   │     базовый указатель — диспетчеризация обязана попадать в override хоста)
   ├── плагин игры (gravelands-plugin): GravelandsEditorHost + игровой модуль —
   │     единственная точка входа bengGetGameModule() (extern "C", глобальный
   │     символ; этап 1 — статический линк (gravelands_plugin_type=STATIC),
   │     этап 2 — gravelands.dll через LoadLibrary/GetProcAddress (SHARED) —
   │     exe игру не знает; id игры = имя DLL = значение --game)
   └── Play mode: хостит gravelands-client-core in-process + gravelands-server-core in-process
         (связь между ними — loopback TCP, сетевой код-путь остаётся настоящим)
```

- **Shared-сборка (сделано, 2026-09-28):** blib и beng собираются как DLL
  (`blib_build_type=blib_build_dynamic`, `beng_build_type=beng_build_dynamic`) —
  один `GlobalAllocator`/`Console`/реестры на процесс. Экспорт: модуль — `dllexport`
  (`blib_*_export`/`beng_export`, PRIVATE в CMake) + `WINDOWS_EXPORT_ALL_SYMBOLS`
  (template-инстанции, неразмеченные символы); потребители — без `dllimport`
  (inline-члены классов не существуют в DLL — импортов нет; линкер резолвит
  напрямую через импорт-таблицу), статические данные — `__blib_data_api`/
  `__beng_data_api` (dllimport). Все exe/DLL — в общий `bin/` (Windows ищет
  DLL рядом с exe). Статическая сборка — дефолт и режим отладки.

- **`EditorApplication` (сделано, 2026-09-28)** — игра-агностичный каркас:
  окно/FBO вьюпорта/орбитальная камера, ImGui (контекст, WndProc-хук, кадр),
  ECS-сцена с движковыми типами beng-client и системами Transform→Animation→Render,
  панели вьюпорта/консоли, горячие клавиши, раскладка панелей по зонам.
  Хост (игра/инструмент) наследует каркас и переопределяет хуки
  `onInitialize/onInput/onSceneWillUpdate/onSceneDidUpdate/onUi/onEscapePressed`
  — это зародыш `IGameModule`: на DLL-этапе хук-интерфейс станет границей
  эдитор ↔ игра. Хосты: `model_viewer` (инструмент) и `GravelandsEditorHost`
  (плагин игры).
- **Плагин Gravelands (этап 1, сделано 2026-09-28):** `gravelands-plugin`
  хостит общий `gravelands::World` в сцене эдитора (та же сцена, что у
  клиента — эдитор правит игру), подключает `scene_save`/`scene_load`,
  дебаг-свет и hot-reload шейдеров. Статический линк и DLL используют
  одну точку входа `bengGetGameModule()` (игровой модуль, контракт
  `GameModuleFunctions`) — переход на DLL меняет способ доставки, не код
  плагина; статический режим остаётся удобным режимом отладки и после
  DLL-этапа.
- **Контракт «эдитор ↔ игра» (`GameModuleFunctions`, сделан 2026-09-30):**
  `beng/editor/gameModule.h` — POD-структура (стабильный ABI): стабильный
  id игры `getGameName()` (= имя DLL = значение `--game=<id>`), фабрики
  хоста `createEditorHost()`/`destroyEditorHost()` (память —
  GlobalAllocator, конкретный тип известен только плагину), версия
  контракта (сверка — fatal). Точка входа — `bengGetGameModule()`
  (extern "C", глобальный символ, стабильное имя `gameModuleEntryName`).
  Загрузчик `beng-editor/main/main.cpp` игра-агностичен: DLL-режим —
  `LoadLibrary("<id>.dll")` + GetProcAddress; статический — заголовок
  игры и прямой вызов (дефайны `BENG_EDITOR_STATIC_GAME_HEADER`/
  `BENG_EDITOR_GAME_DLL` + `BENG_EDITOR_DEFAULT_GAME_NAME` ставит игра
  через CMake); без опций — игра по умолчанию, без игры — пустая сцена.
  Имя DLL плагина = id игры (CMake `OUTPUT_NAME`; дефолтное имя таргета
  `gravelands-plugin` эдитор не нашёл бы).
- **Inspector строит поля из дескрипторов рефлексии beng-core** — эдитор
  не знает типов игры. **Сделано (2026-09-28):** `SceneHierarchyPanel`
  (сущности + компоненты + выбор + Create/Delete) и `InspectorPanel`
  (поля по рефлексии + Remove/Add Component) — каркасные панели
  beng-editor-core; selection — сервис каркаса; **gizmo-манипулятор
  Unity-стиля** (W/E/R: Translate/Rotate/Scale, драг за стрелку/
  окружность, отрисовка в каркасе) и **выбор кликом** (ray-picking
  через хук `onViewportClick`); **undo/redo** (`CommandHistory` в
  beng-core: поля/TRS/сущности/компоненты; Ctrl+Z / Ctrl+Shift+Z /
  Delete). Отражаются Transform (position/scale/parent) и свет.
- **Сцены сериализуются** в текстовый версионированный формат (JSON,
  `sceneSaveFormat.h`), типы ссылаются по стабильным именам из реестра —
  не `typeid`, не адрес.
- **Play mode (PIE)** без Process API: оба ядра хостятся in-process,
  принцип «сервер — отдельный процесс» соблюдается в продакшене тонкими exe.

### Сложности плагин-модели (осознанные и зафиксированные)

1. **Дублирование глобального состояния.** Статический blib/beng-core в двух
   модулях = два `GlobalAllocator`, две `Console`, два реестра типов.
   **Сделано (2026-09-28):** blib и beng собираются как shared DLL
   (`blib_build_type`/`beng_build_type` = `*_build_dynamic`) — см. выше.
2. **Реестр типов — только явная регистрация.** Никаких static-local ID
   и `typeid()` через границу DLL. Игра экспортирует `registerGameTypes(...)`.
   **Сделано:** глобального реестра нет — типы регистрируются явно на
   сцену (`scene.registerComponentType<T>()`) по стабильному имени
   (`T::componentTypeName`, литерал из класса — работает из любого модуля);
   коллизия имени в сцене — fatal, guard — `isRegisteredComponentType<T>()`.
   Осталось: `registerGameTypes` для эдитора (имена + рефлексия).
3. **Стабильный интерфейс эдитор ↔ игра.** **Сделано (2026-09-30):**
   контракт `GameModuleFunctions` + точка входа `bengGetGameModule`
   (см. выше «Контракт "эдитор ↔ игра"»); хук-интерфейс каркаса
   `EditorApplication::on*()` + `initialize`/`shutdown` — **виртуальные**
   (плагин-контракт зовёт через базовый указатель: без virtual
   диспетчеризации impl хоста не создался бы — AV на старте; грабли
   задокументированы в BENG.md). Полноценный `IGameModule` с жизненным
   циклом PIE (команды play/stop от каркаса) — остаётся фьюче-этапом
   эдитора.
4. **Экспорт символов: сделан (2026-09-28).** `__blib_api`-семейство и
   `__beng_api` — настоящие `dllexport` при сборке модуля; потребители —
   без декораций (auto-import линкера), данные — через data-макросы.
   `WINDOWS_EXPORT_ALL_SYMBOLS` покрывает template-инстанции и пропущенные
   классы. Публичное API дополнительно размечать по мере надобности.
5. **Версионированный формат сцен** и стабильные имена типов — сцена должна
   переживать рефакторинги.
6. **Владение памятью через границу** — решается единым `GlobalAllocator`
   (shared blib) + явными правилами владения.
7. **Жизненный цикл DLL**: порядок init/shutdown, запрет висячих указателей
   после выгрузки.
8. **Сборка/отладка**: эдитор находит нужную game.dll (Debug/Release).

**Поэтапное внедрение:** пункты 1, 2, 5 закладываются сразу (shared core,
явная регистрация, версионированный формат). **Этап 1 (статический линк) —
сделан (2026-09-28):** плагин игры линкуется в эдитор через CMake-опцию
`gravelands_plugin_type` (STATIC — дефолт/отладка, SHARED — DLL-этап).
**Этап 2 (DLL) — сделан (2026-09-28), доведён до рабочего запуска
(2026-09-30):** shared blib/beng, gravelands.dll с единственной точкой
входа (игровой модуль, контракт `GameModuleFunctions`), загрузчик в
`beng-editor.exe` (LoadLibrary/GetProcAddress, гашение парной фабрикой,
FreeLibrary); починено имя DLL (= id игры, `OUTPUT_NAME` — до этого
эдитор искал несуществующий файл) и виртуализированы
`initialize`/`shutdown` каркаса. Код плагина один и тот же — меняется
только способ доставки; оба режима проверены запуском эдитора.

---

## 🗂️ Целевая структура каталогов

```
src/
├── blib/            # сервисы (как есть)
├── beng/
│   ├── core/        # beng-core
│   ├── components/  # движковые компоненты (Transform — есть)
│   ├── systems/     # движковые системы (TransformSystem — есть)
│   ├── test/        # юнит-тесты beng (beng_test_*, BUILD_TESTS)
│   ├── test_ecs/    # демо/Smoke ECS-ядра (есть)
│   ├── client/      # beng-client (есть)
│   ├── server/      # beng-server (есть, 2026-09-30 — см. SERVER.md)
│   └── editor/      # beng-editor-core: EditorApplication + панели (есть);
│                    # beng-editor: единый exe эдитора, main/ (есть)
├── misc/gravelands/         # игра Gravelands (диаблоид)
│   ├── common/      # gravelands-common (lib, есть)
│   ├── world/       # gravelands-world: мир World (ECS-сцена + контент, есть) —
│   │                # общий для клиента и эдитора
│   ├── plugin/      # gravelands-plugin: игровая сторона эдитора (хост +
│   │                # фабрика; этап 1 — статика, этап 2 — gravelands.dll)
│   ├── client/      # gravelands-client-core (lib) + gravelands-client (тонкий exe, есть)
│   └── server/      # gravelands-server-core (lib) + gravelands-server (тонкий exe, есть)
├── misc/model_viewer/   # отдельный 3D-инструмент (хост EditorApplication), не трогать до этапа 3D
├── vochat/          # отдельный инструмент, не часть игровой архитектуры
└── thirdparty/
```

---

## ❓ Таблица «вопрос → слой»

| Вопрос | Ответ (слой) |
|--------|--------------|
| Как нарисовать спрайт / отправить пакет / аллоцировать память? | blib |
| Как устроен игровой цикл, тикрейт, ECS, ресурсы? | beng |
| Какие у монстра статы и формулы урона? | gravelands |
| Кто авторитет в мире: клиент или сервер? | сервер (gravelands-server-core) |
| Кто знает, как выглядит эдитор (окно, панели, вьюпорт)? | beng-editor (`EditorApplication`) |
| Кто знает, какие типы есть у игры? | плагин игры (gravelands-common) |
| Кто владеет while-циклом процесса? | тонкий exe (или эдитор в PIE) |

---

## 🗺️ Roadmap

1. **beng-core: Application + тикрейт + интерфейсы модулей.** Рефлексия
   компонентов и явная регистрация типов. Shared-сборка blib/beng-core.
2. **beng-server + gravelands-server-core**: TCP-сервер, снапшоты, WorldManager;
   простейшая симуляция (движение юнитов, сессия игрока). **beng-server
   сделан (2026-09-30):** `ServerApplication` (тикрейт + сетевой цикл),
   `NetworkServer` (слоты, backpressure), `ReplicationManager`
   (рефлексивная репликация: зеркала → полные/дельта/destroy-снапшоты,
   хеш-сверка схем), `WorldManager` (save/load), `IServerGame` +
   тест-группы `replication`/`server` (в т.ч. loopback-интеграция) —
   см. SERVER.md. **Миграция gravelands на beng-server выполнена
   (2026-09-30, фаза 3)** — см. п.3.
3. **beng-client + gravelands-client-core**: рендер-ECS (спрайтовая
   изометрия на базе isometricTileset), подключение клиента,
   интерполяция. **Сделано (2026-09-30, фаза 3):** клиентская оболочка
   `ClientApplication` + `IClientGame` (см. CLIENT.md); **движковая
   интерполяция зеркал** (`ReplicationClientState` + флаг
   `FunctionField::interpolated`); **миграция gravelands одним заходом**:
   `ServerCore`/`ClientCore` — тонкие обёртки над `ServerApplication`/
   `ClientApplication`, игровая логика — `GravelandsServerGame`/
   `GravelandsClientGame` (IServerGame/IClientGame), `protocol.h` ужат
   до кодека команд, рукописная сеть удалена, PIE без правок
   `PieSession`; **local-server mode** (одиночная игра — in-process
   сервер клиента); **client-side prediction игрока — движковая фича
   оболочки (2026-10-01)** — `ClientPrediction`/`getLatestFieldSample` +
   хуки `buildPlayerCommand`/`applyPlayerCommand` (см. CLIENT.md).
   AudioModule — TODO. *(ResourceManager сделан раньше
   плана — в blib-core: кеш ISaveLoadable с dedup и refcount, см.
   RESOURCE_MANAGER.md.)*
4. **Геймплей-петля диаблоида**: бой, лут, скиллы (gravelands-common/server),
   UI (ImGui), звук.
5. **beng-editor (единый эдитор)**: `EditorApplication` (каркас — **сделан**,
   см. BENG.md; первый хост — model_viewer) + тонкий `beng-editor.exe` —
   **сделан**; Inspector через рефлексию + Hierarchy + selection-сервис +
   gizmo-манипулятор (W/E/R) + выбор кликом + undo/redo — **сделано**;
   плагин игры (этап 1 — статический линк, **сделан**; этап 2 — DLL,
   **сделан**; контракт `GameModuleFunctions` + игра-агностичный
   загрузчик `--game=<id>` — **сделан 2026-09-30**, оба режима проверены
   запуском); **PIE — сделан (2026-09-28)**: `PieSession`
   (in-process хостинг сервера + клиента, loopback TCP — настоящий
   сетевой путь), Play/Stop в эдиторе; докинг, наконечники/плоскости
   gizmo — TODO.
6. **Опционально**: UDP-канал, hot-reload плагина. *(3D-меши с изокамерой —
   сделано раньше плана: `MeshRenderComponent` + слои `RenderLayer`, единый
   ECS-рендер; контент-плейсхолдеры из obj_spider — опционально.)*

---

## 🧹 Миграция текущего кода

**Выполнено (2026-09):**

- Таргет `beng` → `beng-core`; демо переехало в `beng/test_ecs/` (линкует beng-core).
- `misc/game` → `misc/gravelands`: игра именуется Gravelands, namespace `gravelands`,
  инклюды `<gravelands/...>` (корень `src/misc`).
- `server/engine/` (дубль ECS с битыми инклюдами) удалён; вместо него —
  `gravelands-server-core` (ServerCore: beng::Scene + аккумулятор фикс. 60 Гц)
  + тонкий `gravelands-server`.
- `client` переписан на паттерн «lib + тонкий exe»: `gravelands-client-core`
  (ClientCore с pimpl: окно/таргет/камера/тайлы) + тонкий `gravelands-client`;
  `printf` заменён на Console; IsometricTileset пересобран на Mesh
  (старый код опирался на уже удалённые Romb/Rectangle).
- `misc/misc` (legacy Types/ObjectPool/LinkedList) удалён.
- **`EditorApplication` выделен в beng-editor (2026-09-28):** игра-агностичный
  каркас эдитора (окно/FBO/камера/ImGui/сцена/раскладка + хуки хоста)
  вынесен из `ViewerCore`; вьювер стал первым хостом каркаса (инструмент,
  не эдитор); `gravelands-editor` из плана убран — эдитор один на все игры.
- **`beng-editor` стал exe (2026-09-28):** таргет переименован в
  `beng-editor-core` (каркас), добавлен тонкий `beng-editor` (main/) —
  единый эдитор запускается с пустой сценой и движковыми типами;
  модель_viewer/тесты переведены на `beng-editor-core`.
- **Мир Gravelands вынесен в `gravelands-world` (2026-09-28):** `World`
  (ECS-сцена + контент + scene_save/load + дебаг-свет) — общий для
  клиента и эдитора; `ClientCore` ужат до окна/камеры/ввода/презентации.
- **Плагин Gravelands в эдиторе, этап 1 (2026-09-28; контракт — 2026-09-30):**
  `gravelands-plugin` (GravelandsEditorHost + игровой модуль, точка входа
  `bengGetGameModule()`) линкуется в `beng-editor.exe` статически
  (`gravelands_plugin_type=STATIC`, дефайн
  `BENG_EDITOR_STATIC_GAME_HEADER`) — единый эдитор правит мир Gravelands;
  на DLL-этапе та же точка входа — экспорт gravelands.dll.
- **Рефлексия + Inspector (2026-09-28):** `componentReflection.h` в beng-core
  (FieldValue/FunctionField/ComponentTypeDescriptor), дескрипторы в сцене
  per-тип; панели `SceneHierarchyPanel`/`InspectorPanel`; `Scene::reset()`
  (сброс данных без сноса реестра) — scene_load хостов; мир привязывается
  к сцене хоста (`World::initialize(scene)`), базовый рендер-пайплайн вешает
  хост, мир добавляет только свои системы (тень/свет).
- **Gizmo + undo/redo (2026-09-28):** gizmo-манипулятор Unity-стиля в каркасе
  (W/E/R, стрелки/окружности, драг за стрелку, отрисовка LineRenderer'ом
  каркаса), `CommandHistory` в beng-core (поля/TRS/сущности/компоненты;
  Ctrl+Z / Ctrl+Shift+Z / Delete), type-erased add/removeComponent в Scene,
  кнопки панелей + история; плагин очищает историю на scene_load
  (`World::setSceneResetCallback`).
- **DLL-этап плагина (2026-09-28):** shared-сборка blib/beng
  (dllexport + WINDOWS_EXPORT_ALL_SYMBOLS, data-макросы, общий bin/),
  gravelands.dll с extern "C"-фабриками (create/destroy), загрузчик в
  beng-editor.exe; обе конфигурации (STATIC/SHARED) собираются и проходят
  тесты. Правки для shared: `RWLocker` помечен `__blib_system_api`,
  blib-graphics линкует Assimp сам, export-дефайны blib — PRIVATE.
- **Сетевой цикл + PIE (2026-09-28):** дочинен blib-network (NetworkError,
  WouldBlock-контракт, Address — value-тип, тест-группа `network`);
  бинарный протокол gravelands (Command/Snapshot/Welcome + MessageFramer,
  тест-группа `protocol`); `UnitComponent`; сервер — команды→тики→снапшоты
  (`NetworkServer`/`MovementSystem`); клиент — интерполяция зеркал юнитов
  (`NetworkClient`, офлайн-фолбэк); PIE — `PieSession` + Play/Stop в
  `beng-editor.exe` (клиент в PIE без своего ImGui-контекста,
  `EditorApplication::setEditorInputEnabled`).
- **Сетевой feel (2026-09-29):** Nagle выключен (`setTcpNoDelay` в
  blib-network); интерполяция клиента — кольцевой буфер снапшотов +
  фиксированная задержка рендера по tick number; слив ВСЕХ снапшотов за
  кадр; **client-side prediction игрока** (локальная интеграция ввода
  формулой сервера, реконсиляция снапом при расхождении > порога);
  тикрейт 30→60 Гц; PIE-перф (уменьшенное окно клиента, пост-пасс off,
  debug-лог кадрового времени).
- **Контракт `GameModuleFunctions` (2026-09-30):** игровой модуль
  (`beng/editor/gameModule.h`: id игры + фабрики хоста + версия контракта),
  единственная точка входа плагина — `bengGetGameModule()`; загрузчик
  `beng-editor` игра-агностичен (заголовок/имя игры — дефайны CMake,
  выбор — `--game=<id>` + `BENG_EDITOR_DEFAULT_GAME_NAME`); имя DLL =
  id игры (`OUTPUT_NAME`); `initialize`/`shutdown` каркаса — virtual
  (плагин-контракт зовёт через базовый указатель). Оба режима
  (STATIC/SHARED) проверены запуском эдитора + smoke-сценариями
  `--game`. Удалён рудимент `blib/graphics/imageref.h` (не используется —
  в ResourceManager другой интерфейс).
- **beng-server + рефлексивная репликация (2026-09-30):** таргет
  `beng-server` (ServerApplication/NetworkServer/ReplicationManager/
  WorldManager/IServerGame); в beng-core — модуль репликации
  (`replicationSchema` — wire-типы + FNV-1a-хеши схем полей;
  `replicationCodec` — FieldValue-кодек, Welcome, полные/дельта/destroy-
  снапшоты фиксированных капов; `replicationFramer` — фрейминг потока;
  `replicationClientState` — клиентское зеркало по рефлексии);
  `FunctionField::replicated` + реплицируемые поля Transform (position/
  scale; parent — нет); `Scene::createEntityWithId` (зеркала сохраняют
  серверные ID). Тест-группы `replication` (схема/кодек/зеркало) и
  `server` (менеджер/WorldManager/loopback-интеграция TCP) — 36/36 в
  обеих сборках. gravelands пока на старой сети — миграция в фазе 3.
- **beng-client + миграция gravelands (фаза 3, 2026-09-30):** клиентская
  оболочка `ClientApplication` + `IClientGame` (окно/FBO/камера/пост-пасс/
  ImGui/сеть/headless, local-server mode — см. CLIENT.md); **движковая
  интерполяция зеркал** (флаг `FunctionField::interpolated`,
  `ReplicationClientState::renderMirror` — lerp с постоянным лагом в
  тиках) и **события зеркал** (take-буферы spawn/destroy);
  `Scene::setNextEntityId` + `serverEntityIdBase` — разделение
  ID-пространств (серверные сущности с высокой базы, клиентский
  локальный контент — низкие ID). Gravelands мигрирован одним заходом:
  `ServerCore`/`ClientCore` — тонкие обёртки, игровая логика —
  `GravelandsServerGame`/`GravelandsClientGame`, `protocol.h` — кодек
  команд, рукописная сеть удалена, PIE без правок `PieSession`;
  local-server — одиночный запуск клиента сам хостит сервер.
   Исправлено по ходу: MSVC материализует тернарник в init-списке окна
   → деструктор временного `RenderWindow` удалял GL-контекст (фабрика
   «return prvalue», см. CLIENT.md); коллизия ID зеркал с локальным
   контентом клиента (ID-пространства, см. SERVER.md). 36/36 тестов в
   обеих сборках, smoke: клиент соло (local-server), пара сервер+клиент,
   эдитор STATIC/SHARED.
- **Предикшн игрока — движковая фича оболочки (2026-10-01):**
  client-side prediction вынесен из `GravelandsClientGame` в
  `ClientApplication` (beng-client): чистая логика — `ClientPrediction`
  (`clientPrediction.h`; старт от серверного сэмпла, снап при
  расхождении > порога, штатный лаг доверяем предсказанию), новейший
  серверный сэмпл — `ReplicationClientState::getLatestFieldSample`
  (резолв по стабильным именам типа/поля; интерполированное значение
  сцены для сверки не годится — отстаёт на `interpolationDelayTicks`).
  Игра даёт только кодек команды и формулу интеграции: новые хуки
  `IClientGame::buildPlayerCommand`/`applyPlayerCommand`; оболочка шлёт
  команды с dedup по байтам и перекрывает зеркало игрока после
  renderMirror; сброс — в переходе ready→lost. `predictionEnabled`/
  `predictionSnapDistance` — в `ClientApplicationParams`. Группа тестов
  `client_prediction` (6 кейсов), 37/37 тестов, smoke: клиент соло,
  эдитор STATIC.

**Осталось (по roadmap):**

- `gravelands-common` — вырастет из header-only в lib с общими компонентами/пакетами.
- `misc/model_viewer` — не трогать: станет основой 3D-ветки на этапе 6.
- `vochat` — отдельный инструмент, вне игровой архитектуры.
