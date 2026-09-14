# GRAVELANDS — референсная игра (диаблоид)

> Слой: `game` (`src/misc/gravelands`). Шпаргалка по устройству игры: таргеты, камера, тайлы, таймстеп.
> Не дублирует правила проекта (`AGENTS.md`) и roadmap (`ARCHITECTURE.md`) — только ссылается.
> **Обновлять при любом изменении кода gravelands** (см. AGENTS.md, «Документация модулей»).

---

## Назначение и границы

- **Gravelands — диаблоид** в духе «Path of Exile 2 с графикой Stronghold»; стилистический ориентир клиента — Hades 2 (см. ARCHITECTURE.md и план NPR-пайплайна в TODO).
- Таргеты (паттерн «lib + тонкий exe», каждый exe — тонкая `main()` над core-библиотекой):

| Таргет | Тип | Содержимое |
|--------|-----|------------|
| `gravelands-common` | library (header-only, INTERFACE) | общие константы (`config.h`); позже — компоненты, пакеты, формулы |
| `gravelands-server-core` + `gravelands-server` | library + exe | авторитетная ECS-симуляция (Scene + TransformSystem), фикс. тикрейт 30 Гц |
| `gravelands-client-core` + `gravelands-client` | library + exe | представление: окно, изокамера, тайлы, ввод |

- Слои: клиент/сервер зависят от beng/blib; `gravelands-common` — только от blib-int/beng-core.
- Что НЕ в этом доке: правила кодирования (AGENTS.md), план развития (ARCHITECTURE.md), детали ECS (BENG.md), детали рендера (GRAPHICS.md).

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Общие константы (тикрейт, окно, имя игры) | `common/config.h` |
| Клиентское ядро (frame-API, pimpl) | `client/core/clientCore.h/.cpp` |
| Сетка тайлов (квадраты на XZ) | `client/core/isometricTileset.h/.cpp` |
| Тонкий exe клиента | `client/main/main.cpp` |
| Серверное ядро (аккумулятор тиков) | `server/core/serverCore.h/.cpp` |
| Тонкий exe сервера | `server/main/main.cpp` |
| CMake-композиция подпроектов | `CMakeLists.txt` (корень gravelands) |

---

## Инварианты и поток данных

### Таймстеп (гибридный)

- **Сервер** — авторитет: `serverCore::tick()` меряет реальное dt (`beng::Time`), копит в аккумулятор и шагает `scene.update(serverFixedDelta)` фиксированными тиками 30 Гц (`serverTickRate`/`serverFixedDelta` из `common/config.h`). Несколько тиков за кадр — норм (догон), неиспользованный остаток живёт в аккумуляторе.
- **Клиент** — переменный dt кадра (`beng::Time::getDeltaTime()`), рендер каждый кадр.
- Сетевой цикл (команды TCP → тик → снапшоты TCP → интерполяция) ещё не реализован — см. ARCHITECTURE.md.

### Клиент (ClientCore)

- Pimpl `ClientCoreImpl` скрывает графические типы blib; память — через `GlobalAllocator` + placement new, `shutdown()` симметрично возвращает (деструктор — страховка).
- **ИНВАРИАНТ — единый ECS-рендер:** алгоритм кадра «создать сцену → добавить объекты на сцену → отрисовать сцену». Вся отрисовка мира — ТОЛЬКО через `scene.update(dt)` (`RenderSystem` beng-client, см. BENG.md); прямых `renderTarget.draw(...)` в клиенте нет. Пост-пасс — после сцены, над FBO (презентация, не объект мира).
- Кадр `tick()`: `time.tick()` → `window.update()` → `Keyboard::update()` → Escape/отладочные клавиши → `updateCamera(dt)` → `updateLight(dt)` → `renderTarget.clear()` → **`scene.update(dt)`** → пост-пасс (`P` вкл/выкл) → оверлей → `swapBuffers`.
- **Мир — сущности со слоями рендера** (`setupWorld()` в `initialize()`):

| Объект | Компоненты | Слой RenderLayer |
|--------|-----------|------------------|
| Тайлы (сетка 10×10 квадратов на XZ, шахматная текстура) | `Transform` + `MeshRenderComponent` (`IsometricTileset::buildMesh()`) | `Ground` |
| Тени (сфера r=30, танцор r=8, деревья r=16; радиальный градиент, подъём 0.5) | `Transform` + `MeshRenderComponent` (`BlobShadow::takeMesh()`) | `Shadow` |
| Деревья (3 квада 40×70, процедурная текстура, alpha-test, поворот к камере) | `Transform` + `MeshRenderComponent` (`SpritePlane::takeMesh()`) | `AlphaTested` |
| Сфера (r=25, шахматка, Toon + контур 0.6) | `Transform` + `MeshRenderComponent` (`Sphere::takeMesh()`) | `Opaque` |
| Танцор (`resources\Hip Hop Dancing.fbx`, масштаб 0.1, позиция (0,0,60)) | `Transform` + `SkinnedMeshComponent` + `AnimatorComponent` | `Opaque` (скиннинг) |

