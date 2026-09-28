# Архитектура проекта Stuff — слои, движок beng, референсная игра

> Документ закрепляет разделение ответственностей между слоями проекта
> (`blib` → `beng` → `game`), требования к движку beng и целевую
> архитектуру референсной игры (диаблоид). Статус: план, не код.
> Обновляется вместе с развитием кодовой базы.
> Сверено: 2026-09-28

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
| `beng-core` | общее ядро: ECS, Application, тикрейт, интерфейсы модулей, рефлексия, сетевой фрейминг | blib-core, blib-system |
| `beng-client` | клиентская среда: RenderModule, InputModule, AudioModule, рендер-ECS, сетевой клиент | beng-core, blib-graphics, blib-sound |
| `beng-server` | headless-сервер: NetworkServer, WorldManager (save/load), снапшоты | beng-core, blib-network |
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
- `beng-server` работает headless: без рендера, звука и ввода.
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
- Сетевая репликация компонентов на базе рефлексии.
- 3D-контент с изометрической камерой — **начато**: `MeshRenderComponent` +
  `RenderLayer` реализованы, весь мир рисуется единым путём через
  `RenderSystem` (см. BENG.md, «beng-client»); свет в ECS — позже.
- Hot-reload игровой DLL в эдиторе (не на старте).

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
сервер шагает симуляцию фиксированными тиками 30 Гц с аккумулятором
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
beng-editor.exe (единственный эдитор, аналог UnrealEditor.exe; есть —
                 с миром Gravelands через плагин, этап 1)
   ├── EditorApplication (beng-editor-core: окно, вьюпорт, панели, раскладка,
   │     сцена с движковыми типами; хуки хоста on*() — есть)
   ├── плагин игры (gravelands-plugin): GravelandsEditorHost + фабрика
   │     gravelandsCreateEditorHost() — единственная точка входа; этап 1 —
   │     статический линк (CMake gravelands_plugin_type=STATIC), этап 2 —
   │     та же фабрика становится экспортом gravelands.dll
   └── Play mode: хостит gravelands-client-core in-process + gravelands-server-core in-process
        (связь между ними — loopback TCP, сетевой код-путь остаётся настоящим)
```

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
  одну фабрику `gravelandsCreateEditorHost()` — переход на DLL меняет
  способ доставки, не код плагина; статический режим остаётся удобным
  режимом отладки и после DLL-этапа.
- **Inspector строит поля из дескрипторов рефлексии beng-core** — эдитор
  не знает типов игры. **Сделано (2026-09-28, первая версия):**
  `SceneHierarchyPanel` (сущности + компоненты + выбор) и `InspectorPanel`
  (поля по рефлексии) — каркасные панели beng-editor-core; selection —
  сервис каркаса; gizmo (маркер-оси + drag-перемещение по G). Отражаются
  Transform (position/scale/parent) и свет.
- **Сцены сериализуются** в текстовый версионированный формат (JSON,
  `sceneSaveFormat.h`), типы ссылаются по стабильным именам из реестра —
  не `typeid`, не адрес.
- **Play mode (PIE)** без Process API: оба ядра хостятся in-process,
  принцип «сервер — отдельный процесс» соблюдается в продакшене тонкими exe.

### Сложности плагин-модели (осознанные и зафиксированные)

1. **Дублирование глобального состояния.** Статический blib/beng-core в двух
   модулях = два `GlobalAllocator`, две `Console`, два реестра типов.
   Решение: **blib и beng-core собираются как shared DLL**
   (CMake уже умеет `blib_build_type=blib_build_dynamic`).
2. **Реестр типов — только явная регистрация.** Никаких static-local ID
   и `typeid()` через границу DLL. Игра экспортирует `registerGameTypes(...)`.
   **Сделано:** глобального реестра нет — типы регистрируются явно на
   сцену (`scene.registerComponentType<T>()`) по стабильному имени
   (`T::componentTypeName`, литерал из класса — работает из любого модуля);
   коллизия имени в сцене — fatal, guard — `isRegisteredComponentType<T>()`.
   Осталось: `registerGameTypes` для эдитора (имена + рефлексия).
3. **Стабильный интерфейс эдитор ↔ игра** (`IGameModule`): регистрация типов,
   хуки сериализации, жизненный цикл PIE. **Задел сделан:** хук-интерфейс
   `EditorApplication::on*()` + фабрика плагина `gravelandsCreateEditorHost()`
   (gravelands-plugin); на DLL-этапе фабрика станет экспортом gravelands.dll,
   а хук-интерфейс вырастет в `IGameModule`.
4. **Экспорт символов:** `__blib_api`/`__beng_api` становятся настоящими
   `dllexport/dllimport`, всё публичное API помечается.
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
`gravelands_plugin_type` (STATIC — дефолт/отладка, SHARED — DLL-этап); код
плагина один и тот же, меняется только способ доставки.

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
│   ├── server/      # beng-server (будущее)
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
   простейшая симуляция (движение юнитов, сессия игрока).
3. **beng-client + gravelands-client-core**: RenderModule/InputModule/AudioModule,
   рендер-ECS (спрайтовая изометрия на базе isometricTileset);
   подключение клиента, интерполяция. *(ResourceManager сделан раньше
   плана — в blib-core: кеш ISaveLoadable с dedup и refcount, см.
   RESOURCE_MANAGER.md.)*
4. **Геймплей-петля диаблоида**: бой, лут, скиллы (gravelands-common/server),
   UI (ImGui), звук.
5. **beng-editor (единый эдитор)**: `EditorApplication` (каркас — **сделан**,
   см. BENG.md; первый хост — model_viewer) + тонкий `beng-editor.exe` —
   **сделан**; Inspector через рефлексию + Hierarchy + selection-сервис +
   gizmo (маркер + drag-перемещение) — **сделано** (первая версия);
   плагин игры (этап 1 — статический линк через фабрику, **сделан**;
   этап 2 — DLL); докинг, полный gizmo-манипулятор, undo/redo, PIE
   (in-process хостинг, loopback TCP).
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
  `gravelands-server-core` (ServerCore: beng::Scene + аккумулятор фикс. 30 Гц)
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
- **Плагин Gravelands в эдиторе, этап 1 (2026-09-28):** `gravelands-plugin`
  (GravelandsEditorHost + фабрика `gravelandsCreateEditorHost()`) линкуется
  в `beng-editor.exe` статически (`gravelands_plugin_type=STATIC`, дефайн
  `BENG_EDITOR_GRAVELANDS_STATIC`) — единый эдитор правит мир Gravelands;
  на DLL-этапе та же фабрика станет экспортом gravelands.dll.
- **Рефлексия + Inspector (2026-09-28):** `componentReflection.h` в beng-core
  (FieldValue/FunctionField/ComponentTypeDescriptor), дескрипторы в сцене
  per-тип; панели `SceneHierarchyPanel`/`InspectorPanel`; `Scene::reset()`
  (сброс данных без сноса реестра) — scene_load хостов; мир привязывается
  к сцене хоста (`World::initialize(scene)`), базовый рендер-пайплайн вешает
  хост, мир добавляет только свои системы (тень/свет).

**Осталось (по roadmap):**

- `gravelands-common` — вырастет из header-only в lib с общими компонентами/пакетами.
- `misc/model_viewer` — не трогать: станет основой 3D-ветки на этапе 6.
- `vochat` — отдельный инструмент, вне игровой архитектуры.
