# TESTING — blib/test

> Слой: `blib`. Фреймворк `blib::test` и группы тестов (CTest).
> Шпаргалка по устройству тестов. **Обновлять при изменениях фреймворка/групп** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-09-24

---

## Назначение и границы

- Header-only фреймворк для юнит-тестов blib и beng + набор групп, регистрируемых в CTest.
- Собирается только при `BUILD_TESTS=ON` (`cmake -B ../build -DBUILD_TESTS=ON`).
- Прогон: `ctest --test-dir ../build -C Debug` (из `src/`).
- Не в этом доке: что именно проверяет каждая группа по существу — смотрите сами тесты; здесь — карта и конвенции.

---

## Фреймворк `blib::test`

- Заголовок: `src/blib/test/src/test.h`; `main()` — `src/blib/test/src/impl/main.cpp` (`BLIB_TEST_MAIN`).
- **Макросы:**
  - `BLIB_TEST_CASE(name)` — объявляет тест; регистрация происходит на статической инициализации (саморегистрация).
  - `BLIB_TEST_CHECK(expr)` — проверка; при провале тест продолжается.
  - `BLIB_TEST_REQUIRE(expr)` — проверка с немедленным `return` из теста.
  - `BLIB_TEST_CHECK_CLOSE(a, b, eps)` — сравнение с допуском.
  - `BLIB_TEST_KNOWN_FAILURE(reason)` — пометить кейс как XFAIL (объявлен, но сейчас не используется).
  - `BLIB_TEST_REQUIRE_THROWS` / `REQUIRE_NOTHROW` — объявлены, не используются (в проекте нет исключений).
- **Запуск:** `BLIB_TEST_MAIN` включает stdout-эхо консоли, печатает `[i/N] <name> ...`, ловит исключения, выводит `PASSED`/`FAILED`, итог `Results: X/Y passed`; код возврата `0` при успехе, `1` иначе.
- Тесты пишут в `Console` (логи) — вывод виден в stdout.

---

## Группы тестов

Список групп — в `src/blib/test/CMakeLists.txt` (имя группы = суффикс `src/impl/test<group>.cpp`; исключение — группа `json`, разбитая на несколько файлов через `blib_test_group_sources_<group>`):

