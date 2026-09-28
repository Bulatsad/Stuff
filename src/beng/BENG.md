# BENG — среда выполнения (Bulat Engine)

> Слой: `beng`. Шпаргалка по устройству, инвариантам и граблям — чтобы не перечитывать исходники.
> Не дублирует правила проекта (`AGENTS.md`) и roadmap (`ARCHITECTURE.md`) — только ссылается на них.
> **Обновлять при любом изменении кода beng** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-09-27

---

## Назначение и границы

- **beng — среда выполнения игры, а не игра.** Правила, контент и геймплей живут в game-слое (`gravelands`).
- Таргеты и текущий статус:

| Таргет | Статус | Содержимое |
|--------|--------|------------|
| `beng-core` | реализован | ECS (`Scene`, `ComponentPool`, `ISystem`), `Transform`, `Time` |
| `beng-client` | зачаток | `SkinnedMeshComponent`, `AnimatorComponent`, `AnimationSystem`, `RenderSystem` |
| `beng-editor` | зачаток | панели ImGui на `IPanel` |
| `beng-server` | нет | headless-сервер (план — см. ARCHITECTURE.md) |

- Зависимости: `beng-core` → `blib-core` (+ `blib-system` транзитивно), без графики; `beng-client`/`beng-editor` → `blib-graphics`.
- Frame-API и «lib + тонкий exe»: ядро не владеет главным циклом (см. ARCHITECTURE.md).
- Что НЕ в этом доке: правила кодирования (AGENTS.md), план развития (ARCHITECTURE.md), детали графики и ассетов (см. `src/blib/graphics/GRAPHICS.md`).

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| ECS: сущности/компоненты/системы | `src/beng/core/scene.h` + `src/beng/core/impl/scene.inl` |
| Пул компонентов | `src/beng/core/componentPool.h` |
| Базовый интерфейс компонента (+ трейт имени типа) | `src/beng/core/icomponent.h` |
| Формат файла сохранения сцены (магия, версия, ключи) | `src/beng/core/sceneSaveFormat.h` |
| Интерфейс системы, приоритеты | `src/beng/core/system.h` |
| Лимиты и базовые типы ECS | `src/beng/config.h` |
| Время кадра | `src/beng/core/time.h/.cpp` |
| Transform + иерархия | `src/beng/components/transform.h/.cpp` |
| TransformSystem | `src/beng/systems/transformSystem.h/.cpp` |
| Скелетная модель (ref на слот кеша или owned `SkinModel`) | `src/beng/client/components/skinnedMeshComponent.h/.cpp` |
| Статический меш (ref на слот кеша или `Mesh` + слой рендера) | `src/beng/client/components/meshRenderComponent.h/.cpp` |
| Направленный свет сцены (`beng.DirectionalLight`) | `src/beng/client/components/directionalLightComponent.h/.cpp` |
| Эмбиент сцены (`beng.AmbientLight`) | `src/beng/client/components/ambientLightComponent.h/.cpp` |
| Blob-тень, следующая за моделью | `src/beng/client/components/blobShadowComponent.h/.cpp` |
| Анимация (состояние плейбека) | `src/beng/client/components/animatorComponent.h/.cpp` |
| Продвижение и применение анимации | `src/beng/client/systems/animationSystem.h/.cpp` |
| Применение света сцены в RenderContext | `src/beng/client/systems/lightSystem.h/.cpp` |
| Отрисовка сцены | `src/beng/client/systems/renderSystem.h/.cpp` |
| Контракт панели | `src/beng/editor/panels/iPanel.h` |
| Панели: иерархия/анимации/вьюпорт/опции/консоль | `src/beng/editor/panels/*` |
| Пример композиции приложения | `src/misc/model_viewer/core/viewerCore.cpp` |
| Тесты | `src/beng/test/src/impl/test*.cpp` (фреймворк `blib::test`, `BUILD_TESTS=ON`) |
| Интерфейсы сериализации/сравнения (blib-core) | `src/blib/core/{isaveable,iloadable,isaveloadable,icomparable,verifyHelper}.h` |
| Демо ECS | `src/beng/test_ecs/` |

---

## Инварианты и поток данных

### beng-core (ECS)

