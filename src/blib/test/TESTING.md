# TESTING — blib/test

> Слой: `blib`. Фреймворк `blib::test` и группы тестов (CTest).
> Шпаргалка по устройству тестов. **Обновлять при изменениях фреймворка/групп** (см. AGENTS.md, «Документация модулей»).

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

Список групп — в `src/blib/test/CMakeLists.txt` (имя группы = суффикс `src/impl/test<group>.cpp`):

| Группа | Что покрывает |
|--------|---------------|
| `angle` | `AngleDegree`/`AngleRadian`: нормализация, конверсии, операторы |
| `matrix` | `Matrix<T,W,H>`: identity, initializer_list, `Transpose`, `+ - *` |
| `quaternion` | Конструкторы, axis-angle, `normalize/conjugate/inverse/rotate` |
| `vector` | `Vector<T,N>`: доступ, операторы, `dot/cross/magnitude/normalize` |
| `pdl` | PDL-парсер: `demo*.pdl`, `parse/parseNext`, команды/опции, ошибки |
| `allocator` | GlobalAllocator, Default/Malloc/Pool/Debug, type-erased `Allocator`, `StdAllocatorAdapter` |
| `iterator` | `AnyIterator` (SBO/heap), `Range`/`makeRange`, `LinkedList` |
| `stream` | Контракты `IInputStream/IOutputStream`, Memory/Slice/File/Std-адаптеры, владение |
| `binary` | endian-утилиты, `BinaryReader`/`BinaryWriter` |
| `compression` | `HuffmanCompressor`: roundtrip, блочность, порча данных, настройки |
| `hash` | MD5 (RFC 1321) и CRC32: векторы, потоковость, границы |
| `circlequeue` | SPSC и MPSC очереди (roundtrip, переполнение, многопоточные стрессы) |
| `console` | токенизация, cvar/команды, `execute`, история, автодополнение, `ConsoleOutput` |

Группы `allocator` и `circlequeue` фактически тестируют `blib-system`; остальные — `blib-core`. beng переиспользует `blib_test_main` (`src/beng/test/`, группы `registry componentPool scene entity transform time`).

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
- Часть тестов аллокатора зависит от `BLIB_DEBUG_ALLOCATOR_ENABLED` (иначе `SKIPPED`).
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
