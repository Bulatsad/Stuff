# GRAPHICS — blib-graphics

> Слой: `blib`. OpenGL/WGL, окно, ассеты, скелетная анимация, ImGui.
> Шпаргалка по инвариантам, владению GL и граблям. **Обновлять при изменениях кода модуля** (см. AGENTS.md, «Документация модулей»).

---

## Назначение и границы

- Рендер-модуль blib: Win32-окно + WGL, ручная загрузка OpenGL (без GLEW/GLAD), рендер-контекст/таргеты, шейдеры, меши, материалы/текстуры, камеры, скелетная анимация, отладочные линии, ImGui + консольное окно, парсеры TGX/GM1.
- **Реализация только под Windows** (`impl/win/`). При включении модуля на не-Windows CMake даёт `FATAL_ERROR`.
- Assimp-сцены модуль **не парсит сам** — принимает готовый `aiScene*` (загрузка файлов — в `beng-client`, см. `../../beng/BENG.md`). В сборку входят ImGui, stb (stb_image), Assimp-хедеры (PUBLIC include).
- Не в этом доке: ECS-интеграция и панели эдитора — `../../beng/BENG.md`.

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Окно + WGL-контекст | `renderWindow.h`, `impl/win/renderWindow.cpp`, `impl/win/winRenderWindowUtil.h` |
| Загрузчик GL-функций | `opengl.h`, `impl/openglNoPlatformDependent.cpp`, `impl/win/opengl.cpp` |
| Рендер-контекст (матрицы/uniforms) | `rendercontext.h`, `impl/rendercontext.cpp` |
| Рендер-таргет (FBO) | `rendertarget.h`, `impl/rendertarget.cpp` |
| Шейдеры | `shader.h`, `impl/win/shader.cpp`, `shaderPaths.h` |
| Меш | `mesh.h`, `impl/mesh.cpp`, `face.h`, `vertex.h`, `vector.h` |
| Материал/текстура/изображение | `material.h`, `impl/material.cpp`, `texture.h`, `impl/win/texture.cpp`, `image.h`, `impl/image.cpp` |
| Камеры | `iCamera.h`, `camera.h`, `orbitCamera.h`, `viewport.h` |
| Скелет и анимация | `skelet.h`, `bone.h`, `skinmodel.h`, `skinmesh.h`, `animator.h`, `animationclip.h`, `impl/skelet.cpp`, `impl/skinmodel.cpp`, `impl/skinmesh.cpp`, `impl/win/animator.cpp`, `impl/bone.cpp` |
| Отладочные линии | `lineRenderer.h`, `impl/lineRenderer.cpp` |
| Консольное окно | `console/consoleWindow.h` |
| Форматы TGX/GM1 | `tgx.h`, `gm1.h`, `impl/tgx.cpp`, `impl/gm1.cpp` |
| Трансформации | `transform.h`, `transformable.h`, `transformMatrix.h`, `impl/transformable.cpp`, `impl/transformMatrix.cpp` |
| GLSL-шейдеры | `src/shaders/mesh/*.glsl`, `src/shaders/line/*.glsl` (пути — `shaderPaths.h`) |

---

## Поток кадра

1. **Окно:** `RenderWindow(width, height, title, style)` создаёт Win32-окно и WGL-контекст; `update()` качает сообщения, `isOpen()`, `close()`, `swapBuffers()`. `__getCtx()` даёт платформенный `WinCtx` (hwnd и т.д.).
2. **Кадр таргета:** `IRenderTarget::clear(color)` биндит очередной FBO из кольца и очищает его; `draw(const IDrawable&)` вызывает `drawable.draw(rc)`. `rc` — публичное поле таргета.
3. **Отрисовка:** `RenderContext::setShaderProgram`, отправка матриц (`gViewMatrix`, `gProjectionMatrix`, `gModelMatrix`) и костей (`gBones`), бинд текстуры. Матрицы уходят с `transpose = GL_TRUE`.
4. **UI:** ImGui-кадр рисуется в back-буфер (`ImGui_ImplOpenGL3_*`); порядок «сцена в FBO → UI → swap» — ответственность приложения (см. `../../beng/BENG.md`, кадр вьювера).

