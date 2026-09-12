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
| Камеры | `iCamera.h`, `camera.h`, `orbitCamera.h`, `isometricCamera.h`, `viewport.h` |
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

- Поля: `transform`, `pCamera`, `vievMatrix` (опечатка в API), `projectionMatrix`, `lastShader`, `api` (копия указателей GL), `useDiffuseTextures`, `showNormals` (отладочная раскраска нормалями, uniform `gShowNormals` — шейдеры без него игнорируют), `directionalLight`/`ambientLight` (NPR-свет).
- `setCamera` только сохраняет указатель; `sendVievMatrixToShaderProgram`/`sendProjectionMatrixToShaderProgram` **разыменовывают `pCamera` без проверки** — камеру обязательно задать до отрисовки.
- `sendBoneMatricesToShaderProgram` шлёт `finalMatrices` скелета и обрезает их до `__blib_max_bones` (100).
- `getFlatWhiteTexture()` — ленивая статическая 1×1 белая GL-текстура; подставляется, когда `useDiffuseTextures == false` или у материала нет текстуры.
- `applyTransform` — пустая заглушка (не полагаться).

### IRenderTarget (FBO)

- Конструктор `IRenderTarget(width, height, frameBuffersCount = 2)`; по умолчанию **кольцо из 2 FBO**. `clear()` переключает индекс кольца на следующий.
- Аттачменты: цветовая RGBA-текстура + **depth-текстура** (GL_DEPTH_COMPONENT24, сэмплируется пост-процессингом; раньше был depth/stencil renderbuffer). Доступ — `getColorTexture()`/`getDepthTexture()` (текстуры **текущего** кадрового буфера, валидны после `clear()`).
- **Грабли:** между `clear()` и `draw()` одного таргета нельзя очищать/рисовать другой таргет (переключение глобального GL-контекста — предупреждение в `rendertarget.h`).
- `clear(color)` **учитывает цвет** (glClearColor; BUG-FIX — раньше аргумент игнорировался, фон всегда был чёрным). Фон вне мира всё равно заливается дымкой пост-пасса (depth = far → fog).
- `resize()` пересоздаёт хранилища цветовых и depth-текстур (ID GL-объектов сохраняются); `getContext()` даёт `RenderContext&`.
- Деструктор освобождает текстуры/фреймбуферы — **требует живого GL-контекста** (см. «Владение GL»).

### Камеры

- `ICamera` — только `getViewMatrix/getProjectionMatrix`.
- `Camera` (fly-cam): `Transform` + перспектива, `controlUpdate(dt, window, focused)` (мышь+клавиатура).
- `OrbitCamera`: орбита вокруг цели (`setTarget/setDistance/rotate/pan/zoom/setPerspective/update`), используется вьювером/эдитором.
- `IsometricCamera`: фиксированный наклон (pitch 55°)/азимут (yaw 45°) вокруг цели, зум дистанцией, `moveTarget` для следования; перспектива с малым FOV (Hades-подобный ракурс для gravelands-клиента). `update()` обязателен после изменений — view-матрица не пересчитывается сама.
- `Viewport` — пустая надстройка над `IRenderTarget`, не используется.

### Шейдеры

