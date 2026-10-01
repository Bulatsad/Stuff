# GRAPHICS — blib-graphics

> Слой: `blib`. OpenGL/WGL, окно, ассеты, скелетная анимация, ImGui.
> Шпаргалка по инвариантам, владению GL и граблям. **Обновлять при изменениях кода модуля** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-10-01

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
| Сериализация ассетов (ISaveLoadable) | `impl/mesh.cpp`, `impl/material.cpp`, `impl/image.cpp`, `impl/bone.cpp`, `impl/skelet.cpp`, `impl/animator.cpp`, `impl/skinmodel.cpp` (см. раздел «Сериализация») |
| Отладочные линии | `lineRenderer.h`, `impl/lineRenderer.cpp` |
| Консольное окно | `console/consoleWindow.h` |
| Форматы TGX/GM1 | `tgx.h`, `gm1.h`, `impl/tgx.cpp`, `impl/gm1.cpp` |
| Трансформации | `transform.h`, `transformable.h`, `transformMatrix.h`, `impl/transformable.cpp`, `impl/transformMatrix.cpp` |
| GLSL-шейдеры | `src/shaders/mesh/*.glsl`, `src/shaders/line/*.glsl` (пути — `shaderPaths.h`) |

---

## Поток кадра

1. **Окно:** `RenderWindow(width, height, title, style)` создаёт Win32-окно и WGL-контекст; `update()` качает сообщения, `isOpen()`, `close()`, `swapBuffers()`. `__getCtx()` даёт платформенный `WinCtx` (hwnd и т.д.). Дефолтный `RenderWindow()` — **headless** (без окна/контекста; PIE-клиент, см. «Владение GL»).
2. **Кадр таргета:** `IRenderTarget::clear(color)` биндит очередной FBO из кольца и очищает его; `draw(const IDrawable&)` вызывает `drawable.draw(rc)`. `rc` — публичное поле таргета.
3. **Отрисовка:** `RenderContext::setShaderProgram`, отправка матриц (`gViewMatrix`, `gProjectionMatrix`, `gModelMatrix`) и костей (`gBones`), бинд текстуры. Матрицы column-major → уходят с `transpose = GL_FALSE` (без транспонирования; см. «Конвенция матриц» в `../core/CORE.md`).
4. **UI:** ImGui-кадр рисуется в back-буфер (`ImGui_ImplOpenGL3_*`); порядок «сцена в FBO → UI → swap» — ответственность приложения (см. `../../beng/BENG.md`, кадр вьювера).

### RenderContext

- Поля: `transform`, `pCamera`, `vievMatrix` (опечатка в API), `projectionMatrix`, `lastShader`, `api` (копия указателей GL), `useDiffuseTextures`, `showNormals` (отладочная раскраска нормалями, uniform `gShowNormals` — шейдеры без него игнорируют), `directionalLight`/`ambientLight` (NPR-свет).
- `setCamera` только сохраняет указатель; `sendVievMatrixToShaderProgram`/`sendProjectionMatrixToShaderProgram` **разыменовывают `pCamera` без проверки** — камеру обязательно задать до отрисовки.
- `sendBoneMatricesToShaderProgram` шлёт `finalMatrices` скелета и обрезает их до `__blib_max_bones` (100).
- `getFlatWhiteTexture()` — ленивая статическая 1×1 белая GL-текстура; подставляется, когда `useDiffuseTextures == false` или у материала нет текстуры.
- `applyTransform` — пустая заглушка (не полагаться).

### IRenderTarget (FBO)

- Конструктор `IRenderTarget(width, height, frameBuffersCount = 2)`; по умолчанию **кольцо из 2 FBO**. `clear()` переключает индекс кольца на следующий.
- `bind()` — публичный бинд **текущего** кадрового буфера + `glViewport` + depth-test **без очистки и смены индекса** (то, что `clear()` делает первым шагом). Нужен хостам, рисующим в чужой таргет в середине кадра (Game-превью эдитора рендерит в свой FBO в `onSceneDidUpdate`, затем обязан вернуть FBO вьюпорта каркасу — иначе `drawGizmo`/следующие проходы рисуют в чужой бинд).
- Аттачменты: цветовая RGBA-текстура + **depth-текстура** (GL_DEPTH_COMPONENT24, сэмплируется пост-процессингом; раньше был depth/stencil renderbuffer). Доступ — `getColorTexture()`/`getDepthTexture()` (текстуры **текущего** кадрового буфера, валидны после `clear()`).
- **Грабли:** между `clear()` и `draw()` одного таргета нельзя очищать/рисовать другой таргет (переключение глобального GL-контекста — предупреждение в `rendertarget.h`); смена таргета в середине кадра требует явного `bind()` возврата.
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