### RenderContext

- Поля: `transform`, `pCamera`, `vievMatrix` (опечатка в API), `projectionMatrix`, `lastShader`, `api` (копия указателей GL), `useDiffuseTextures`.
- `setCamera` только сохраняет указатель; `sendVievMatrixToShaderProgram`/`sendProjectionMatrixToShaderProgram` **разыменовывают `pCamera` без проверки** — камеру обязательно задать до отрисовки.
- `sendBoneMatricesToShaderProgram` шлёт `finalMatrices` скелета и обрезает их до `__blib_max_bones` (100).
- `getFlatWhiteTexture()` — ленивая статическая 1×1 белая GL-текстура; подставляется, когда `useDiffuseTextures == false` или у материала нет текстуры.
- `applyTransform` — пустая заглушка (не полагаться).

### IRenderTarget (FBO)

- Конструктор `IRenderTarget(width, height, frameBuffersCount = 2)`; по умолчанию **кольцо из 2 FBO**. `clear()` переключает индекс кольца на следующий.
- **Грабли:** `clear()` игнорирует аргумент цвета (`glClearColor` закомментирован); между `clear()` и `draw()` одного таргета нельзя очищать/рисовать другой таргет (переключение глобального GL-контекста — предупреждение в `rendertarget.h`).
- `resize()` пересоздаёт FBO; `getContext()` даёт `RenderContext&`.
- Деструктор освобождает текстуры/рендербуферы/FBO — **требует живого GL-контекста** (см. «Владение GL»).

### Камеры

- `ICamera` — только `getViewMatrix/getProjectionMatrix`.
- `Camera` (fly-cam): `Transform` + перспектива, `controlUpdate(dt, window, focused)` (мышь+клавиатура).
- `OrbitCamera`: орбита вокруг цели (`setTarget/setDistance/rotate/pan/zoom/setPerspective/update`), используется вьювером/эдитором.
- `Viewport` — пустая надстройка над `IRenderTarget`, не используется.

### Шейдеры

- `Shader`: `setPath/setType/setRenderApi/compile()` → `ShaderError {None, InvalidType, FileNotFound, CompilationFailed, LinkFailed}`.
- `ShaderProgram`: `create/AttachShader/compile/use/unuse`.
- Ошибки компиляции в `Mesh::bake`/`LineRenderer::bake` **не фатальны**: warning + `baked = true`, объект молча не рисуется.
- `shaderPaths.h` хранит **абсолютные пути** `M:\Stuff\src\shaders\...` — вьювер работает только при запуске из корня репозитория (TODO: путь относительно exe).

---

## Ассеты

### Mesh

- CPU-данные: `vertices` (Vector3f), `textureCoords`, `normals` (загружаются, но VBO норм не заполняется), `boneIds` (Vector4i), `boneWeights` (Vector4f), `faces`, `material`.
- GL-атрибуты: position=0, textureCoords=1, boneIds=2 (`glVertexAttribIPointer`, GL_INT), boneWeights=3.
- **Ленивый `bake`** при первом `draw`: VAO + 6 VBO + EBO, компиляция шейдеров, `material.bake(ctx)`, сохранение указателя на `RenderContext`.
- Выбор вершинного шейдера: `boneIds.empty() ? MeshVertexShader : SkinMeshVertexShader`.
- `draw(ctx)` и `draw(ctx, const std::vector<TransformMatrix>* boneMatrices)`; второй путь — скиннинг.
- **`Mesh` некопируем по смыслу:** владеет сырым `ctx` (`GlobalAllocator`) и GL-ресурсами; копирование/перемещение объекта приводит к двойному освобождению. Векторы мешей заполняются через `resize`/`swap` (см. `SkinModel`).