- **Камера** (`blib::graphics::IsometricCamera`): ракурс фиксирован (pitch 55°, yaw 45°, FOV 30° — лёгкая перспектива); WASD двигает `target` камеры в плоскости земли; `Add`/`Subtract` — зум. После изменений обязателен `camera.update()`.
- **Поворот плоскостей к камере**: направление **к** камере (`camera.getPosition() - getTarget()`), `yaw = atan2(-dir.x, dir.z)` (конвенция `rotateY`: локальная +Z → `(-sin(yaw), 0, cos(yaw))`, см. GRAPHICS.md/SpritePlane).
- **Отладочное управление (клавиши):** `N` — раскраска нормалями; `M` — сфера unlit/toon; `O` — контур сферы; **стрелки** — азимут/элевация света; `[`/`]` — интенсивность; **F5** — `hotreload` (перекомпиляция шейдеров, `registerGraphicsConsoleCommands()`); `P` — пост-пасс.
- **Оверлей** — полупрозрачное ImGui-окно в углу (без рамок/ввода): параметры света/камеры + список клавиш. Паттерн вьювера: WndProc-хук + сцена → back-буфер (пост-пасс или `blitToBackbuffer`), ImGui поверх, `swapBuffers`.
- **Пути контента** резолвятся из cwd → каталога exe → подъёмом по родителям (`resolveContentPath`).

### Сервер (ServerCore)

- Владеет `beng::Scene` и `beng::TransformSystem` (Scene хранит сырой указатель на систему — порядок полей важен: система объявлена после сцены, разрушается раньше).
- `initialize()`: регистрация типов компонентов (строго до цикла — реестр не thread-safe, см. BENG.md) + `time.reset()`.
- `tick()`: аккумулятор → `transformSystem.update(scene, serverFixedDelta)` столько раз, сколько тиков накопилось; heartbeat-лог раз в секунду симуляции (`tickCounter`/`lastHeartbeatSecond` — защита от повторных логов в кадрах без тиков).

---

## Подводные камни / известные баги

- **Порядок выгрузки GL:** в `ClientCoreImpl` сцена (меши) объявлена ПОСЛЕ окна/таргета и разрушается РАНЬШЕ них — GL-контекст на момент освобождения ресурсов мешей жив (см. GRAPHICS.md «Владение GL»). Новые графические члены добавлять только перед `time` (после окна/таргета).
- **Изокамера:** `moveTarget` двигает цель в мировых координатах — движение по диагонали (W+D) быстрее одиночного; для геймплея нужна нормализация в `updateCamera`. Дистанция кламплена `[5, 100000]`, pitch `[1, 89]` — вырождение `lookAt` исключено.
- **Шахматная текстура тайлов** — 1 пиксель на клетку с LINEAR-фильтрацией: на границах ячеек лёгкое «просачивание» соседнего цвета. Терпимо для отладки; при появлении настоящих тайловых текстур заменить.
- **Сервер:** heartbeat печатает в консоль каждый тик первой секунды — шумно на старте (терпимо, но держать в уме при подключении сетевого слоя).
- **Два процесса при одиночной игре** (клиент + локальный сервер, ARCHITECTURE.md) — сейчас сервер отдельным exe не запускается; интеграция не реализована.

---

## TODO

- [x] **NPR/Hades-пайплайн, фазы 1–9**: изокамера + тайлы на XZ; нормали в VBO/шейдеры + сфера; шейдерный минимум (владение GL-ресурсов, кеш uniform-локаций, hotreload-команда, относительные пути); мягкий toon-свет (DirectionalLight/AmbientLight, smoothstep-ramp, rim, emission) + оверлей и управление светом клавишами; рисованные плоскости (SpritePlane, unlit + alpha-test, процедурные «деревья»); пост-пасс (depth-fog, grading, виньетка, `P` — вкл/выкл); blob-тени (BlobShadow, блендинг без записи глубины); контуры (inverted hull, `O` — вкл/выкл); реальная скелетная модель (beng-client ECS, Mixamo-FBX с анимацией, свет + тень + контур).
- [x] **Единый ECS-рендер**: весь мир — сущности (`MeshRenderComponent` + `RenderLayer` в beng-client), отрисовка только через `scene.update()`/`RenderSystem`; прямых `renderTarget.draw(...)` в клиенте нет (инвариант закреплён в BENG.md и ARCHITECTURE.md).
- [ ] Свет в ECS (`LightComponent`/свет сцены) — сейчас свет живёт в `RenderContext` (состояние презентации); разобраться отдельно.
- [ ] Полупрозрачность + сортировка, MSAA/FXAA, sRGB-конвейер (GRAPHICS.md TODO) — отдельными заходами, если понадобятся.
- [ ] Толщина контура должна масштабироваться от размера объекта/дистанции (сейчас фикс. мировые единицы).
- [ ] `ResourceManager`, рендер-ECS на базе beng-client (RenderSystem, CameraComponent) — тайлы и сферы переедут в ECS.
- [ ] Сетевой слой: TCP-команды/снапшоты, запуск локального сервера при одиночной игре.
- [ ] Спрайтовые персонажи/интерполяция между снапшотами (клиент).
- [ ] Normalize движения камеры по диагонали, подгон скорости к дистанции зума.

---

## Связанные доки

- `AGENTS.md` — правила проекта и конвенции.
- `ARCHITECTURE.md` — слои, требования к beng, референсная игра, roadmap.
- `src/beng/BENG.md` — ECS-ядро, компоненты/системы, паттерны сцен.
- `src/blib/graphics/GRAPHICS.md` — рендер, камеры (IsometricCamera), меши, шейдеры.