| Группа | Что покрывает |
|--------|---------------|
| `angle` | `AngleDegree`/`AngleRadian`: нормализация, конверсии, операторы |
| `matrix` | `Matrix<T,W,H>`: identity, initializer_list, `Transpose`, `+ - *` |
| `quaternion` | Конструкторы, axis-angle, `normalize/conjugate/inverse/rotate` |
| `vector` | `Vector<T,N>`: доступ, операторы, `dot/cross/magnitude/normalize` |
| `pdl` | PDL-парсер: `demo*.pdl`, `parse/parseNext`, команды/опции, ошибки |
| `allocator` | GlobalAllocator, Default/Malloc/Pool/Debug, type-erased `Allocator` (copy/move/clone: stateless-копии, ref-counting stateful — разделение состояния пула, цепочка копий, копии переживают оригинал, промоция из const-источника, deepCopy copyable/move-only, GA-баланс), `StdAllocatorAdapter`; итераторы `PoolAllocator` (пустой/свободный пул, пропуск свободных, chunk-major порядок, iterator_traits/std-алгоритмы, const-итерация, деаллоцируемость `*it` — в release и через debug-обёртку), битмап под нагрузкой (churn, разные чанки), release-детекты double-free/чужого ptr |
| `typeerased` | `ITypeErased`: пустое состояние, `construct` (в т.ч. несколько ctor-аргументов, повторный поверх занятого — старое содержимое уничтожается), уничтожение при выходе из scope и через виртуальный dtor при удалении по указателю на базу, кастомный аллокатор (MallocAllocator через SBO `Allocator`), отсутствие утечек (статистика GlobalAllocator); copy/move value-семантика базы — static_assert copyable/movable, copy-ctor/copy-assign/move-ctor/move-assign (move опустошает источник, fresh-аллокация, счётчики деструкторов); пример-наследник `TypeErasedValue` (emplace/get/несовпадение типа); наследник `CopyableValue` — copy/move ctor'ы и `operator=` через хелперы `copyConstructFrom`/`moveConstructFrom` (deep copy независимость, copy/move/assign пустого, self-assign guard, move-only и non-movable типы — недоступная операция даёт пустую копию, счётчики деструкторов, без утечек) |
| `allocatoraware` | `IAllocatorAware`: дефолтный аллокатор, `setAllocator` подменяет источник памяти (счётчик stateful-аллокатора, память через MallocAllocator — детерминированно мимо GlobalAllocator), share-семантика копирования (`setAllocator` от одного источника → два объекта делят impl), `getAllocator` как живой аллокатор |
| `iterator` | `AnyIterator` (SBO/heap), `Range`/`makeRange`, `LinkedList` |
| `stream` | Контракты `IInputStream/IOutputStream`, Memory/Slice/File/Std-адаптеры, владение |
| `binary` | endian-утилиты, `BinaryReader`/`BinaryWriter` |
| `compression` | `HuffmanCompressor`: roundtrip, блочность, порча данных, настройки |
| `hash` | MD5 (RFC 1321) и CRC32: векторы, потоковость, границы |
| `circlequeue` | SPSC и MPSC очереди (roundtrip, переполнение, многопоточные стрессы) |
| `console` | токенизация, cvar/команды, `execute`, история, автодополнение, `ConsoleOutput` |
| `json` | `JsonValue` (конструкторы всех типов + SFINAE-интегральный шаблон, мутация, deep copy/move, сравнение со смешанной знаковостью, self-assign), парсер (примитивы, числа/границы int64/uint64, overflow→double, токены 128/129+ символов, строки/эскейпы/суррогаты/UTF-8-ошибки, вложенность/граница глубины 512/513, дубликаты ключей, BOM), writer (компактный и pretty, chunk-границы отступа 16/17/20, эскейпы `\b\f\r`/`\u00XX`, UTF-8-валидация на выходе, NaN/Inf → `NumberOutOfRange`), round-trip и идемпотентность, все коды `JsonError` (включая `AllocationFailed` через тест-хук `setDocumentAllocatorForTests`), `getErrorOffset()`, потоки с троттлингом/сбоями, отсутствие утечек (статистика GlobalAllocator). Файлы: `testjson.cpp` (DOM+интеграция), `testjsonParser.cpp`, `testjsonWriter.cpp`, общие хелперы — `jsonTestUtils.h` |
| `skinmesh` | `SkinMesh::loadFromAssimpMesh` (remap весов на предка, отброс+ренормализация, строгий режим, лимит 4 слотов) и `Skelet::adoptOffsetMatricesFrom` (перенос inverse-bind, идемпотентность) — на фикстурных aiScene/aiBone |
| `resource` | `ResourceManager` (на фикстурных ISaveLoadable+IAllocatorAware типах): construct+commit+get roundtrip, dedup одинакового содержимого под разными ключами (общий слот, оба ключа → один объект), разное содержимое — разные слоты, get отсутствующего — пустой ref, идемпотентный construct, unload (ключ снят, внешний ref держит «осиротевший» слот до последнего release), unload несуществующего — false, preload из MemoryStream, preload при битом потоке (слот снят с кеша), несовпадение тега типа → nullptr, unloadAll (очистка, повторное использование RM), отсутствие утечек (счётчики живых объектов) |

Группы `allocator`, `allocatoraware` и `circlequeue` фактически тестируют `blib-system`; `skinmesh` — `blib-graphics` (per-group libs: `blib_test_group_libs_<group>`); остальные (включая `resource` — `blib-core/resource/ResourceManager`) — `blib-core`. При `BUILD_TESTS=ON` blib-core получает PUBLIC-дефайн `BLIB_BUILD_TESTS`, который включает тест-хуки (например, `JsonParser::setDocumentAllocatorForTests` — инъекция сбоящего аллокатора для проверки `AllocationFailed`; покрывается только отказ на первой аллокации — отказ на N-й уводит в `std::bad_alloc` из std-контейнеров). beng переиспользует `blib_test_main` (`src/beng/test/`, группы `registry componentPool scene entity transform time dialogWindow`; `componentPool` покрывает и итераторы: пропуск `isActive`, const-итерация, инвалидация dense после swap-and-pop, range-for); группа `dialogWindow` линкует `beng-editor` (per-group libs — переменная `beng_test_group_libs_<group>` в `src/beng/test/CMakeLists.txt`) и тестирует `DialogWindow` на **headless-ядре ImGui**: живой контекст (`CreateContext`/`NewFrame`/`EndFrame`) без бэкенда/окна/GL, шрифтовый атлас строится вручную (`io.Fonts->GetTexDataAsRGBA32`), ввод подаётся через `io.AddKeyEvent`/`AddMousePosEvent`/`AddMouseButtonEvent`. Проверка состояния попапа — через internals `ImGui::FindWindowByName` + `window->Active` (публичный `IsPopupOpen` читает `g.CurrentWindow` и вне окон неприменим — ID попапа завязан на окно-хост `DialogWindow`).