### Material

- Поля: `DiffuseColor`/`AmbientColor`/`SpecularColor`, `hasDiffuseColor`, `diffuse` (Texture), `diffuseImage` (Image), `m_transparencyFactor`, `m_alphaTest`.
- `loadFromAssimpMaterial(material, folder, scene)`: диффузный цвет (fallback) + диффузная текстура.
- Диффузная текстура: `"*<N>"` — встроенная в сцену (`aiScene::mTextures`); иначе внешний файл (`Folder::down` + `stbi_load`); если файла рядом нет — повторный поиск встроенной **по имени файла** (кейс Mixamo: материал ссылается на путь билд-сервера Adobe).
- Встроенные: сжатые (`mHeight == 0`, `mWidth` = размер в байтах) через `stbi_load_from_memory`; несжатые `aiTexel` (BGRA) тоже поддержаны. `stbi_set_flip_vertically_on_load(1)`.
- `bake(ctx)`: пустое изображение + `hasDiffuseColor` → синтез 1×1 текстуры из цвета; иначе GL-текстура не создаётся (в `Mesh::draw` будет белая заглушка).
- `MaterialError {None, TextureLoadFailed, UnsupportedFormat, NotImplemented}`; >1 диффузной текстуры — `NotImplemented`.
- Деструктор освобождает GL-текстуру через сохранённый `pRenderContext`.

### Texture / Image

- `Texture`: `create(Image, ctx, genFlags)`, `free(ctx)`, `resize`, `update`, `makeTextureAtlas` (заглушка), `getContext()` (`textureID`). **Удаляется только через `free(ctx)`** — деструктор лишь предупреждает об утечке.
- `Image` — CPU-битмап RGBA (`std::vector<Color>`, row-major), `operator[]` → `UnsafeSlicer`; загрузка TGX; `getData()`/`update()`.

### Форматы

- `TGXFile`/`GM1File` — парсеры форматов Heroes III (TGX — картинки, GM1 — контейнер с палитрой). Используются gravelands-клиентом (`tiles.gm1`).

---

## Скелет и анимация

### Bone (`bone.h`)

- `name`, `node` (aiNode из Assimp, заполняется `PopulateArmatureData`), `chain` (цепочка узлов FBX-декомпозиции `<имя>_$AssimpFbx$_...` от внешнего к собственному), `weights`, `offsetMatrix` (inverse bind), `localTransform`, `globalTransform`.
- `BoneChainElement { nodeName, bindTransform }`.

### Skelet (`skelet.h` / `impl/skelet.cpp`)

- `loadFromAssimp(aiMesh*)` / `loadFromAssimp(aiScene*)` → `finishFromArmature`:
  - корень — сам узел armature, иначе первая кость, чьё поддерево содержит все кости, иначе синтетическая корневая кость с identity;
  - `makeBoneTree` (узлы без кости пропускаются, поддерево идёт с тем же родителем);
  - `loadDefaultPoseFromNodes` строит `bone.chain` и bind-local (трансформ scene root не включается — у MD5 там конверсия осей);
  - `computeBindPose()` **обязателен** до первой отрисовки: без него `finalMatrices` нулевые, а скелет «разобран».
- `applyClip(clip, timeTicks)`:
  - если у клипа есть `boneChains` — локальный трансформ собирается по цепочке **файла анимации** (сэмпл канала или bind-узел);
  - иначе — по `bone.chain` модели; кости без канала сохраняют bind-local.
  - Финал: `updateTransforms` (global = parent.global * local) + `finalMatrices[i] = global * offsetMatrix`.