- `light.h`: `DirectionalLight {direction, color, intensity}` + `AmbientLight {color, intensity}` — цвета линейные [0,1], в шейдер уходит `color * intensity`. Живут в `RenderContext` (дефолты — мягкий тёплый свет + холодный эмбиент), отправка — `sendLightsToShaderProgram()` (`gLightDir/gLightColor/gAmbientColor`). **Ведомый сценой режим:** в beng-client свет описывается компонентами (`DirectionalLightComponent`/`AmbientLightComponent`), `LightSystem` копирует их в rc каждый кадр — см. BENG.md «beng-client».
- `ICamera::getPosition()` — чистый виртуальный (rim-light/туман); `sendCameraPositionToShaderProgram()` шлёт `gCameraPosition`.
- **sRGB-решение:** рендер линейный, гамма 2.2 — в пост-пассе (`gGammaOutput` в PostProcess), см. ниже.

### Пост-процессинг (NPR, фаза 6)

- `PostProcess` (`postprocess.h` / `impl/postprocess.cpp`): полноэкранный пасс поверх color/depth-текстур сцены (`IRenderTarget::getColorTexture()/getDepthTexture()`), рисует в **текущий** framebuffer (обычно back-буфер; depth-тест внутри отключается/включается). Полноэкранный треугольник генерируется в vertex-шейдере из `gl_VertexID` — VBO не нужен, только пустой VAO.
- `PostProcessSettings`: дымка (`fogColor/fogStart/fogEnd` — тёмная тонировка по линейной глубине, «даль тонет в темноте» — Hades-приём), color grading (lift/gamma/gain + saturation), виньетка, `nearPlane/farPlane` (линеаризация глубины), `gammaOutput` (дефолт **1.0** — конвейер пока НЕ линейный: текстуры авторятся в sRGB, гамма 2.2 даст двойное осветление; полноценный sRGB-конвейер — TODO).
- Шейдеры: `shaders/post/*` (пути — `shaderPaths.h`); компилируются лениво при первом `apply()`, **участвуют в hotreload** (реестр `ShaderProgram`).
- Включение/выключение для сравнения — по усмотрению приложения (в gravelands-клиенте клавиша P; иначе `RenderWindow::blitToBackbuffer` — прямой блит без поста).
- `shaderPaths.h` хранит **абсолютные пути** `M:\Stuff\src\shaders\...` — вьювер работает только при запуске из корня репозитория (TODO: путь относительно exe).

---

## Трансформации и матрицы (`transformMatrix.h` / `impl/transformMatrix.cpp`)

