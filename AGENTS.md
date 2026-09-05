# AGENTS.md — Project Context & Preferences

## Project Overview

**Stuff** — собственный C++ игровой/мультимедийный движок под Windows (OpenGL 3.3+), в активной разработке.
- **GitHub**: https://github.com/Bulatsad/Stuff.git
- **Автор**: Bulatsad (bulatsad111199@gmail.com)
- **Язык**: C++17, GLSL, CMake
- **Платформа**: Windows (MSVC), Linux/macOS — только заглушки

### Основные компоненты:
| Модуль   | Тип        | Назначение |
|----------|------------|------------|
| `blib`   | library    | Ядро: math, graphics (OpenGL-обёртка), sound (WinMM), network (winsock), thread, algorithm (FFT) |
| `beng`   | executable | 3D-вьювер: загрузка моделей через Assimp, скелетная анимация (.md5mesh), ImGui |
| `vochat` | executable | Voice chat: запись/воспроизведение звука, FFT, UDP/TCP стриминг |
| `client` | executable | Игровой клиент (изометрические тайлы, зачатки) |
| `server` | executable | Игровой сервер (зачатки ECS) |

### Сторонние библиотеки (все в `thirdparty/`, без пакетных менеджеров):
- **OpenGL API** headers (ручная загрузка, без GLEW/GLAD) — `thirdparty/opengl/`
- **Dear ImGui** — `thirdparty/imgui/`
- **Assimp** (prebuilt .lib) — `thirdparty/assimplib/`
- **zlib** (bundled с Assimp)
- **stb** (stb_image, stb_truetype и др.) — `thirdparty/stb/`

## Project Structure

```
M:\Stuff\
├── AGENTS.md              <-- этот файл
├── src/
│   ├── CMakeLists.txt      <-- корневой CMake (project "Stuff", cmake >= 3.29)
│   ├── blib/               <-- ядро (library)
│   ├── beng/               <-- 3D-вьювер (executable)
│   ├── vochat/             <-- voice chat (executable)
│   ├── misc/game/          <-- client + server (executables)
│   ├── shaders/            <-- GLSL шейдеры
│   ├── test/               <-- тестовые файлы
│   └── thirdparty/         <-- сторонние библиотеки
└── obj_spider/             <-- тестовые 3D-ассеты
```

## Build

```powershell
# Конфигурация (из src/)
cmake -B ../build -DCMAKE_BUILD_TYPE=Debug

# Сборка (из src/)
cmake --build ../build --config Debug
```

- CMake >= 3.29
- Компилятор: MSVC (флаги захардкожены в CMakeLists.txt: `/std:c++17 /EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD`)
- `blib` может собираться как static или shared (`blib_build_type`)

## Ключевые паттерны кода

### Именование
- Свои целочисленные типы: `buint8`, `bint16`, `buint32` и т.д. (см. `src/blib/blibint.h`)
- Макросы атрибутов: `_In`, `_Out`, `__blib_override`, `__blib_api`, `__beng_api`
- Классы графики в неймспейсе `beng::graphics` и `blib::graphics`

#### Naming conventions
- **Классы/структуры**: `PascalCase` (например, `Allocator`, `GlobalAllocator`, `RenderWindow`)
- **Методы/функции**: `camelCase` (например, `allocate()`, `getTotalAllocated()`, `readLock()`)
- **Переменные**: `camelCase` (например, `totalSize`, `blockCount`, `rwsyncer`)
- **Константы**: 
  - Макросы: `UPPER_SNAKE_CASE` (например, `#define MAX_SIZE 1024`, `#define __blib_cache_size 64`)
  - Переменные: `camelCase` (например, `const double pi = 3.14;`, `constexpr buint32 maxBones = 100;`)
- **Приватные поля**: как у обычных переменных, без префиксов (например, `impl`, `buffer`, `rwsyncer`)
  - `this->ctx` используется для платформо-зависимых структур (дескрипторы окон, хендлеры звуковой подсистемы)

### OpenGL
- Все расширения загружаются вручную (см. `src/blib/graphics/impl/win/`)
- Нет GLEW/GLAD — самописный загрузчик

### Безопасность
- Никаких секретов/ключей в коде
- Не коммитить пароли, токены, ключи API

---

## Строгие правила проекта

### Include-директивы (СТРОГО)
- **`#include "..."` СТРОГО ЗАПРЕЩЁН во всём коде проекта** — только `#include <...>`
- Пути — относительно корня `src/`: `<blib/...>`, `<beng/...>`, `<imgui/...>`
- Исключение: вендорный код в `thirdparty/` (живёт по своим правилам, см. ниже)

