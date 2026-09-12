# BLIB — сервисный слой (Bulat Library)

> Слой: `blib`. Общая философия, карта модулей и правила сборки.
> Не дублирует строгие правила проекта (`AGENTS.md`) и roadmap (`ARCHITECTURE.md`) — только ссылается.
> **Обновлять при любых изменениях кода/сборки blib** (см. AGENTS.md, «Документация модулей»).

---

## Назначение и границы

- **blib — сервисы и платформенные примитивы. Ноль игровых концепций.** Если код отвечает на вопрос «как это сделать» — он в blib; «как устроена игра» — в beng; «что это за игра» — в game (см. ARCHITECTURE.md).
- **Кроссплатформенный по задумке.** Windows реализован полностью; Linux/macOS — заглушки или отсутствуют, но обязаны со временем стать реализациями.
- Верхние слои (`beng`, game) зависят от blib напрямую, без прослоек. blib не знает о beng.
- Что НЕ в этом доке: правила кодирования (AGENTS.md), roadmap (ARCHITECTURE.md), детали модулей — см. их доки.

---

## Философия и сквозные конвенции

1. **Слоистая зависимость:** `blib-system` → `blib-core` → `{blib-graphics, blib-sound, blib-network}`. Нижний модуль не знает о верхнем.
2. **Свои типы и макросы** вместо стандартных: целые (`buint8`…`buint64`, `bint8`…`bint64`), SAL-аннотации, branch hints, коды ошибок. Строгие правила (без `new`/`throw`/smart pointers/`printf`) — в AGENTS.md.
3. **Память — только через `GlobalAllocator`** (или аллокаторы поверх него). Bootstrap-исключения самого аллокатора помечены в коде. Детали — `system/SYSTEM.md`.
4. **Логирование — только через `Console`** (`__blib_log_*`), но сам `blib-system` не может зависеть от `blib-core` и пишет в `stderr` напрямую — архитектурное исключение.
5. **Ошибки — enum-кодами** на модуль (`None = 0`), через `__blib_return_error`; фатальные — `__blib_fatal`. См. `ERROR_HANDLING_ARCHITECTURE.md`.
6. **Кроссплатформенность через CMake + `#ifdef`:** платформенные реализации лежат в `impl/win/`, `impl/linux/`; на неподдерживаемой платформе модуль либо не собирается, либо даёт заглушку.

### Базовые заголовки (корень `src/blib/`)

| Файл | Что даёт |
|------|----------|
| `config.h` | `BLIB_DEBUG` (из `_DEBUG` MSVC или `-DBLIB_DEBUG` GCC/Clang), платформенные макросы, per-module API-макросы (`__blib_core_api`, `__blib_graphics_api`, …), `__blib_unlikely/likely`, `__blib_return_error`, `__blib_fatal`, `__blib_max_bones = 100`, `__blib_default_cache_size = 64`, `__blib_unsafe`, `__blib_render_api_opengl` |
| `blibint.h` | `buint8`…`buint64`, `bint8`…`bint64`, `*Max`-константы |
| `utilmacro.h` | SAL-макросы `_In/_Out/_In_opt/_Out_opt` (пустые), `__blib_override` |
| `inline.h` | `__blib_force_inline`, `__blib_noinline`, `__blib_inline`, `__blib_private_func` (= `static`) |
| `align.h` | `__blib_cache_size`, `__blib_align`, `__blib_cache_aligned`, `__blib_thread_safe` (маркер, не используется) |

---

## Карта модулей

| Модуль | Назначение | Зависимости | Док |
|--------|------------|-------------|-----|
| `blib-system` | Нижний уровень: память (GlobalAllocator, Allocator, pool/debug/malloc, SBO), потоки и синхронизация (RWLocker, MutexLocker, SPSC/MPSC очереди) | только базовые заголовки | `system/SYSTEM.md` |
| `blib-core` | Переносимое ядро: math, console, streams, string/folder, алгоритмы (hash, compression, DFT/FFT), PDL, iterator/linkedList, endian | `blib-system` | `core/CORE.md` |
| `blib-graphics` | OpenGL/WGL, Win32-окно, ассеты (mesh/material/texture/skin), камеры, ImGui, консольное окно; **Windows-only** | `blib-core`, `Opengl32`, Assimp, ImGui, stb | `graphics/GRAPHICS.md` |
| `blib-sound` | Запись/воспроизведение звука (WinMM); **Windows-only, недоделан** | `blib-core`, `Winmm` | `sound/SOUND.md` |
| `blib-network` | TCP/UDP сокеты (winsock); **Windows-only, недоделан** | `blib-core`, `Ws2_32` | `network/NETWORK.md` |
| `blib/test` | Фреймворк `blib::test` и группы тестов (CTest) | `blib-core` | `test/TESTING.md` |