- **Конвенция — column-major + column-vectors** (единая, см. `../core/CORE.md`, «Конвенция матриц»): хранилище `data[столбец][строка]`, трансляция — в последней колонке `data[3][0..2]`, загрузка в GL — `GL_FALSE`. Раньше матрицы хранились транспонированными + `GL_TRUE` (компенсация на границе GL) — сломаны были `decomposeMatrix` и иерархии beng; с 2026-09-25 транспонированное хранение запрещено.
- `composeMatrix(position, rotation, scale)` — стандартная TRS: столбцы 0..2 = колонки R·S, колонка 3 = трансляция. Дубликат в `beng/components/transform.cpp` (`composeTrsMatrix`, beng-core не зависит от graphics).
- `mul(lhs, rhs)` — **алиас** `Matrix::operator*` (стандартное произведение `lhs * rhs`, НЕ транспонированное). Используется в `skelet.cpp`: `global = mul(parent.global, local)`, `finalMatrices = mul(global, offsetMatrix)` — обе формулы теперь стандартные.
- `lookAt(camera, target, worldUp)` — правая система: `f = normalize(target-camera)`, `s = normalize(cross(f, up))`, `u = cross(s, f)`; столбцы — образы осей, трансляция `(-dot(s,eye), -dot(u,eye), dot(f,eye))` в последней колонке. Вырождается при взгляде вдоль up (камеры не доводят pitch до ±90°).
- **Конверсия Assimp-матриц:** `aiMatrix4x4` — row-major (трансляция в `a4,b4,c4`); `transformFromAssimp` (skelet.cpp) и `Bone::loadFromAssimp` читают построчно. У Mixamo-FBX трансформы узлов (в т.ч. декомпозиция `_$AssimpFbx$_Translation/PreRotation/Rotation`) и offsets — обычные column-vector матрицы; раньше конверсия транспонировала их, и после перехода на column-major «танцор» взрывался. Регресс — `testSkinMesh.cpp`.
- `rotateX/Y/Z(matrix, angle)` — `matrix * R(angle)`, углы в **градусах**; матрицы R — стандартные (через initializer_list).
- `decomposeMatrix(matrix, transform)` — извлекает TRS из стандартной матрицы: position = `data[3][0..2]`, scale = длины столбцов 0..2, rotation = Euler **X·Y·Z в градусах** (согласовано с `ITransformable::getTransform` = `X·Y·Z·T`; при нулевом масштабе — нулевые углы). Тесты: `blib/test/.../testTransformMatrix.cpp`.

### ITransformable (`transformable.h` / `impl/transformable.cpp`)

- `getTransform()` лениво собирает `X·Y·Z·T` (трансляция — последняя колонка) из `Transform`-полей; `setTransform(m)` ставит матрицу и разбирает её `decomposeMatrix`.
- `transform(point)` — точка как вектор-столбец `(x, y, z, 1)`; стандартное произведение сохраняет трансляцию.
- Углы `Transform` — в **градусах** (`rotateX` — `fmod(..., 360)`).

---

## Ассеты

- **Все сериализуемые ассеты — `blib::memory::IAllocatorAware`** (2026-09-24): `Mesh`, `SkinMesh`, `Material`, `Image`, `SkinModel`, `Skelet`, `Bone`, `Animator`, `AnimationClip`. Дефолтный аллокатор — `DefaultAllocator` (прокси к GlobalAllocator), поэтому поведение без `setAllocator` не изменилось. Каждый класс несёт `static constexpr const char* resourceTypeName` («blib.graphics.Mesh» и т.д.) — стабильный тег типа для кеша ресурсов blib-core (`RESOURCE_MANAGER.md`; сравнение тегов — по содержимому строки, не по адресу литерала).
- **Контракт IAllocatorAware:** конструктор не аллоцирует динамическую память (аллокатор настраивается после конструирования), `deallocate` идёт через тот же аллокатор, что и `allocate`. Реализовано для `Mesh` (см. ниже); остальные классы аллоцируют только через std-контейнеры (см. «Долг»).
- **Вложенные ресурсы** (`SkinModel` → `SkinMesh`/`Mesh`/`Material`/`Image` → …) имеют **собственные** аллокаторы: `setAllocator` на корне НЕ пропагируется вниз. При v1-аллокаторе кеша (DefaultAllocator) источник памяти у всех один — GlobalAllocator; пропагация понадобится, только когда кеш раздаёт не-дефолтные аллокаторы (см. TODO).
- **Долг (TODO):** std-контейнеры ассетов (`Mesh::vertices`, `Image::bitmap`, `Bone::chain` и т.д.) остаются на std::allocator. Перевод на `StdAllocatorAdapter` с self-pointing аллокатором требует явной rule-of-five на каждом классе (move — element-wise move-assign в теле: move-ctor вектора украл бы буфер с адаптером-указателем на аллокатор источника → висячий указатель; см. SYSTEM.md, грабля `PoolAllocatorImpl`).

### Mesh