- Типы (`config.h`): `EntityID = buint64`, `invalidEntity = 0`; `ComponentType = buint8` — **локальный индекс типа в Scene** (бит в `ComponentMask`, слот в таблице пулов сцены), лимит **64 типа на сцену**.
- **Регистрация типов — только per-scene, через `scene.registerComponentType<T>()`.** Глобального реестра нет. Имя типа (`static constexpr const char* componentTypeName` — обязательно у каждого компонента, трейт `HasComponentTypeName<T>`) — стабильная **идентичность типа внутри сцены**: по нему тип регистрируется и резолвится всеми шаблонными методами сцены.
- **ИНВАРИАНТ: сущность не может существовать без `TransformComponent`.** Сцена регистрирует его САМА в конструкторе (всегда **typeId 0** — слот зарезервирован, кастомным типам остаётся `maxComponentTypes - 1`) и создаёт Transform каждой сущности в `Scene::createEntity()` (дефолтный TRS, `ownerScene` выставлен). Запрещено: `registerComponentType<TransformComponent>()` (fatal — имя уже зарегистрировано), `addComponent<TransformComponent>()` (fatal — компонент уже есть), `removeComponent<TransformComponent>()` (fatal — инвариант); доступ — только `getComponent`/`resolveComponent` (resolve игнорирует аргументы: компонент существует).
- **Коллизия имени в сцене — fatal** (повторная регистрация имени): guard-паттерн `if (!scene.isRegisteredComponentType<T>()) scene.registerComponentType<T>();` для идемпотентности. `getComponentPool<T>()`/`addComponent<T>()` без регистрации — fatal; `tryGetComponentPool<T>()`/`tryGetComponent<T>()` — не-fatal (nullptr, контракт для систем с опциональными компонентами).
- **Регистрация — строго до запуска цикла** (Scene не thread-safe). Горячий путь резолва `T → ComponentType` — линейный скан таблицы имён (`≤ maxComponentTypes` strcmp, без аллокаций); словарь `typeIdByName` (строка → ID, ключи через blib-аллокатор) используется при регистрации и в `Scene::load` (резолв имени типа из файла).
- Имена типов не копируются сценой: это литералы из классов компонентов, живущие весь процесс. Имена уникальны по конвенции (префикс модуля: `beng.*`, `gravelands.*`).
- **Владение:** `ComponentPool<T>` хранит компоненты через `PoolAllocator`; `destroyEntity`/`removeComponent` вызывают `~T()`. Компонент, владеющий ресурсом, освобождает его в деструкторе (пример: `SkinnedMeshComponent` → `SkinModel` или `ResourceRef` на слот кеша сцены).
- Указатель на компонент стабилен, пока компонент жив: `destroy` другого компонента двигает только `Entry` (swap-and-pop).
- **Итерация `ComponentPool`:** `begin()/end()` (+ const/`cbegin`/`cend`) обходят **только активные** компоненты (`IComponent::isActive == true`, флаг читается вживую на каждом шаге); порядок — dense, как у `getByIndex`; `*it` → `T&` (const-пул → `const T&`), владелец — `it.getEntityId()`. **Инвалидация:** `create()` (push_back) и `destroy()` (swap-and-pop) убивают живые итераторы — стандартная контейнерная семантика, `end()` пересчитывать. `size()` считает все компоненты **включая неактивные**; `getByIndex` возвращает неактивные как есть (тесты/диагностика), системы используют итераторы.
- Служебные контейнеры — через `StdAllocatorAdapter` (GlobalAllocator), без `::operator new`.
- **Системы:** `ISystem::update(scene, dt)`; сортировка по `getPriority()` (меньше — раньше); `Scene` не владеет системами; всё последовательно в main thread. Добавление системы в рантайме безопасно (флаг `systemsDirty` → пересортировка в `update`).
- `Scene` и `ComponentPool` **некопируемы/неперемещаемы** (держат указатель на собственный аллокатор).
- ID сущностей не переиспользуются: сохранённый ID валиден до конца жизни `Scene`.

### Transform