- `isCompatibleWith(other)` — полное совпадение скелетов: число костей, имена, имена родителей, `offsetMatrix` с допуском `1e-4` (для подмены скина).
- `adoptOffsetMatricesFrom(other)` — перенос inverse-bind на одноимённые кости (retarget кожи на текущий риг; force-подмена мешей); кости без пары не трогаются; повторный вызов идемпотентен.
- `hasNodeName(name)` — есть ли узел среди костей/цепочек.
- `bindClipToSkeleton(clip, animationScene)` — строит `clip.boneChains` по дереву файла анимации; обязателен, когда декомпозиция внешнего FBX отличается от модели.

### AnimationChannel / AnimationClip (`animationclip.h`)

- Канал: имя узла + ключи position/rotation/scale; `sample()` возвращает дефолты при отсутствии ключей; интерполяция — `lerp`/`nlerp`.
- Клип: `name`, `tickPerSecond`, `durationTicks/durationMs`, `cycled` (по умолчанию `true`), `channels`, `boneChains`.
- `BoneChain { boneIndex, elements }`; элемент: `channelIndex` (< `channels.size()`) либо bind-трансформ.

### Animator (`animator.h` / `impl/win/animator.cpp`)

- `loadFromAssimp(scene)` — все клипы, сброс состояния.
- `appendFromAssimp(scene, skelet, fallbackName)` — **добавляет** клипы, не сбрасывая плейбек; единственный клип файла получает имя файла (Mixamo-клипы часто `mixamo.com`), дубликаты — `" #N"`; при заданном `skelet` привязывает `boneChains`.
- `selectAnimation` (сброс времени в 0), `play/pause`, `setCycled` (только текущий клип), `update(dtMs)` (при `cycled` — `fmod`, иначе время не клампится).

### SkinModel / SkinMesh

- `SkinModel` = `Skelet` + `Animator` + `vector<SkinMesh>`.
- `loadFromAssimp(scene, filename, animationScene = nullptr)`: скелет → аниматор (из `animationScene` или основной сцены) → меши + материалы.
- `replaceMeshesFromAssimp(scene, filename, force = false)`: атомарная подмена скина — отказ при нуле мешей (force не отменяет); скелет-кандидат грузится только для `isCompatibleWith`; при несовместимости обычный режим отказывает, `force = true` — warning, `adoptOffsetMatricesFrom` + `computeBindPose` (retarget bind-позы: меш ведёт себя как нативно скиннутый к текущему ригу) и продолжение (кандидат пробрасывается в `SkinMesh` для remap-а весов); новые меши во временном векторе, затем `swap`.
- `update(dtMs)`: `animator.update` + `timeTicks = (currentTimeMs/1000)*tickPerSecond` + `skelet.applyClip`.
- `draw(ctx)`: `modelTransform * meshTransform` на каждый меш + `mesh.draw(ctx, &finalMatrices)`.
- `SkinMesh::loadFromAssimpMesh(aiMesh*, const Skelet&, force = false, candidateSkelet = nullptr)` — раскладка весов: кость ищется по имени; на вершину максимум 4 веса (лишние отбрасываются с warning); `boneIndex >= __blib_max_bones` — ошибка всегда. Неизвестная кость: обычный режим — ошибка; `force = true` — веса переносятся на ближайшего предка из иерархии `candidateSkelet`, существующего в текущем скелете (у Mixamo ленты `Ribbon*` висят на Head); если предка нет — отброс весов + **ренормализация** затронутых вершин к сумме 1 (вершины с нулевой суммой остаются нулевыми, warning).

### Assimp-флаги (важно)

Загрузка файлов живёт в `beng-client` (`SkinnedMeshComponent`): `aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_PopulateArmatureData`. **`PopulateArmatureData` обязателен** — без него `aiBone::mArmature`/`mNode` пустые и скелет не грузится. `aiScene` живёт только внутри функции загрузки — `SkinModel` копирует нужное.

---

## Отладка и прочее