- `Shader`: `setPath/setType/setRenderApi/compile()` → `ShaderError {None, InvalidType, FileNotFound, CompilationFailed, LinkFailed}`. Владеет GL-шейдером (`~Shader` удаляет через сохранённый `RenderApi`); ctx — GlobalAllocator + placement new. **Копировать нельзя** (copy удалён), перемещение передаёт владение ctx (источник обнуляется) — нужно `vector<SkinMesh>::resize`.
- `ShaderProgram`: `create/AttachShader/compile/use/unuse/reload`; владеет GL-программой (деструктор удаляет), живёт в глобальном реестре живых программ (hotreload). Copy удалён, move перерегистрирует адрес в реестре.
- **Кеш uniform-локаций** (`getUniformLocation(name)`): первый запрос — в GL, далее из кеша (фикс. массив 16, линейный поиск, без аллокаций); сбрасывается при `reload()`. `RenderContext::send*` и `Material::apply` работают через кеш — `glGetUniformLocation` в hot path больше нет.
- **Hot-reload по консольной команде** (без опроса диска): команды `hotreload`/`reload_shaders` (регистрируются `registerGraphicsConsoleCommands()`) перекомпилируют все живые программы с диска и перелинковывают; при любой ошибке старая программа остаётся рабочей (атомарная замена: свежие объекты собираются во временные, старые удаляются после успеха). В gravelands-клиенте завязано на F5.
- **Пути шейдеров относительные** (`shaderPaths.h`): `Shader::compile` резолвит относительно cwd → каталога exe → подъёмом по родителям exe (candidate и `src\candidate` на каждом уровне, до 8) — работает из любого cwd: dev-запуск из студии (exe в `build\...\Debug`, шейдеры в `<корне>\src\shaders`) и деплой (шейдеры рядом с exe). TODO закрыт.
- Меш-шейдеры: вершинные пробрасывают мировую нормаль и мировую позицию во фрагментный (`Normal`, `WorldPos`; скиннинг — через линейную часть костных матриц); `gShowNormals` — отладочный uniform раскраски нормалями (флаг `RenderContext::showNormals`).
- Ошибки компиляции в `Mesh::bake`/`LineRenderer::bake` **не фатальны**: warning + `baked = true`, объект молча не рисуется.

### Свет (NPR, фаза 4)

- `light.h`: `DirectionalLight {direction, color, intensity}` + `AmbientLight {color, intensity}` — цвета линейные [0,1], в шейдер уходит `color * intensity`. Живут в `RenderContext` (дефолты — мягкий тёплый свет + холодный эмбиент), отправка — `sendLightsToShaderProgram()` (`gLightDir/gLightColor/gAmbientColor`).
- `ICamera::getPosition()` — чистый виртуальный (rim-light/туман); `sendCameraPositionToShaderProgram()` шлёт `gCameraPosition`.
- **sRGB-решение:** рендер линейный, гамма 2.2 — в пост-пассе (`gGammaOutput` в PostProcess), см. ниже.

### Пост-процессинг (NPR, фаза 6)

- `PostProcess` (`postprocess.h` / `impl/postprocess.cpp`): полноэкранный пасс поверх color/depth-текстур сцены (`IRenderTarget::getColorTexture()/getDepthTexture()`), рисует в **текущий** framebuffer (обычно back-буфер; depth-тест внутри отключается/включается). Полноэкранный треугольник генерируется в vertex-шейдере из `gl_VertexID` — VBO не нужен, только пустой VAO.
- `PostProcessSettings`: дымка (`fogColor/fogStart/fogEnd` — тёмная тонировка по линейной глубине, «даль тонет в темноте» — Hades-приём), color grading (lift/gamma/gain + saturation), виньетка, `nearPlane/farPlane` (линеаризация глубины), `gammaOutput` (дефолт **1.0** — конвейер пока НЕ линейный: текстуры авторятся в sRGB, гамма 2.2 даст двойное осветление; полноценный sRGB-конвейер — TODO).
- Шейдеры: `shaders/post/*` (пути — `shaderPaths.h`); компилируются лениво при первом `apply()`, **участвуют в hotreload** (реестр `ShaderProgram`).
- Включение/выключение для сравнения — по усмотрению приложения (в gravelands-клиенте клавиша P; иначе `RenderWindow::blitToBackbuffer` — прямой блит без поста).
- `shaderPaths.h` хранит **абсолютные пути** `M:\Stuff\src\shaders\...` — вьювер работает только при запуске из корня репозитория (TODO: путь относительно exe).

---

## Ассеты

### Mesh