- **`TransformComponent` — обязательный компонент** (инвариант, см. выше): создаётся сценой автоматически в `createEntity`, снятие запрещено; в коде — только `getComponent`/`resolveComponent`.
- `TransformComponent` — локальные TRS + `parent`/`children` (EntityID), кеш `worldMatrix` с dirty-флагом (рекурсивно тянет мировую матрицу родителя).
- Мировая матрица — стандартная column-major TRS (`composeTrsMatrix`: трансляция в последней колонке `data[3][0..2]`, см. CORE.md «Конвенция матриц»); иерархия — `worldMatrix = parentMatrix * worldMatrix` (стандартное column-vector произведение, трансляция родителя переносится корректно).
- `setParent` отклоняет циклы и самого себя (warning, no-op); для смены родителя нужен `Scene`.
- `TransformSystem` (приоритет -100) просто вызывает `getWorldMatrix()` у всех — порядок обхода dense не важен.

### beng-client

- **ИНВАРИАНТ — единый ECS-рендер:** вся отрисовка мира идёт ТОЛЬКО через `Scene`: создать сцену → добавить объекты на сцену (сущности с рендер-компонентами) → отрисовать сцену (`scene.update()`, рисует `RenderSystem`). Прямые вызовы `renderTarget.draw(...)` вне RenderSystem запрещены. Пост-процессинг (пасс над FBO после сцены) и свет (`RenderContext`) — состояние презентации, не объекты мира (могут жить в приложении).
- `SkinnedMeshComponent` — модель в **двух режимах**: (1) `ResourceRef` на слот кеша сцены (`loadFromFile(path, rm)`, ключ = путь; уже «опечатанный» слот просто берётся ref'ом — разделение/dedup); (2) owned-фолбэк `SkinModel*` (GlobalAllocator + placement new) — standalone-`loadFromFile(path)` и verifyRoundTrip. `getModel()` возвращает АКТИВНУЮ модель (ref ?? owned); `unload()` снимает ref, очищает путь/RM-ссылку и выгружает owned. `loadFromFile(path, rm)` при неудаче **оставляет предыдущую модель нетронутой** (RM-работа до выгрузки) — это фолбэк-путь `onLoaded` при недоступном файле. **Мутация общего слота** (`loadSkinFromFile`/`loadAnimationsFromFile`/onLoaded в ref-режиме) пере-«опечатывает» его через `reCommit` (см. RESOURCE_MANAGER.md) — dedup-индекс снова отражает содержимое.
- `MeshRenderComponent` — меш в **двух режимах**: `ResourceRef` на слот кеша (ctor от ref'а / `setMeshResource`; для тайлов: `IsometricTileset::buildMeshInto` собирает прямо в слот — Mesh move-присваивание удалено) либо `Mesh` по значению (move-only, из билдера/примитива, напр. `Sphere::takeMesh()`). `getMesh()` возвращает АКТИВНЫЙ меш (ref ?? owned; owned пуст в ref-режиме и служит verify-фолбэком). `RenderLayer {Ground, Shadow, AlphaTested, Opaque}` задаёт порядок/поведение. ComponentPool не двигает компоненты — move-only член безопасен. Пулы RenderSystem берёт через `Scene::tryGetComponentPool` — сцены без статики (вьювер) работают как раньше.
- `AnimatorComponent` — **не владеет** аниматором: хранит указатель на `Animator` внутри `SkinModel` + `loop`/`poseDirty`. При выгрузке модели указатель обязан быть снят (`setAnimator(nullptr)` или уничтожение сущности) — иначе висячий указатель.
- `AnimationSystem` (приоритет -50): играет → `SkinModel::update(dt_ms)` (время в миллисекундах!); нециклическая доиграла → `pause()`; пауза + `poseDirty` (выбор клипа/скраб) → `update(0)` + сброс флага.
- `BlobShadowComponent` (на сущности-тени) + `BlobShadowSystem` (приоритет 50): тень следует за root-motion анимации цели — позиция кости (`Skelet::getBonePosition`, кандидаты «boneName»/«Hips»/«mixamorig:Hips»/подстрока hips·pelvis), мировая = `getWorldPosition() + rotate(getWorldScale()·pos, getWorldRotation())` (TRS-путь короче сборки матрицы; `Matrix::operator*` стандартен — column-major конвенция, см. CORE.md), тень ставится **строго под цель** `(world.x, groundOffset, world.z)` — blob-тень лежит под объектом, без световой проекции; запись в `TransformComponent` тени.
- `RenderSystem` (приоритет 100) — единственная точка отрисовки мира. Порядок слоёв фиксирован: `Ground` → `Shadow` (альфа-блендинг, запись глубины выключена) → `AlphaTested` → `Opaque` (включая `SkinnedMeshComponent`). Трансформации — из `TransformComponent::getWorldMatrix()`; без таргета — no-op; таргетом не владеет.
- **Свет сцены — компоненты движка:** `DirectionalLightComponent` (`direction` ИЗ источника, линейный `color`, `intensity`) и `AmbientLightComponent` (`color`, `intensity`) — зеркала `blib::graphics::DirectionalLight`/`AmbientLight`. `LightSystem` (приоритет 90, `setRenderTarget` как у RenderSystem) копирует **первый активный** компонент каждого типа в `RenderContext::directionalLight`/`ambientLight` перед отрисовкой; пулов/компонентов нет — rc-дефолты не трогаются (вьювер и чужие сцены работают как раньше). Ограничение (текущий RenderContext): по одному источнику каждого типа, дубликаты не проверяются; для deferred-шейдинга LightSystem — будущая точка сбора источников. Оба компонента сериализуемы — свет сохраняется/загружается со сценой и сверяется `strongCompare`.

### Кеш ресурсов сцены

- `Scene::getResources()` — член-`blib::resource::ResourceManager` (см. `src/blib/core/resource/RESOURCE_MANAGER.md`): кеш ISaveLoadable-ресурсов по ключам с dedup по содержимому (MD5 сериализованной формы) и refcount-доступом. **Сцена владеет кешем** — загрузка идёт через `scene.getResources()`, не через глобальный сервис.
- **Порядок жизни:** `Scene::clear()` (в деструкторе) уничтожает компоненты **до** деструктора кеша — ref'ы компонентов отпускаются раньше; внешние `ResourceRef`'ы обязаны умереть раньше сцены, иначе `~ResourceManager` фаталит (утечка ref'ов). В gravelands-клиенте сцена объявлена после окна/таргета — GL-контекст на момент уничтожения кеша жив (см. GRAPHICS.md «Владение GL»).
- Грабли: `construct` идемпотентен по ключу (второй вызов игнорирует ctor-аргументы, warning + ref на существующий); после `commit` дубликат возвращает ref на канонический слот — паттерн `rf = rm.commit(rf)`.

### Сериализация компонентов (ISaveLoadable)

- `IComponent` наследует `blib::core::ISaveLoadable` (save/load + строгое сравнение + verify). Чистые виртуальные `strongCompare(other, session)` и `verify()` реализует **каждый** конкретный компонент; `save()`/`load()` — только сериализуемые (default — `Unsupported`; `Scene::save` отклоняет такие типы с `SaveStatus::ComponentNotSerializable`). Базовые поля (`ownerId` + `isActive`) сравниваются через protected-помощник `IComponent::strongCompareBase(other)`.
- **Формат save:** JSON-объект через `JsonValue::writeTo(IOutputStream)`; `load()` — `JsonParser` из `IInputStream`. Ключи — именованные `constexpr` константы в `.cpp` компонента.
- **Границы сериализации:** `ownerId` не пишется (владение ставит пул, `Scene::load` восстанавливает при создании); `isActive` пишется. Контекстные указатели (`Scene*`, `Animator*`, `model*`) не сериализуются и сравниваются по null-состоянию → `verify()` standalone-компонента (без контекста) = true, компонента в сцене — false (строгая модель); восстановление контекста — `onLoaded(Scene&)`, см. «Сохранение/загрузка сцены».
- `TransformComponent` — сериализуется всё состояние, включая кеш `worldMatrix` (бит-в-бит контракт ISaveable). `BlobShadowComponent` — все собственные поля. `AnimatorComponent` — собственные поля + **плейбек**: save() снимает snapshot с живого аниматора (`clipName`/`timeMs`/`playing`; клип — по имени, не по индексу), load() кладёт его в «отложенное состояние», onLoaded применяет к перепривязанному аниматору (selectAnimation → setTime → play/pause; selectAnimation заодно восстанавливает `loop → cycled` — у клипа из файла cycled сбрасывается). strongCompare при обоих привязанных аниматорах сравнивает живой плейбек бит-в-бит — round-trip сцены воспроизводит состояние один в один.
- `MeshRenderComponent` — слой + меш: **делегирует** `blib::graphics::Mesh` (ISaveLoadable: вся CPU-геометрия + CPU-поля `Material`, включая битмап `diffuseImage`; GL-хендлы не сериализуемы — см. GRAPHICS.md, «Сериализация»). JSON-объём пропорционален геометрии (у примитивов/спрайтов малый).
- `SkinnedMeshComponent` — **полное содержимое АКТИВНОЙ модели** через `blib::graphics::SkinModel::toJson/fromJson` (скелет, веса, геометрия, материалы с битмапами диффуза, клипы анимации) + **путь загрузки**: `{path: <ключ RM | "">, model: <SkinModel JSON> | null, isActive}`; `load()` восстанавливает в активную модель — ref-слот, либо owned-фолбэк (аллоцируется при отсутствии: GlobalAllocator + placement new, standalone/verify-путь). Путь пишется только при живой модели (выгруженная не «воскресает»). `Bone::node` (aiNode*) — контекст Assimp, после восстановления nullptr (рантайм на нём не зависит).
- `verify()` сериализуемых компонентов — `blib::core::verifyRoundTrip<T>` (без RTTI: save → свежий `T()` → load → `strongCompare`); требует default-конструктор (есть у всех сериализуемых).
- `load()` валидирует **все** поля до применения — при ошибке (`LoadStatus::InvalidData`) состояние компонента не меняется. У `MeshRenderComponent`/`SkinnedMeshComponent` меши пересоздаются destroy + placement new (move-присваивание у `Mesh` удалено): GL-кэш старой геометрии невалиден после перезаписи CPU-данных, свежий меш перезапечётся в `draw()`. **Перезапись запечённого меша требует живого RenderContext.**
- Не прятать точку входа: в каждом компоненте `using blib::core::IStrongComparable::strongCompare;` — иначе 1-аргументная перегрузка базы скрывается 2-аргументной и `verifyRoundTrip` не компилируется.

### Сохранение/загрузка сцены (Scene::save / Scene::load)

- `Scene` наследует `blib::core::ISaveLoadable`: `save(IOutputStream&)` / `load(IInputStream&)` / `strongCompare` / `verify()`. Формат — `sceneSaveFormat.h`: магия `"JSON\0"` (5 байт) + JSON-документ `{format: "beng.scene", version: 1, nextEntityId, entities: [{id, components: [{type: <имя>, data: <JSON компонента>}]}]}`.
- **Типы — по стабильным именам** (`T::componentTypeName` → `typeIdByName`); незарегистрированный тип → `LoadStatus::ComponentTypeNotRegistered` (проверка ДО любых мутаций). Версия не совпала → `VersionMismatch`, магия — `UnknownFormat`, документ битый — `InvalidData`.
- **Инвариант Transform в файле:** у каждой сущности обязана быть ровно одна запись Transform — иначе `LoadStatus::InvalidData` (файлы, сохранённые до введения инварианта, загрузкой отвергаются; версия формата не менялась). Второй Transform при загрузке не создаётся: запись Transform грузится в компонент, авто-созданный `createEntity` (спец-кейс в фазе мутации по `transformTypeId`), бит маски уже установлен.
- **load() атомарен**: мутации начинаются только после полной пре-валидации; при ошибке в фазе мутации — откат (созданные сущности уничтожаются, пулы, созданные этим load'ом, удаляются, `nextEntityId` восстанавливается; реестр типов не трогается). load — только в пустую сцену (нет сущностей и `nextEntityId == 1`) → `SceneNotEmpty`.
- **ID сущностей** в файле последовательны (не переиспользуются) — создание в порядке массива со сверкой выданного ID; `nextEntityId` из файла обязан покрывать загруженные ID (иначе `InvalidData`).
- **Type-erased пулы:** при регистрации типа сцена сохраняет fn-таблицы на тип (фабрика пула, creator default-ctor'ом, getter, deleter, destroyer) — `load`/`save`/`strongCompare` работают без RTTI и compile-time `T`. Creator есть только у default-конструируемых типов (`if constexpr`); тип без default-ctor в файле → `InvalidData`. Лямбды без захвата — копируются между сценами (`copyComponentTypeRegistryFrom`, нужно `verify()`).
- **Двухфазная загрузка:** фаза 1 — создать сущности/компоненты и вызвать `component->load(поток)`; фаза 2 — `component->onLoaded(*this)` (virtual no-op в `IComponent`) у каждого компонента, внутри сущности — по возрастанию `ComponentType` (порядок регистрации). Восстановление контекста: `TransformComponent` → `ownerScene`; `SkinnedMeshComponent` → перезагрузка модели через `scene.getResources()` по сохранённому пути (разделение/dedup возвращается; при неудаче остаётся встроенное содержимое), затем **переприменение встроенного сохранённого содержимого к слоту** (`embeddedModel`, бит-в-бит — рантайм-правки материалов/клипов не теряются при перезагрузке из файла) + `reCommit` слота; `AnimatorComponent` → перепривязка `animator` к модели той же сущности + восстановление плейбека (клип/время/play — см. «Сериализация компонентов»). **Порядок регистрации — контракт зависимостей**: компонент, чей onLoaded зависит от чужого (`Animator` ← `SkinnedMesh`), обязан регистрироваться ПОСЛЕ источника.
- **Системы и кеш ресурсов не сериализуются**: системы перевешивает код игры после load; RM-слоты восстанавливаются через пути моделей (см. выше).
- `Scene::strongCompare` — nextEntityId + сущности (ID+маски) + компоненты в детерминированном порядке (dense-порядок сущностей, типы по возрастанию typeId). `Scene::verify()` — save → `MemoryStream` → свежая сцена с копией реестра типов → load → `strongCompare` (важно: сравнение компонентов идёт после onLoaded — контексты у обеих сцен не-null).
- Тесты — группа `sceneSave` (`testsceneSave.cpp`, подключена к CMake): магия/документ, полный roundtrip + иерархия, отказ по магии/типу/непустоте/несериализуемости, standalone-verify Transform.

### beng-editor (панели)

- Контракт `IPanel`:
  - панель рисует своё ImGui-окно (вызывающий уже открыл кадр);
  - позиция/размер — ответственность вызывающего (`SetNextWindowPos/Size` до `draw()`);
  - панель **не владеет данными**: получает указатели через `set*()`, nullptr = заглушка;
  - при выгрузке данных вызывающий обязан снять указатели.
- Панели: `HierarchyPanel` (скелет + выбранная кость), `AnimationPanel` (`AnimatorComponent`), `ViewportPanel` (`IRenderTarget` + `OrbitCamera`, ввод камеры, замер размера), `RenderOptionsPanel` (галочки), `ConsolePanel` (обёртка `ConsoleWindow`; единственный консюмер буфера вывода — двух таких панелей быть не должно), `DialogWindow` (модальный вопрос «продолжить/отменить» с колбэками; позиционирует себя сам, ID = заголовок).
- Пример композиции — `ViewerCore`: хранит панели, расставляет окна, читает геттеры (см. `misc/model_viewer`).
- Границы переиспользования: стек `blib-graphics` + ImGui + `beng-client`; `ViewportPanel` привязан к `OrbitCamera`, `ConsolePanel` — к синглтону `Console`.
- Будущее: докинг и регистрация панелей в `EditorApplication` (см. комментарий в `iPanel.h`).

### Сквозные маршруты (вьювер как референс)

- **Загрузка модели:** `SkinnedMeshComponent::loadFromFile` → Assimp → `SkinModel::loadFromAssimp` (скелет + аниматор + меши + материалы) → `AnimatorComponent::setAnimator(&model->getAnimator())` → панели привязываются (`setSkelet`, `setAnimatorComponent`).
- **Внешняя анимация:** `loadAnimationsFromFile` → `Animator::appendFromAssimp(scene, &skelet, имя_файла)` → для каждого нового клипа `Skelet::bindClipToSkeleton` → вьювер выбирает последний клип и запускает его.
- **Подмена скина:** `loadSkinFromFile` → `SkinModel::replaceMeshesFromAssimp` (атомарно; скелет и аниматор не трогаются) → внутри проверка `Skelet::isCompatibleWith` (имена + иерархия + inverse bind); при несовместимости `force = true` продолжает загрузку: перенос inverse-bind кандидата на текущий скелет (`adoptOffsetMatricesFrom` — меш рендерится как нативно скиннутый к текущему ригу, швы суставов не расходятся при анимации) + remap весов неизвестных костей на предков (или отброс с ренормализацией).
- **Кадр вьювера:** ввод → `time.tick()` → `scene.update(dt)` (Transform → Animation → Render в FBO) → отладочные слои (скелет/каркас) → UI в back buffer → `swapBuffers()`. Ресайз FBO измеряется в UI-кадре и применяется в начале следующего.

---

## Подводные камни / известные баги

- **Внешние анимации и FBX-декомпозиция.** У файла анимации узлы могут называться иначе, чем у модели (`mixamorig:Hips_$AssimpFbx$_Rotation` и т.п.). Без привязки `AnimationClip::boneChains` часть каналов молча не применяется (вплоть до Hips/Spine). `bindClipToSkeleton` обязателен и вызывается из `appendFromAssimp` при переданном скелете.
- **Mixamo-практика:** одиночный клип переименовывается в имя файла (в файлах клип часто называется `mixamo.com`); текстуры встроены в FBX, а пути ведут на билд-сервер Adobe (загрузка — через fallback по имени встроенной текстуры); MD5 приходит Z-up (вьювер поворачивает на -90° вокруг X), FBX/DAE/OBJ — Y-up.
- **Скиннинг:** максимум `__blib_max_bones = 100` костей (шейдер); у Mixamo 68 — запас есть, но лимит общий.
- **Владение GL:** модель обязана выгружаться раньше окна/рендер-таргета — деструкторы `Mesh`/`Material` освобождают GL-ресурсы через сохранённый `RenderContext`.
- `AnimatorComponent::getAnimations()` для непривязанного компонента возвращает static-пустой вектор — ссылка валидна всегда.
- **Панели и выгрузка:** после `unloadModel` снять `setSkelet(nullptr)`/`setAnimatorComponent(nullptr)` — иначе панели держат висячие указатели.
- **DialogWindow и TextWrapped:** `AlwaysAutoResize`-модалка без `SetNextWindowSizeConstraints` схлопывается в узкий столбец (текст переносится по слову, кнопки уходят за край) — в `draw()` задаётся минимальная ширина `dialogMinWidth`.
- **DialogWindow и окно-хост:** `OpenPopup`/`BeginPopupModal` читают `g.CurrentWindow` (ID-стек) — на корневом уровне (вне окон) это UB. Весь цикл жизни попапа живёт внутри невидимого окна `##DialogWindowPopupHost` (флаги NoDecoration/NoBackground/NoSavedSettings/NoInputs, позиция за экраном); ID попапа завязан на ID-стек хоста — проверять открытость снаружи можно только через internals (`FindWindowByName` + `Active`).
- **DialogWindow и пустые label:** `finishWith*`/`close()` очищают строки контента — после них контент кадра больше не рисуется (флаг `finished`). Отрисовка `TextWrapped("")`/`Button("")` в корне окна даёт `id == window->ID` и `IM_ASSERT` в Debug-сборке ImGui (регрессия покрыта группой тестов `dialogWindow`).

---

## TODO

- [ ] Фаза 2 модульных доков beng: `MODEL_VIEWER.md`, `CLIENT.md`, `EDITOR.md` (`GRAVELANDS.md` — готов, см. `misc/gravelands`).
- [ ] `beng-server` — не реализован (см. ARCHITECTURE.md).
- [ ] `Application`, рефлексия компонентов, `ResourceManager` — не реализованы (must-требования ARCHITECTURE.md).

---

## Связанные доки

- `AGENTS.md` — правила проекта и конвенции.
- `ARCHITECTURE.md` — слои, требования к beng, roadmap.
- `ERROR_HANDLING_ARCHITECTURE.md` — обработка ошибок.
- `src/blib/BLIB.md` — карта blib; `src/blib/graphics/GRAPHICS.md` — рендер и скелетная анимация; `src/blib/system/SYSTEM.md` — память и потоки.
- `src/blib/system/memory/AUTO_DEBUG_ALLOCATOR.md` — пример модульного дока; аллокатор.