- CPU-данные: `vertices` (Vector3f), `textureCoords`, `normals`, `boneIds` (Vector4i), `boneWeights` (Vector4f), `faces`, `material`.
- GL-атрибуты: position=0, textureCoords=1, boneIds=2 (`glVertexAttribIPointer`, GL_INT), boneWeights=3, normals=5 (VBO заполняется только при `normals.size() == vertices.size()`).
- **Ленивый `bake`** при первом `draw`: VAO + 6 VBO + EBO, компиляция шейдеров, `material.bake(ctx)`, сохранение указателя на `RenderContext`.
- **Ленивый `ctx` (2026-09-24):** GL-контекст (`oglMeshContext`) аллоцируется в `bake` через `IAllocatorAware::allocate` (конструктор пуст — контракт кеша ресурсов). `allocate()` не-const → в const-`bake` снятие const через `const_cast` (кеш GL-состояния, как `mutable baked`).
- Выбор вершинного шейдера: `boneIds.empty() ? MeshVertexShader : SkinMeshVertexShader`.
- `draw(ctx)` и `draw(ctx, const std::vector<TransformMatrix>* boneMatrices)`; второй путь — скиннинг.
- **`Mesh` некопируем** (copy удалён): владеет сырым `ctx` (аллокатор IAllocatorAware) и GL-ресурсами. **Перемещение безопасно** — ctx передаётся, источник обнуляется (нужно `vector<SkinMesh>::resize`, MoveInsertable). Аллокатор: у `IAllocatorAware` пользовательский dtor подавляет implicit move-ctor, поэтому `std::move(other)` в move-ctor связывается с copy-ctor базы — share() (обе стороны делят impl по ref-counting; `deallocate` ctx пойдёт через тот же impl). Векторы мешей заполняются через `resize`/`swap` (см. `SkinModel`).

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

- `TGXFile`/`GM1File` — парсеры форматов серии Stronghold (TGX — картинки, GM1 — контейнер). **Layout GM1 выверен по референс-декомпилу Stronghold Image Toolbox (ilspycmd) и файлам Stronghold/Crusader Extreme:** заголовок 22×u32 (Image_Count@12, Data_Type@20, Data_Size@80, Unknown18@84), палитра 2560×u16 @88, затем offsets (u32, **относительные от начала блока данных**), sizes, заголовки кадров 16 байт (width/height/horizontalOffset/verticalOffset/part/subparts/baseHeight/direction/horizontalStartOffset/widthInGame/performanceId). Данные кадров по Data_Type: 1/4/6 — 16-бит RGB555 в TGX-токенах; 2 — индексы палитры (1 байт, индекс 0 — прозрачный); 3 — 512 байт ромб-тайла 30×16 (строки ромба 2,6,…,30,…,2, центрированы) + TGX-часть `widthInGame × (baseHeight+7)`; 5 — raw 16 бит `w×(h−7)`; 7 — raw 16 бит `w×h`. **Тайлы кадров Building отдаются отдельным списком** (`getImageTiles()` — ромб 30×16 с прозрачными углами, индекс-в-индекс с images; декодируются для КАЖДОГО кадра, даже когда есть TGX-часть) — композит «здание + земля» по offsets собирает потребитель (sc2img). `getHeader()` отдаёт разобранный заголовок (Data_Type нужен для пост-обработки). `GM1File` поддерживает переопределение палитры (`playerColorOverridden`/`playerColorOverride`). Валидация: quantity ≤ 4096, стороны кадров 1..4096, offset+size ≤ Data_Size — битые файлы дают пропуск кадра (пустой 1×1), не исключение. Потребитель — утилита sc2img (`src/misc/sc2img`, см. SC2IMG.md).

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
- `getBonePosition(name, out)` — позиция кости в model-space для привязки эффектов к позе (blob-тени — см. BlobShadowSystem в beng-client); false, если кость не найдена. Требует `computeBindPose`/`applyClip`. **Грабли:** позиция — трансляция `globalTransform` кости, а НЕ `finalMatrices` (`finalMatrices = global * inverse-bind` — «дельта» от bind-позы, почти ноль).
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