- CPU-данные: `vertices` (Vector3f), `textureCoords`, `normals`, `boneIds` (Vector4i), `boneWeights` (Vector4f), `faces`, `material`.
- GL-атрибуты: position=0, textureCoords=1, boneIds=2 (`glVertexAttribIPointer`, GL_INT), boneWeights=3, normals=5 (VBO заполняется только при `normals.size() == vertices.size()`).
- **Ленивый `bake`** при первом `draw`: VAO + 6 VBO + EBO, компиляция шейдеров, `material.bake(ctx)`, сохранение указателя на `RenderContext`.
- Выбор вершинного шейдера: `boneIds.empty() ? MeshVertexShader : SkinMeshVertexShader`.
- `draw(ctx)` и `draw(ctx, const std::vector<TransformMatrix>* boneMatrices)`; второй путь — скиннинг.
- **`Mesh` некопируем** (copy удалён): владеет сырым `ctx` (`GlobalAllocator`) и GL-ресурсами. **Перемещение безопасно** — ctx передаётся, источник обнуляется (нужно `vector<SkinMesh>::resize`, MoveInsertable). Векторы мешей заполняются через `resize`/`swap` (см. `SkinModel`).

### Material

- Поля: `DiffuseColor`/`AmbientColor`/`SpecularColor`, `hasDiffuseColor`, `diffuse` (Texture), `diffuseImage` (Image), `m_transparencyFactor`, `m_alphaTest`.
- **NPR-параметры (фаза 4):** `shadingMode {Unlit, Toon}`, `rampSoftness`, `rimColor`, `rimPower`, `emission`. `apply(ctx, program)` биндит диффуз (или плоскую белую заглушку при `useDiffuseTextures == false`/без текстуры) + шлёт material-униформы (`gShadingMode`, `gRampSoftness`, `gRimColor`, `gRimPower`, `gEmission`, `gAlphaTest`) через кеш локаций; вызывается из `Mesh::draw`. `m_alphaTest` (порог discard в шейдере, фаза 5) — рабочее поле: 0 = без отсечения.
- **Контур (фаза 8, inverted hull):** `outlineEnabled/outlineWidth/outlineColor`. В `Mesh::draw` второй проход: cull FRONT + `OutlineVertexShader`/`SkinOutlineVertexShader` (раздув позиции вдоль нормали на `outlineWidth`, скиннинг-вариант тянет кости) + плоский цвет (`OutlineFragmentShader`). См. «Подводные камни» — culling теперь включается на отрисовку мешей.
- `loadFromAssimpMaterial(material, folder, scene)`: диффузный цвет (fallback) + диффузная текстура.
- Диффузная текстура: `"*<N>"` — встроенная в сцену (`aiScene::mTextures`); иначе внешний файл (`Folder::down` + `stbi_load`); если файла рядом нет — повторный поиск встроенной **по имени файла** (кейс Mixamo: материал ссылается на путь билд-сервера Adobe).
- Встроенные: сжатые (`mHeight == 0`, `mWidth` = размер в байтах) через `stbi_load_from_memory`; несжатые `aiTexel` (BGRA) тоже поддержаны. `stbi_set_flip_vertically_on_load(1)`.
- `bake(ctx)`: пустое изображение + `hasDiffuseColor` → синтез 1×1 текстуры из цвета; иначе GL-текстура не создаётся (в `Mesh::draw` будет белая заглушка).
- `MaterialError {None, TextureLoadFailed, UnsupportedFormat, NotImplemented}`; >1 диффузной текстуры — `NotImplemented`.
- Деструктор освобождает GL-текстуру через сохранённый `pRenderContext`.

### Texture / Image

- `Texture`: `create(Image, ctx, genFlags)`, `free(ctx)`, `resize`, `update`, `makeTextureAtlas` (заглушка), `getContext()` (`textureID`). **Удаляется только через `free(ctx)`** — деструктор лишь предупреждает об утечке.
- `Image` — CPU-битмап RGBA (`std::vector<Color>`, row-major), `operator[]` → `UnsafeSlicer`; **индексация `image[x][y]` — x = колонка, y = строка** (см. image.cpp, легко перепутать). `Color::Transparent` = `(0,0,0,0)` (был баг: чёрный непрозрачный — ломал TGX/GM1-заливки и alpha-test); загрузка TGX; `getData()`/`update()`.