---

## Интеграция с CMake/CTest

- Каждая группа — **OBJECT-библиотека** `blib_test_<group>_obj` + отдельный exe `blib_test_<group>` + `add_test`. Почему OBJECT: MSVC-линкер не вырезает неиспользуемые объекты из OBJECT-библиотек (в отличие от static), поэтому саморегистрация `BLIB_TEST_CASE` работает без `/WHOLEARCHIVE` (комментарий в `CMakeLists.txt`).
- **`blib_test_all`** — агрегат: все группы в одном процессе (удобно, но падение одной группы валит прогон).
- **`blib_tests`** — кастомный таргет для сборки всех тестов одной командой.
- **`run_tests.ps1`** — ручной прогон: запускает каждый exe в отдельном процессе, поддерживает `-Build` и `-Config`.
- `TEST_SOURCE_DIR` (абсолютный путь к `src/`) задан для групп — используется PDL-тестами для чтения `demo*.pdl`; FileStream-тесты создают временные файлы в `%TEMP%`.

---

## Ограничения

- Нет CLI-фильтрации/листинга: нельзя запустить отдельный кейс (только выбрать exe группы).
- Агрегат не изолирован: падение группы в `blib_test_all` валит весь прогон — для CI использовать `ctest` (каждый exe отдельно).
- Abort-тесты `DebugAllocator` работают только на MSVC (SEH); на других платформах печатают `SKIPPED`.
- **ImGui-ассерты работают только в Debug**: регрессионные тесты `dialogWindow` (пустые label → `IM_ASSERT(id != window->ID)`) в Release проходят тривиально — гонять их нужно в Debug-конфигурации.
- **Assimp-структуры владеют массивами**: `~aiBone`/`~aiMesh` делают `delete[]`, `~aiNode`/`~aiScene` удаляют рекурсивно. Фикстуры `skinmesh` оборачивают их в `AssimpFixtureSlot` — placement new в сырую память и **без вызова деструкторов** (данные живут в векторах, а выделяющий `new[]` запрещён проектом).
- Часть тестов аллокатора зависит от `BLIB_DEBUG_ALLOCATOR_ENABLED`: счётчики GA-аллокаций в копировании адаптируются `#ifdef` (промоция в debug даёт +2 аллокации вместо +1), поведенческие проверки режимо-независимы; `SKIPPED` остаётся только у release-недоступных (debug-обёртка) и abort-тестов (см. выше).
- `BLIB_TEST_KNOWN_FAILURE` реализован, но не используется; `BLIB_TEST_REQUIRE_THROWS/NOTHROW` не используются.
- Справочные примеры (`allocatorExamples.cpp`, `debugExamples.cpp`, `statsExamples.cpp`, `autoDebugExample.cpp`, `singleProducerSingleConsumerQueueTest.txt`) **не собираются** — это просто примеры в дереве.
- `TEST_SOURCE_DIR` требует наличия исходников при прогоне тестов.

---

## TODO

- [ ] Обсудить каталог известных багов (отдельная задача).
- [ ] Добавить фильтрацию тестов (по имени/группе) и/или листинг.
- [ ] Использовать или удалить `BLIB_TEST_KNOWN_FAILURE` / throws-макросы.
- [ ] Актуализировать `AUTO_DEBUG_ALLOCATOR.md` (путь к примеру уже исправляется в этом же рефакторинге).

---

## Связанные доки

- `../BLIB.md` — общая карта blib.
- `../system/SYSTEM.md` — аллокаторы (группа `allocator`).
- `../core/CORE.md` — модуль, покрываемый большинством групп.
- `../system/memory/AUTO_DEBUG_ALLOCATOR.md` — DebugAllocator и abort-тесты.
- `AGENTS.md` — команды сборки/прогона.