Потребители внутри проекта: `beng-core` → `blib-core`; `beng-client`/`beng-editor` → `blib-graphics`; `vochat` → `blib-sound` + `blib-network` + `blib-core`.

---

## Сборка

- **Таргеты создаются хелперами** `blib_add_module(...)` и `blib_configure_module(target export_define)` (`src/blib/CMakeLists.txt`). Второй пробрасывает PUBLIC-дефайны конфигурации (числовые значения платформы/типа библиотеки, читаемые `config.h` в каждом TU), экспортный дефайн модуля (`blib_core_export`, …) и include-корень `src/`.
- **Тип сборки:** `blib_build_type` = `blib_build_static` (по умолчанию) | `blib_build_dynamic`. В shared-сборке Windows API-макросы становятся `dllimport`, а текущий модуль — `dllexport`; в static — пустые.
- **Inline-режим:** `blib_inline_mode` = `__blib_inline_compiler` (по умолчанию) | `__blib_inline_always` | `__blib_inline_never`.
- **Платформа:** определяется CMake (`WIN32` → windows, `CMAKE_SYSTEM_NAME` → linux/macos) и уходит в `config.h` числовыми дефайнами. На не-Windows `BLIB_BUILD_GRAPHICS/SOUND/NETWORK` по умолчанию OFF; принудительное включение — `FATAL_ERROR` («has no non-Windows implementation yet»).
- **MSVC-флаги захардкожены** в CMake (`/std:c++17 /EHsc /Gd /Gy /Oi /Gm- /MP /W3 /ZI /MD`; Debug — `/Od /MDd`). Для GCC/Clang в Debug добавляется `-DBLIB_DEBUG`.
- **Include-пути:** все инклюды вида `<blib/...>`; корень — `src/`. `COMPILE_ASSIMP_COMPATIBLE` — PUBLIC-дефайн `blib-graphics` (включает конверсии Assimp в math-типах).

### Известные грабли сборки/конфигурации

- **Два механизма определения платформы:** `align.h` и `inline.h` проверяют CMake-дефайн `WIN32`, а `config.h` — `__blib_compile_platform_windows`. При нестандартной сборке они могут разойтись.
- **`AGENTS.md` упоминает `__blib_api`** — такого макроса в blib нет: API-макросы по-модульные (`__blib_core_api` и т.д.).
- **Баг в `blibint.h`:** все `bint8Max/bint16Max/bint32Max/bint64Max` равны `UINT8_MAX` (255) — для знаковых типов это неверно; `buint*Max` корректны.
- **Мёртвые/черновые файлы:** `src/blib/doc/interfaces.h` (скетч `Transormable`, нигде не подключён), `src/blib/graphics/Новый текстовый документ.txt` (черновик), пустые `allocator.h`, `linkedList.cpp`, `algorithm/fdft.h`, `algorithm/impl/dft.cpp`, `algorithm/impl/fdft.cpp`.
- `config.h` при отсутствии платформенных дефайнов не падает (`#error` закомментирован) — платформа молча остаётся undefined.

---

## TODO

- [ ] Обсудить и составить каталог известных багов модулей (отдельная задача; в доках пока только архитектурные грабли).
- [ ] Довести `blib-sound` и `blib-network` до рабочего состояния (сейчас недоделаны, см. их доки).
- [ ] Linux/macOS: заглушки/реализации для graphics/sound/network (сейчас только `rwlock` имеет Linux-реализацию).
- [ ] Починить `bint*Max` в `blibint.h`.
- [ ] Убрать мёртвые файлы (или пометить как черновики) — `doc/interfaces.h`, `Новый текстовый документ.txt`, пустые `.cpp`.
- [ ] Унифицировать определение платформы (`WIN32` vs `__blib_compile_platform_windows`).

---

## Связанные доки

- `AGENTS.md` — правила проекта и конвенции.
- `ARCHITECTURE.md` — слои blib → beng → game, roadmap.
- `ERROR_HANDLING_ARCHITECTURE.md` — обработка ошибок.
- `src/blib/system/SYSTEM.md`, `src/blib/core/CORE.md`, `src/blib/graphics/GRAPHICS.md`, `src/blib/sound/SOUND.md`, `src/blib/network/NETWORK.md`, `src/blib/test/TESTING.md`.
- `src/blib/system/memory/AUTO_DEBUG_ALLOCATOR.md` — авто-debug-wrapping аллокаторов.
- `src/beng/BENG.md` — слой beng.