### Форматы

- `TGXFile`/`GM1File` — парсеры форматов серии Stronghold (TGX — картинки, GM1 — контейнер с палитрой). Используются gravelands-клиентом (`tiles.gm1`).

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

- `SkinModel` = `Skelet` + `Animator` + `vector<SkinMesh>`; `getMeshes()` — прямой доступ к мешам (настройка NPR-материалов/контуров; структуру вектора не менять).
- `loadFromAssimp(scene, filename, animationScene = nullptr)`: скелет → аниматор (из `animationScene` или основной сцены) → меши + материалы.
- `replaceMeshesFromAssimp(scene, filename, force = false)`: атомарная подмена скина — отказ при нуле мешей (force не отменяет); скелет-кандидат грузится только для `isCompatibleWith`; при несовместимости обычный режим отказывает, `force = true` — warning, `adoptOffsetMatricesFrom` + `computeBindPose` (retarget bind-позы: меш ведёт себя как нативно скиннутый к текущему ригу) и продолжение (кандидат пробрасывается в `SkinMesh` для remap-а весов); новые меши во временном векторе, затем `swap`.
- `update(dtMs)`: `animator.update` + `timeTicks = (currentTimeMs/1000)*tickPerSecond` + `skelet.applyClip`.
- `draw(ctx)`: `modelTransform * meshTransform` на каждый меш + `mesh.draw(ctx, &finalMatrices)`.
- `SkinMesh::loadFromAssimpMesh(aiMesh*, const Skelet&, force = false, candidateSkelet = nullptr)` — раскладка весов: кость ищется по имени; на вершину максимум 4 веса (лишние отбрасываются с warning); `boneIndex >= __blib_max_bones` — ошибка всегда. Неизвестная кость: обычный режим — ошибка; `force = true` — веса переносятся на ближайшего предка из иерархии `candidateSkelet`, существующего в текущем скелете (у Mixamo ленты `Ribbon*` висят на Head); если предка нет — отброс весов + **ренормализация** затронутых вершин к сумме 1 (вершины с нулевой суммой остаются нулевыми, warning).

### Assimp-флаги (важно)

Загрузка файлов живёт в `beng-client` (`SkinnedMeshComponent`): `aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_PopulateArmatureData`. **`PopulateArmatureData` обязателен** — без него `aiBone::mArmature`/`mNode` пустые и скелет не грузится. `aiScene` живёт только внутри функции загрузки — `SkinModel` копирует нужное.

---

## Отладка и прочее

- `Sphere` (`sphere.h` / `impl/sphere.cpp`): процедурная UV-сфера (`createSpere(radius, pointPerCircle, color)` — число сегментов экватора, кольца = половина), нормали = нормализованные позиции, UV по долготе/широте, однотонная 1×1 диффуз-текстура из цвета. `draw()` (const, контракт IDrawable) синхронизирует трансформ сферы в меш (`sphereMesh` — `mutable`, как `SkinModel`), поэтому позиция задаётся `setPosition` на самой сфере. `getMesh()` возвращает `const Mesh&` (Mesh некопируем).
- `SpritePlane` (`spritePlane.h` / `impl/spritePlane.cpp`): вертикальный unlit-квад с alpha-test (NPR-гибрид, фаза 5). `create(width, height, image)` строит квад в локальной плоскости XY (нормаль +Z, нуль — «нога»), на камеру выставляется поворотом вокруг Y (`setRotation`; конвенция `rotateY`: локальная +Z → `(-sin(yaw), 0, cos(yaw)`), при фиксированной камере billboarding не нужен. Материал: `ShadingMode::Unlit` + `m_alphaTest` (по умолчанию 0.5) — discard в шейдере даёт жёсткие края; сортировка не требуется (depth-test), полупрозрачный блендинг не используется. `setAlphaTest()` — порог.
- `BlobShadow` (`blobShadow.h` / `impl/blobShadow.cpp`): мягкая blob-тень (NPR, фаза 7) — горизонтальный квад в XZ (нормаль +Y) с градиентной текстурой (альфа = мягкость). `draw()` включает альфа-блендинг (SRC_ALPHA/ONE_MINUS_SRC_ALPHA) и отключает запись глубины (без z-fighting с землёй), затем восстанавливает состояние. Отрисовка — после непрозрачной земли, до персонажей. Позиция — `setPosition` с подъёмом над землёй (y-сдвиг). Альфа текстуры проходит во `FragColor.a` (MeshFragmentShader) — для непрозрачных текстур alpha = 1, поведение прежнее.
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
- `Shader`/`ShaderProgram` владеют GL-объектами (деструкторы удаляют через сохранённый `RenderApi`); ctx — GlobalAllocator. Не копировать (copy удалён), move — безопасен.

