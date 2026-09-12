# BENG — среда выполнения (Bulat Engine)

> Слой: `beng`. Шпаргалка по устройству, инвариантам и граблям — чтобы не перечитывать исходники.
> Не дублирует правила проекта (`AGENTS.md`) и roadmap (`ARCHITECTURE.md`) — только ссылается на них.
> **Обновлять при любом изменении кода beng** (см. AGENTS.md, «Документация модулей»).

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
| Базовый компонент, реестр типов | `src/beng/core/component.h` |
| Интерфейс системы, приоритеты | `src/beng/core/system.h` |
| Лимиты и базовые типы ECS | `src/beng/config.h` |
| Время кадра | `src/beng/core/time.h/.cpp` |
| Transform + иерархия | `src/beng/components/transform.h/.cpp` |
| TransformSystem | `src/beng/systems/transformSystem.h/.cpp` |
| Скелетная модель (владеет `SkinModel`) | `src/beng/client/components/skinnedMeshComponent.h/.cpp` |
| Анимация (состояние плейбека) | `src/beng/client/components/animatorComponent.h/.cpp` |
| Продвижение и применение анимации | `src/beng/client/systems/animationSystem.h/.cpp` |
| Отрисовка сцены | `src/beng/client/systems/renderSystem.h/.cpp` |
| Контракт панели | `src/beng/editor/panels/iPanel.h` |
| Панели: иерархия/анимации/вьюпорт/опции/консоль | `src/beng/editor/panels/*` |
| Пример композиции приложения | `src/misc/model_viewer/core/viewerCore.cpp` |
| Тесты | `src/beng/test/src/impl/test*.cpp` (фреймворк `blib::test`, `BUILD_TESTS=ON`) |
| Демо ECS | `src/beng/test_ecs/` |

---

## Инварианты и поток данных

### beng-core (ECS)

- Типы (`config.h`): `EntityID = buint64`, `invalidEntity = 0`; `ComponentType = buint8`, `ComponentMask = buint64`, лимит **64 типа** компонентов.
- **Регистрация типов обязательна:** `scene.registerComponentType<T>()`. `getComponentPool<T>()`/`addComponent<T>()` без регистрации — fatal. Регистрация — строго до запуска цикла (реестр типов не thread-safe).
- **Владение:** `ComponentPool<T>` хранит компоненты через `PoolAllocator`; `destroyEntity`/`removeComponent` вызывают `~T()`. Компонент, владеющий ресурсом, освобождает его в деструкторе (пример: `SkinnedMeshComponent` → `SkinModel`).
- Указатель на компонент стабилен, пока компонент жив: `destroy` другого компонента двигает только `Entry` (swap-and-pop).
- Служебные контейнеры — через `StdAllocatorAdapter` (GlobalAllocator), без `::operator new`.
- **Системы:** `ISystem::update(scene, dt)`; сортировка по `getPriority()` (меньше — раньше); `Scene` не владеет системами; всё последовательно в main thread. Добавление системы в рантайме безопасно (флаг `systemsDirty` → пересортировка в `update`).
- `Scene` и `ComponentPool` **некопируемы/неперемещаемы** (держат указатель на собственный аллокатор).
- ID сущностей не переиспользуются: сохранённый ID валиден до конца жизни `Scene`.

### Transform

- `TransformComponent` — локальные TRS + `parent`/`children` (EntityID), кеш `worldMatrix` с dirty-флагом (рекурсивно тянет мировую матрицу родителя).
- `setParent` отклоняет циклы и самого себя (warning, no-op); для смены родителя нужен `Scene`.
- `TransformSystem` (приоритет -100) просто вызывает `getWorldMatrix()` у всех — порядок обхода dense не важен.

### beng-client

- `SkinnedMeshComponent` — **владеет** `blib::graphics::SkinModel` (GlobalAllocator + placement new; `unload()` идемпотентен; `getModel()` может быть nullptr). `loadFromFile` пересоздаёт модель.
- `AnimatorComponent` — **не владеет** аниматором: хранит указатель на `Animator` внутри `SkinModel` + `loop`/`poseDirty`. При выгрузке модели указатель обязан быть снят (`setAnimator(nullptr)` или уничтожение сущности) — иначе висячий указатель.
- `AnimationSystem` (приоритет -50): играет → `SkinModel::update(dt_ms)` (время в миллисекундах!); нециклическая доиграла → `pause()`; пауза + `poseDirty` (выбор клипа/скраб) → `update(0)` + сброс флага.
- `RenderSystem` (приоритет 100): `TransformComponent::getWorldMatrix()` → `model->setTransform(...)` → `renderTarget->draw(*model)`. Без таргета — no-op; таргетом не владеет.

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

- [ ] Фаза 2 модульных доков beng: `MODEL_VIEWER.md`, `CLIENT.md`, `EDITOR.md`, `GRAVELANDS.md`.
- [ ] `beng-server` — не реализован (см. ARCHITECTURE.md).
- [ ] `Application`, рефлексия компонентов, `ResourceManager` — не реализованы (must-требования ARCHITECTURE.md).
- [ ] `ComponentTypeRegistry` не thread-safe — регистрация типов строго до запуска цикла.

---

## Связанные доки

- `AGENTS.md` — правила проекта и конвенции.
- `ARCHITECTURE.md` — слои, требования к beng, roadmap.
- `ERROR_HANDLING_ARCHITECTURE.md` — обработка ошибок.
- `src/blib/BLIB.md` — карта blib; `src/blib/graphics/GRAPHICS.md` — рендер и скелетная анимация; `src/blib/system/SYSTEM.md` — память и потоки.
- `src/blib/system/memory/AUTO_DEBUG_ALLOCATOR.md` — пример модульного дока; аллокатор.