- `Sphere` (`sphere.h` / `impl/sphere.cpp`): процедурная UV-сфера (`createSpere(radius, pointPerCircle, color)` — число сегментов экватора, кольца = половина), нормали = нормализованные позиции, UV по долготе/широте, однотонная 1×1 диффуз-текстура из цвета. `draw()` (const, контракт IDrawable) синхронизирует трансформ сферы в меш (`sphereMesh` — `mutable`, как `SkinModel`), поэтому позиция задаётся `setPosition` на самой сфере. `getMesh()` возвращает `const Mesh&` (Mesh некопируем). **`takeMesh()`** — передаёт меш по move в ECS-рендер-компонент (после вызова объект пуст); примитивы служат источниками мешей, а не drawable-объектами мира.
- `SpritePlane` (`spritePlane.h` / `impl/spritePlane.cpp`): вертикальный unlit-квад с alpha-test (NPR-гибрид, фаза 5). `create(width, height, image)` строит квад в локальной плоскости XY (нормаль +Z, нуль — «нога»), на камеру выставляется поворотом вокруг Y (`setRotation`; конвенция `rotateY`: локальная +Z → `(-sin(yaw), 0, cos(yaw)`), при фиксированной камере billboarding не нужен. Материал: `ShadingMode::Unlit` + `m_alphaTest` (по умолчанию 0.5) — discard в шейдере даёт жёсткие края; сортировка не требуется (depth-test), полупрозрачный блендинг не используется. `setAlphaTest()` — порог. **`takeMesh()`** — меш уходит в ECS (`RenderLayer::AlphaTested`).
- `BlobShadow` (`blobShadow.h` / `impl/blobShadow.cpp`): мягкая blob-тень (NPR, фаза 7) — горизонтальный квад в XZ (нормаль +Y) с градиентной текстурой (альфа = мягкость). `draw()` включает альфа-блендинг (SRC_ALPHA/ONE_MINUS_SRC_ALPHA) и отключает запись глубины (без z-fighting с землёй), затем восстанавливает состояние — при использовании в ECS это делает `RenderSystem` для всего слоя `Shadow` (см. BENG.md). Позиция — `setPosition` с подъёмом над землёй. **`takeMesh()`** — меш уходит в ECS (`RenderLayer::Shadow`). Альфа текстуры проходит во `FragColor.a` (MeshFragmentShader) — для непрозрачных текстур alpha = 1, поведение прежнее.
- `LineRenderer`: `addLine(start, end, color)`, `clear()`, `draw(ctx)`; VAO/VBO, ленивый bake, GL_LINES, перезаливка `GL_DYNAMIC_DRAW`. GL-ресурсы и шейдеры не освобождаются.
- `ConsoleWindow` (`console/consoleWindow.h`): ImGui-окно консоли — презентация над `blib::console::Console`; единственный консюмер вывода.
- `CoordinateSystemXYZ` — заглушка конвертации координат.
- `keyboard.h`/`mouse.h` — опрос состояния Win32 (`GetAsyncKeyState`/`GetCursorPos`), `Keyboard::update()` раз в кадр; опрос идёт вне зависимости от фокуса окна.

---

## Владение GL и порядок разрушения

- `RenderContext` принадлежит `IRenderTarget` по значению; каждый таргет загружает GL-указатели в конструкторе.
- **Multi-window в одном процессе (PIE):** каждое `RenderWindow` владеет своим HGLRC; `update()` и `swapBuffers()` делают контекст своего окна текущим (`makeCurrent()` — кэш по `wglGetCurrentContext`, `wglMakeCurrent` вызывается только при реальной смене окна). Контракт: хост, тикающий ВЛОЖЕННОЕ окно, обязан вернуть контекст СВОЕГО окна перед продолжением рендера (`EditorApplication::tick` вызывает `window.makeCurrent()` после хука хоста). Создавать новое окно в середине ImGui-кадра нельзя — только в начале кадра. Ограничение: GL-объекты НЕ разделяются между контекстами (у каждого окна свои FBO/VAO/текстуры); ImGui-бэкенд привязан к одному окну.
- **Headless-`RenderWindow` (`RenderWindow()`):** окно и GL-контекст НЕ создаются (`ctx = nullptr`); все методы — безопасные no-op (`update/swapBuffers/blitToBackbuffer/close/display/makeCurrent`), `isOpen()` — false, `__getCtx()` — nullptr. Нужен клиенту игры в PIE: он рендерит в свой FBO в контексте хоста (эдитора), а кадр показывает Game-вкладка (см. GRAVELANDS.md, «PIE»). Требование к хосту: `makeCurrent()` вызывать нельзя — контекст уже текущий.
- **PIE-клиент — без второго контекста:** headless-клиент Gravelands рендерит в FBO, созданный в контексте эдитора (FBO/textures живут в том же контексте, кадр сэмплится ImGui-ом эдитора); multi-window остаётся только теоретической возможностью (контракт `makeCurrent` сохранён).
- `Mesh`: `ctx` — аллокатор `IAllocatorAware` (лениво в `bake`, освобождается в `~Mesh` тем же аллокатором), GL-ресурсы (VAO/VBO/EBO) — лениво в `bake`, освобождаются в `~Mesh`, если сохранён `RenderContext*`.
- `Material`: GL-текстура — в `bake`, освобождается в деструкторе через `pRenderContext`.
- **Порядок обязателен: модели/меши/материалы выгружаются раньше окна и рендер-таргета** (иначе GL-контекст уже мёртв). Так сделано в `ViewerCore::shutdown` и `SkinnedMeshComponent`.
- `Texture` живёт до явного `free(ctx)`; копирование `Texture` поверхностное (копия и оригинал указывают на один GL-объект) — не копировать.
- `Shader`/`ShaderProgram` владеют GL-объектами (деструкторы удаляют через сохранённый `RenderApi`); ctx — GlobalAllocator. Не копировать (copy удалён), move — безопасен.