---

## Подводные камни

- **Face culling включается в `Mesh::draw`** (фаза 8): pass 1 — лицевые (cull back), контур — задние (cull front), после — disable. Раньше culling не было: меши обязаны иметь корректную обмотку (CCW снаружи), иначе исчезнут. `LineRenderer`/пост-пасс не затронуты (рисуют со своими состояниями).
- **Односторонние плоскости (SpritePlane) + culling:** видима только лицевая грань — разворот обязан смотреть нормалью НА камеру (конвенция `rotateY`: +Z → `(-sin(yaw), 0, cos(yaw))`, см. spritePlane.h). Ошибка на 180° теперь видна сразу (грань отсекается), а не маскируется двусторонней отрисовкой.
- Шейдеры резолвятся из cwd → каталога exe → подъёмом по родителям — см. «Шейдеры»; абсолютные пути больше не используются.
- `RenderContext::send*Matrix*` без камеры — падение (нет проверки `pCamera`).
- `IRenderTarget::clear` игнорирует цвет и переключает FBO-кольцо; порядок `clear`/`draw` нельзя чередовать между таргетами.
- `Mesh`/`Material`/`Texture` нельзя копировать (сырые GL-хендлы и указатели контекста); `Mesh` поддерживает перемещение (см. выше).
- Скиннинг: **лимит 100 костей** продублирован в C++ (`__blib_max_bones`), в `skinmesh.cpp` и в `SkinMeshVertexShader.glsl` — при рассинхроне молчаливое отсечение; >4 весов на вершину теряются.
- `SkinModel::draw` мутирует трансформы мешей в `const`-методе (`meshes` — `mutable`) — не потокобезопасно.
- Внешняя анимация без `bindClipToSkeleton` частично не применяется (см. `../../beng/BENG.md`).
- Шейдеры мешей/линий: ошибка компиляции не фатальна — объект молча не рисуется, ищите warning в консоли.

---

## TODO

- [ ] Обсудить каталог известных багов модуля (отдельная задача).
- [ ] sRGB-конвейер: GL_SRGB8_ALPHA8-текстуры + линейная работа + гамма 2.2 в пост-пассе (сейчас gammaOutput = 1.0, см. «Пост-процессинг»).
- [ ] MSAA/разрешение рендера (FXAA в пост-пассе) — сейчас сглаживания нет, ramp/контуры будут алиасить.
- [ ] Триангуляция/рендер мешей — TODO в `mesh.cpp`.
- [ ] Атлас текстур (`Texture::makeTextureAtlas` — заглушка).

---

## Связанные доки

- `../BLIB.md` — общая карта и философия blib.
- `../core/CORE.md` — math, streams, console (зависимость graphics).
- `../../beng/BENG.md` — ECS-компоненты (`SkinnedMeshComponent`, `AnimatorComponent`), панели, кадр вьювера.
- `AGENTS.md` — правила проекта (аллокации, логирование, касты).