### Аллокации памяти (СТРОГО)
- **`::new` / new-выражения СТРОГО ЗАПРЕЩЕНЫ во всём проекте** — любая выделяющая память форма `new`/`new[]`/`delete`/`delete[]` недопустима
- Вся динамическая память — **только через `blib::memory::GlobalAllocator`** (`GlobalAllocator::instance().allocate()/deallocate()`) или аллокаторы, которые сами работают через него (DefaultAllocator, Allocator и т.д.)
- **Placement new разрешён** — он не выделяет память, только конструирует объект в уже выделенной области (`new (ptr) T(...)`)
- **Единственное исключение (bootstrap)**: внутренняя реализация `GlobalAllocator` — системный `::operator new/delete` внутри `allocate()/deallocate()` и создание внутренних объектов (RWLocker, LeakTracker, StatsCollector), т.к. через самого себя аллоцировать нельзя (nullptr lock, нереентерабельный SRWLOCK, самоссылка трекера). Такие места обязаны быть помечены комментарием `// bootstrap exception`
- Свободные `malloc/free` — только в `MallocAllocator` (обёртка над libc для C-совместимости)

### Логирование и консоль (СТРОГО)
- Весь лог, дебаг-вывод и пользовательский ввод — **только через `blib::console::Console::instance()`** и макросы:
  - `__blib_log_info(...)`, `__blib_log_warning(...)`, `__blib_log_error(...)`, `__blib_log_debug(...)` (printf-формат)
  - `__blib_log_debug` полностью вырезается в release-сборке на этапе компиляции
- `Console::log*()` потокобезопасны — можно логировать из любого потока
- **ЗАПРЕЩЕНЫ** в коде проекта: `std::cout`, `std::cin`, `std::cerr`, `printf`, `fprintf`, `sprintf` и всё printf-семейство
- Исключение: `vsnprintf` внутри реализации самой `Console` (форматирование в стековый буфер)

### thirdparty (СТРОГО)
- Код в `thirdparty/` — вендорный, **НЕ РЕДАКТИРУЕТСЯ**: никаких правок в ImGui, Assimp, stb, OpenGL-хедерах
- Правила проекта (инклуды, аллокации, логирование, касты) на thirdparty **не распространяются**
- Взаимодействие — только через публичные API/хуки сторонних библиотек

### Паттерн синглтона
- Единый стиль (как `blib::memory::GlobalAllocator`, `blib::console::Console`):
  - `static T& instance();`
  - приватный конструктор
  - удалённые copy-конструктор и `operator=`

### Smart pointers (СТРОГО)
- **`std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr` ЗАПРЕЩЕНЫ во всём проекте**
- Управление памятью — только через аллокаторы blib и ручное владение

### Касты (СТРОГО)
- Только C++-касты: `static_cast`, `reinterpret_cast`, `const_cast`
- C-style касты `(T)x` в новом коде запрещены

### SAL-аннотации (СТРОГО)
- `_In` / `_Out` **обязательны** для параметров-указателей и параметров-ссылок во всех новых функциях/методах
- `_In_opt` / `_Out_opt` — для опциональных (nullable) параметров

### Макросы (рекомендация)
- Вместо макросов использовать: `constexpr`, `if constexpr`, templates, inline-функции
- Допустимые исключения (иначе невыразимо):
  - лог-макросы `__blib_log_*` (часть API консоли)
  - экспортные макросы `__blib_api`, `__beng_api` (обёртки `__declspec`)
  - `#pragma once` и платформенные `#ifdef`
  - макросы тест-фреймворка (`BLIB_TEST_CASE` и т.п.)
  - макросы обработки ошибок `__blib_unlikely`, `__blib_return_error` (см. ниже)

### Обработка ошибок (СТРОГО)

**📖 Полная документация:** `ERROR_HANDLING_ARCHITECTURE.md` (детальная архитектура, примеры, план миграции)

#### **Запреты:**
- **C++ исключения СТРОГО ЗАПРЕЩЕНЫ** — никаких `throw`, `try`, `catch` во всём проекте
- **`std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`** — запрещены (см. выше)

#### **Классификация ошибок:**

**1. Compile-time ошибки** (программистские, должны быть невозможны):
```cpp
template<typename T, int W, int H, int W2, int H2>
auto Matrix<T,W,H>::operator*(const Matrix<T,W2,H2>& rhs) const {
    static_assert(W == H2, "Matrix dimensions must match for multiplication");
    // ...
}
```