- `LineRenderer`: `addLine(start, end, color)`, `clear()`, `draw(ctx)`; VAO/VBO, ленивый bake, GL_LINES, перезаливка `GL_DYNAMIC_DRAW`. GL-ресурсы и шейдеры не освобождаются.
- `ConsoleWindow` (`console/consoleWindow.h`): ImGui-окно консоли — презентация над `blib::console::Console`; единственный консюмер вывода.
- `CoordinateSystemXYZ` — заглушка конвертации координат.
- `keyboard.h`/`mouse.h` — опрос состояния Win32 (`GetAsyncKeyState`/`GetCursorPos`), `Keyboard::update()` раз в кадр; опрос идёт вне зависимости от фокуса окна.

---

## Владение GL и порядок разрушения

- `RenderContext` принадлежит `IRenderTarget` по значению; каждый таргет загружает GL-указатели в конструкторе.
- `Mesh`: `ctx` — GlobalAllocator, GL-ресурсы (VAO/VBO/EBO) — лениво в `bake`, освобождаются в `~Mesh`, если сохранён `RenderContext*`.
- `Material`: GL-текстура — в `bake`, освобождается в деструкторе через `pRenderContext`.
- **Порядок обязателен: модели/меши/материалы выгружаются раньше окна и рендер-таргета** (иначе GL-контекст уже мёртв). Так сделано в `ViewerCore::shutdown` и `SkinnedMeshComponent`.
- `Texture` живёт до явного `free(ctx)`; копирование `Texture` поверхностное (копия и оригинал указывают на один GL-объект) — не копировать.
- `Shader`/`ShaderProgram` не имеют деструкторов (контексты создаются через `new`, GL-объекты не удаляются) — потенциальные утечки; создавать шейдеры в долгоживущих объектах и не пересоздавать в цикле.

---

## Подводные камни

- **Пути шейдеров абсолютные** (`shaderPaths.h`) — запуск только из корня репозитория.
- `RenderContext::send*Matrix*` без камеры — падение (нет проверки `pCamera`).
- `IRenderTarget::clear` игнорирует цвет и переключает FBO-кольцо; порядок `clear`/`draw` нельзя чередовать между таргетами.
- `Mesh`/`Material`/`Texture` нельзя копировать (сырые GL-хендлы и указатели контекста).
- Скиннинг: **лимит 100 костей** продублирован в C++ (`__blib_max_bones`), в `skinmesh.cpp` и в `SkinMeshVertexShader.glsl` — при рассинхроне молчаливое отсечение; >4 весов на вершину теряются.
- `SkinModel::draw` мутирует трансформы мешей в `const`-методе (`meshes` — `mutable`) — не потокобезопасно.
- Внешняя анимация без `bindClipToSkeleton` частично не применяется (см. `../../beng/BENG.md`).
- Шейдеры мешей/линий: ошибка компиляции не фатальна — объект молча не рисуется, ищите warning в консоли.

---

## TODO

- [ ] Обсудить каталог известных багов модуля (отдельная задача).
- [ ] Относительные пути шейдеров (относительно exe/рабочей директории) вместо абсолютных.
- [ ] Освобождение GL-ресурсов шейдеров (`Shader`/`ShaderProgram` деструкторы).
- [ ] Нормали VBO (сейчас грузятся в CPU и не отправляются в GL).
- [ ] Пересмотреть `IRenderTarget::clear` (цвет) и кольцо FBO.
- [ ] Триангуляция/рендер мешей — TODO в `mesh.cpp`.
- [ ] Атлас текстур (`Texture::makeTextureAtlas` — заглушка).

---

## Связанные доки

- `../BLIB.md` — общая карта и философия blib.
- `../core/CORE.md` — math, streams, console (зависимость graphics).
- `../../beng/BENG.md` — ECS-компоненты (`SkinnedMeshComponent`, `AnimatorComponent`), панели, кадр вьювера.
- `AGENTS.md` — правила проекта (аллокации, логирование, касты).
