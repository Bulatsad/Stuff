# Архитектура проекта Stuff — слои, движок beng, референсная игра

> Документ закрепляет разделение ответственностей между слоями проекта
> (`blib` → `beng` → `game`), требования к движку beng и целевую
> архитектуру референсной игры (диаблоид). Статус: план, не код.
> Обновляется вместе с развитием кодовой базы.

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
5. **Эдитор — один на все игры** (плагин-модель, как Unity/Unreal): игра
   поставляется эдитору как библиотека/DLL, сам эдитор игру не знает.
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
| `beng-client` | клиентская среда: RenderModule, InputModule, AudioModule, ResourceManager, рендер-ECS, сетевой клиент | beng-core, blib-graphics, blib-sound |
| `beng-server` | headless-сервер: NetworkServer, WorldManager (save/load), снапшоты | beng-core, blib-network |
| `beng-editor` | каркас эдитора: докинг, Hierarchy, Inspector, вьюпорт, gizmo, selection, undo/redo | beng-client, ImGui |

### Требования

#### must (контракт, на который можно рассчитывать)

- `beng-core` предоставляет ECS-ядро (`Scene`, `ComponentPool`, `ISystem` —
  уже есть) и `Application` с frame-API: `initialize()` / `tick(dt)` / `shutdown()`.
- Симуляция идёт с **фиксированным тикрейтом**; рендер и ввод — с частотой кадра.
- `beng-core` предоставляет **рефлексию компонентов**: статический дескриптор
  типа (имя, список полей с доступом). Из неё растут Inspector, сериализация
  сцен, save/load мира и будущая сетевая репликация.
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
gravelands-editor.exe ──> beng-editor + gravelands-client-core + gravelands-server-core + gravelands-common
gravelands-common ──> beng-core ──> blib
```

- Core-библиотеки дают frame-API (`initialize` / `tick` / `shutdown`),
  **не владеют** while-циклом.
- Тонкий exe содержит только `main()`: создаёт приложение, крутит цикл,
  корректно гасит.
- Этот же паттерн позволяет эдитору хостить игру in-process: он сам вызывает
  `tick` ядер внутри своего ImGui-цикла.
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
| `gravelands-server-core` + `gravelands-server.exe` | library + exe | авторитетная симуляция: Movement, Combat, Loot, Session |
| `gravelands-client-core` + `gravelands-client.exe` | library + exe | представление: ввод, камера, интерполяция, рендер, UI |
| `gravelands-editor.exe` | exe | эдитор: правит сцены, Play mode (PIE) |

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
| `ResourceManager` (меш/текстура/звук) | beng-client | кеш по ключам |
| `UnitComponent`, `InventoryComponent`, `SkillComponent` | gravelands-common | определения, не логика |
| `MovementSystem`, `CombatSystem`, `LootSystem` | gravelands-server-core | только сервер |
| `InputSystem`, `CameraFollowSystem`, `InterpolationSystem` | gravelands-client-core | только клиент |
| формулы урона, таблицы статов | gravelands-common | нужны серверу и UI клиента |
| тайловый мир (статика) | gravelands-common + gravelands-server-core | ECS — только динамика |

---

## 🖥️ Эдитор (один на все игры, плагин-модель)

Как в Unity/Unreal: **единый эдитор** создаёт и правит сцены любой игры,
игра подключается к нему как плагин (DLL).

```
gravelands-editor.exe
   ├── beng-editor (каркас: панели, вьюпорт, gizmo, selection, undo/redo)
   ├── загружает gravelands.dll (плагин игры: регистрация типов + правила сериализации)
   └── Play mode: хостит gravelands-client-core in-process + gravelands-server-core in-process
        (связь между ними — loopback TCP, сетевой код-путь остаётся настоящим)
```

- **Inspector** строит поля из дескрипторов рефлексии beng-core — эдитор
  не знает типов игры.
- **Сцены сериализуются** в текстовый версионированный формат (pdl),
  типы ссылаются по стабильным именам из реестра — не `typeid`, не адрес.
- **Play mode (PIE)** без Process API: оба ядра хостятся in-process,
  принцип «сервер — отдельный процесс» соблюдается в продакшене тонкими exe.

### Сложности плагин-модели (осознанные и зафиксированные)

1. **Дублирование глобального состояния.** Статический blib/beng-core в двух
   модулях = два `GlobalAllocator`, две `Console`, два реестра типов.
   Решение: **blib и beng-core собираются как shared DLL**
   (CMake уже умеет `blib_build_type=blib_build_dynamic`).
2. **Реестр типов — только явная регистрация.** Никаких static-local ID
   и `typeid()` через границу DLL. Игра экспортирует `registerGameTypes(...)`.
3. **Стабильный интерфейс эдитор ↔ игра** (`IGameModule`): регистрация типов,
   хуки сериализации, жизненный цикл PIE.
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
явная регистрация, версионированный формат); на первом этапе плагин игры
можно линковать в эдитор статически — это меняет только CMake, не код игры,
и ускоряет появление эдитора.

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
│   ├── client/      # beng-client (будущее)
│   ├── server/      # beng-server (будущее)
│   └── editor/      # beng-editor (каркас, будущее)
├── misc/gravelands/         # игра Gravelands (диаблоид)
│   ├── common/      # gravelands-common (lib, есть)
│   ├── client/      # gravelands-client-core (lib) + gravelands-client (тонкий exe, есть)
│   ├── server/      # gravelands-server-core (lib) + gravelands-server (тонкий exe, есть)
│   └── editor/      # gravelands-editor (exe, будущее)
├── misc/model_viewer/   # будущая 3D-ветка (меши/анимация), не трогать до этапа 3D
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
| Кто знает, как выглядит эдитор? | beng-editor |
| Кто знает, какие типы есть у игры? | плагин игры (gravelands-common) |
| Кто владеет while-циклом процесса? | тонкий exe (или эдитор в PIE) |

---

## 🗺️ Roadmap

1. **beng-core: Application + тикрейт + интерфейсы модулей.** Рефлексия
   компонентов и явная регистрация типов. Shared-сборка blib/beng-core.
2. **beng-server + gravelands-server-core**: TCP-сервер, снапшоты, WorldManager;
   простейшая симуляция (движение юнитов, сессия игрока).
3. **beng-client + gravelands-client-core**: RenderModule/InputModule/AudioModule,
   ResourceManager, рендер-ECS (спрайтовая изометрия на базе isometricTileset);
   подключение клиента, интерполяция.
4. **Геймплей-петля диаблоида**: бой, лут, скиллы (gravelands-common/server),
   UI (ImGui), звук.
5. **beng-editor + gravelands-editor**: панели, вьюпорт, Inspector через
   рефлексию, сериализация сцен, PIE (in-process хостинг, loopback TCP).
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

**Осталось (по roadmap):**

- `gravelands-common` — вырастет из header-only в lib с общими компонентами/пакетами.
- `misc/model_viewer` — не трогать: станет основой 3D-ветки на этапе 6.
- `vochat` — отдельный инструмент, вне игровой архитектуры.
