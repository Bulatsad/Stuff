# Автоматический Debug Allocator

## Обзор изменений

Интегрирован **автоматический debug wrapping** для `DefaultAllocator`. Теперь в debug сборках все аллокации автоматически защищены от типичных ошибок работы с памятью, **без изменения кода пользователя**.

---

## Как это работает

### В Debug сборках (_DEBUG определён):

```cpp
// defaultAllocator.h
using DefaultAllocator = DebugAllocator<DefaultAllocatorImpl>;
// ↑ Автоматически оборачивается в DebugAllocator
```

**Включает:**
- ✅ Guard bytes для детекции buffer overflow/underflow
- ✅ Poison memory для детекции use-after-free  
- ✅ Double-free detection
- ✅ Header validation с checksum
- ⚠️ Overhead: +40 байт на каждую аллокацию

### В Release сборках:

```cpp
using DefaultAllocator = DefaultAllocatorImpl;
// ↑ Прямой прокси к GlobalAllocator (без overhead)
```

**Характеристики:**
- ✅ Максимальная производительность
- ✅ Минимальный overhead
- ⚠️ Нет debug проверок

---

## Использование

### Обычный код (прозрачно для режима):

```cpp
#include <blib/system/memory/allocator.h>

void myFunction() {
    blib::memory::Allocator alloc;  // В debug автоматически защищён!
    
    void* ptr = alloc.allocate(256);
    // ... использование ...
    alloc.deallocate(ptr, 256);  // Проверит guard bytes в debug
}
```

### Детекция ошибок (только в debug):

```cpp
// Debug сборка:
char* buffer = static_cast<char*>(alloc.allocate(64));
buffer[64] = 'X';  // Buffer overflow!
alloc.deallocate(buffer, 64);  // ABORT! Detected overflow

// Release сборка:
// Тот же код просто работает быстро (без детекции)
```

---

## Управление поведением

### По умолчанию:
- Debug build (`_DEBUG`) → автоматически включён DebugAllocator
- Release build → выключен

### Принудительное отключение в debug:

```cpp
#define BLIB_DEBUG_ALLOCATOR_DISABLED
#include <blib/system/memory/defaultAllocator.h>
// Теперь даже в debug будет DefaultAllocatorImpl
```

### Принудительное включение в release:

```cpp
#define BLIB_DEBUG_ALLOCATOR_ENABLED
#include <blib/system/memory/defaultAllocator.h>
// Теперь даже в release будет DebugAllocator (медленно!)
```

---

## Технические детали

### Структура классов:

```
DefaultAllocatorImpl (внутренний класс)
  ↓ stateless, прокси к GlobalAllocator
  
DefaultAllocator (публичный typedef)
  ├─ Debug:   DebugAllocator<DefaultAllocatorImpl> (stateful)
  └─ Release: DefaultAllocatorImpl (stateless)
```

### AllocatorTraits:

```cpp
// Release
AllocatorTraits<DefaultAllocator>::isStateless == true

// Debug  
AllocatorTraits<DefaultAllocator>::isStateless == false
// (потому что DebugAllocator хранит underlying allocator)
```

### Размер в памяти:

```
Release: 
  DefaultAllocator = 1 байт (empty class)
  Overhead на аллокацию = 0 байт

Debug:
  DefaultAllocator = sizeof(DebugAllocator<...>) ≈ 8-16 байт
  Overhead на аллокацию = 40 байт (header + guards)
```

---

## Изменённые файлы

1. **src/blib/system/memory/defaultAllocator.h**
   - Переименован класс в `DefaultAllocatorImpl`
   - Добавлен conditional typedef `DefaultAllocator`
   - Добавлены traits для debug режима
   - Документация обновлена

2. **src/blib/system/memory/impl/allocator.cpp**
   - Конструктор `Allocator()` использует traits вместо hardcoded значения
   - Поддержка stateful DefaultAllocator в debug

3. **src/test/autoDebugExample.cpp** (NEW)
   - Примеры автоматического debug wrapping
   - Демонстрация workflow

---

## Примеры детектируемых ошибок (только debug)

### Buffer Overflow:
```cpp
char* buf = (char*)alloc.allocate(64);
buf[64] = 'X';  // Перезапись back guard
alloc.deallocate(buf, 64);  // ABORT: Buffer overflow detected!
```

### Buffer Underflow:
```cpp
char* buf = (char*)alloc.allocate(64);
buf[-1] = 'X';  // Перезапись front guard
alloc.deallocate(buf, 64);  // ABORT: Buffer underflow detected!
```

### Use-After-Free:
```cpp
int* data = (int*)alloc.allocate(sizeof(int));
alloc.deallocate(data, sizeof(int));  // Заполнено 0xDEADC0DE
int x = *data;  // Чтение poison pattern (видно в отладчике)
```

### Double-Free:
```cpp
void* ptr = alloc.allocate(256);
alloc.deallocate(ptr, 256);  // OK
alloc.deallocate(ptr, 256);  // ABORT: Double-free detected!
```

---

## Рекомендации

### ✅ DO:
- Разрабатывайте в debug режиме для автоматической защиты
- Релизьте в release для максимальной производительности
- Используйте один и тот же код для обоих режимов

### ⚠️ DON'T:
- Не отключайте debug allocator в debug без причины
- Не включайте debug allocator в production (overhead)
- Не полагайтесь на debug проверки в release (их нет)

---

## Совместимость

- ✅ Обратная совместимость: существующий код работает без изменений
- ✅ API не изменился: `DefaultAllocator` остаётся публичным именем
- ⚠️ Traits изменился: в debug `isStateless = false` вместо `true`
  - Это корректно и обрабатывается автоматически

---

## Будущие улучшения

1. **Stack traces** для аллокаций (определение места утечки)
2. **Configurable error handling** (exception vs abort vs log)
3. **Статистика детектированных ошибок**
4. **Per-allocation metadata** (timestamps, thread ID, custom tags)

---

## Заключение

Теперь **весь проект автоматически защищён от memory corruption в debug сборках**, без необходимости явно использовать `DebugAllocator`. Пользователи просто пишут код с `Allocator` или `DefaultAllocator`, и получают:

- 🐛 Debug: Полная защита + детальная диагностика ошибок
- 🚀 Release: Максимальная производительность без overhead

**Write once, debug everywhere!** ✨