---

## Сериализация (ISaveLoadable)

- **Классы:** `Image`, `Material`, `Mesh`, `SkinMesh`, `Bone`, `Skelet`, `AnimationClip`, `Animator`, `SkinModel` — полный контракт `blib::core::ISaveLoadable`: `toJson()`/`fromJson()` (JSON-DOM) + тонкие `save`/`load` поверх + `strongCompare` + `verify` (`verifyRoundTrip`). Ключи JSON — именованные `constexpr` в `.cpp`. `SkinModel` сериализует ВСЁ содержимое модели: скелет (иерархия по `parentIndex`/`rootIndex`, цепочки, веса, offset/local/global-матрицы), меши (геометрия + веса + материал с битмапом диффуза), аниматор (клипы: каналы с ключами pos/rot/scale, `boneChains`; состояние плейбека).
- **НЕ сериализуемо (контекст/GL):** `Mesh::ctx`, шейдеры, `baked`; `Material`-текстуры (`Texture` — GL-хендл) и `pRenderContext`; `Bone::node` (`aiNode*` — контекст Assimp-загрузки). Диффузная текстура сериализуется как **CPU-битмап** `diffuseImage` — GL-текстура пересоздаётся `bake()`.
- **JSON-восстановленный скелет полностью работоспособен:** рантайм (`applyClip`/`computeBindPose`/`getBonePosition`) зависит только от `chain`/матриц/иерархии, не от `node`. `fromJson` восстанавливает иерархию `IHierarchal` по индексам и пересчитывает `finalMatrices` через `computeBindPose` (они не сериализуются).
- **Семантика `fromJson`/`load`:** валидация ВСЕХ полей до применения (при ошибке — `LoadStatus::InvalidData`, состояние не меняется). `Mesh`/`SkinMesh` пересоздаются destroy + placement new (move-присваивание удалено): GL-кэш старой геометрии невалиден после перезаписи CPU-данных, свежий меш перезапечётся в `draw()`.
- **ГРАБЛИ: перезапись ЗАПЕЧЁННОГО меша/модели требует живого `RenderContext`** — деструктор старого состояния освобождает GL-ресурсы через сохранённый контекст (см. «Владение GL»). Свежие/незапечённые объекты и `verify()` безопасны без контекста.
- Формат матриц: 16 чисел в порядке `Matrix(std::initializer_list)` — `data[0][0], data[1][0], …, data[3][3]`; кватернионы — `[x, y, z, w]`.
- beng использует эту сериализацию: `MeshRenderComponent` делегирует `Mesh`, `SkinnedMeshComponent` — `SkinModel` (см. BENG.md).

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
- [ ] Перевод std-контейнеров ассетов на `StdAllocatorAdapter` с аллокатором `IAllocatorAware` (rule-of-five: move — element-wise move-assign; см. раздел «Ассеты», «Долг»).
- [ ] Пропагация `setAllocator` вложенным ресурсам (SkinModel → меши → материалы → изображения) при загрузке — нужна только для не-дефолтных аллокаторов кеша ресурсов.
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