**2. Fatal ошибки** (unrecoverable, программа должна завершиться):
```cpp
#define __blib_fatal(...) \
    do { \
        __blib_log_fatal(__VA_ARGS__); \
        std::abort(); \
    } while(0)

// Использование
if (!GlobalAllocator::instance().initialize()) {
    __blib_fatal("Failed to initialize GlobalAllocator");
}
```

**3. Runtime recoverable ошибки** (основной случай):

**Структура enum:**
```cpp
// Один enum на модуль (ShaderError, TextureError, NetworkError и т.д.)
enum class ShaderError : buint32 {
    None = 0,              // Успех всегда 0
    InvalidType,
    FileNotFound,
    CompilationFailed,
    LinkFailed
};
```

**Макрос для возврата ошибки:**
```cpp
#define __blib_return_error(code, ...) \
    do { \
        __blib_log_error(__VA_ARGS__); \
        return code; \
    } while(0)
```

**Пример реализации:**
```cpp
ShaderError Shader::compile() {
    // Проверка с branch hint — холодная ветка
    if (__blib_unlikely(type_invalid)) {
        __blib_return_error(ShaderError::InvalidType, "Invalid shader type: %d", (int)this->type);
    }
    
    if (__blib_unlikely(!file_open)) {
        __blib_return_error(ShaderError::FileNotFound, "Cannot open shader file: %s", path.c_str());
    }
    
    // HOT PATH — компиляция шейдера (компактный, без холодных веток)
    // ... compilation code ...
    
    if (__blib_unlikely(!compilation_ok)) {
        __blib_return_error(ShaderError::CompilationFailed, "Compilation failed: %s", err.c_str());
    }
    
    return ShaderError::None;  // Успех
}
```

**Пример использования (caller):**
```cpp
ShaderError err = shader.compile();
if (err != ShaderError::None) {
    __blib_log_warning("Failed to load shader, using default");
    useDefaultShader();  // fallback
    return;
}
// Продолжаем работу с успешно скомпилированным шейдером
```

#### **Вспомогательные макросы (уже в `blib/config.h`):**

```cpp
// Branch prediction hints для оптимизации
__blib_unlikely(x)  // Для проверок ошибок (холодные ветки)
__blib_likely(x)    // Для успешных проверок (горячие ветки)

// Низкоуровневые варианты (для сложных случаев)
__blib_expect_false(x)  // = __blib_unlikely(x)
__blib_expect_true(x)   // = __blib_likely(x)
```

**Поддержка компиляторов:**
- GCC/Clang: используют `__builtin_expect` (работает с C++11+)
- MSVC: fallback без оптимизации (код компилируется, но без hints)

#### **Правила использования:**

✅ **DO:**
- Возвращать enum коды для всех runtime recoverable ошибок
- Использовать `__blib_unlikely` для всех проверок ошибок
- Логировать через `Console` перед возвратом ошибки
- Один enum на модуль (не глобальный для всего проекта)
- `None = 0` всегда — успешное выполнение
- Наследовать enum от `buint32` для zero overhead
- Проверять возвращаемые коды в критических местах

❌ **DON'T:**
- НЕ использовать `throw`/`try`/`catch`
- НЕ создавать глобальный enum для всего проекта
- НЕ игнорировать возвращаемые коды в критических местах
- НЕ использовать `std::cout/printf` для логирования (только `Console`)

---

## User Preferences

### Общение и workflow
- Язык общения: **русский**
- **Спрашивать перед коммитом** — никогда не коммитить без явного разрешения
- **Докладывать перед изменениями** — кратко объяснять что делаю перед правками
- **Не создавать новых файлов** без запроса — предпочитать редактирование существующих

### Стиль кода
- Следовать уже сложившемуся стилю в существующих файлах (именование, отступы, скобки)
- Никакого clang-format или автоформаттера — форматирование вручную по контексту
- **Комментарии обязательны** — щедро комментировать код, особенно:
  - Назначение классов и методов
  - Сложные алгоритмы и неочевидные решения
  - Потокобезопасность и синхронизацию
  - Ограничения и инварианты

### Проверка кода
- Основная проверка: `cmake --build ../build --config Debug` (из `src/`)
- Тестов пока нет — планируются в будущем с помощью ассистента
- Никаких линтеров/typecheck-команд отдельно
