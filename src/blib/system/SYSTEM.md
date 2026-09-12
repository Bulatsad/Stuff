# SYSTEM — blib-system

> Слой: `blib`, нижний модуль. Память, потоки, синхронизация, базовые заголовки.
> Шпаргалка по инвариантам и граблям. **Обновлять при изменениях кода модуля** (см. AGENTS.md, «Документация модулей»).

---

## Назначение и границы

- Самый нижний модуль blib: **не зависит ни от одного другого модуля** — только от базовых заголовков (`blib/align.h`, `blibint.h`, `config.h`, `inline.h`, `utilmacro.h`).
- Содержит: аллокаторы и type erasure для них, RAII-синхронизацию, lock-free/мьютексные очереди.
- **`blib-system` не может использовать `Console`** (core выше по слою) — диагностика идёт напрямую в `stderr`/`fprintf` (архитектурное исключение из правила проекта).
- Потоковой абстракции (`Thread`) здесь нет: потоки в проекте — `std::thread` напрямую (sound, тесты).
- Не в этом доке: авто-debug-обёртка аллокаторов — см. `memory/AUTO_DEBUG_ALLOCATOR.md`.

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Публичный аллокатор | `src/blib/system/memory/globalAllocator.h` + `impl/globalAllocator.cpp` |
| Type-erased аллокатор | `src/blib/system/memory/allocator.h`, `impl/allocator.cpp`, `impl/allocator.inl`, `impl/allocatorImplWrapper.h` |
| SBO-хранилище | `src/blib/system/memory/sbo.h` |
| Дефолтный/debug/malloc/pool | `src/blib/system/memory/defaultAllocator.h`, `allocators/debugAllocator.h`, `allocators/mallocAllocator.h`, `allocators/poolAllocator.h` |
| Адаптер под STL | `src/blib/system/memory/stdAllocatorAdapter.h` |
| Read/write lock | `src/blib/system/thread/rwlock.h` + `impl/win|linux/rwlock.cpp` |
| RAII-мьютекс | `src/blib/system/thread/mutexLocker.h` |
| Очереди | `src/blib/system/thread/circleQueue.h` (SPSC), `multiProducerSingleConsumerCircleQueue.h` (MPSC) |
| CMake | `src/blib/system/CMakeLists.txt` |

---

## Память

### GlobalAllocator — единственный публичный аллокатор

- Синглтон: `GlobalAllocator::instance()` (magic static). **Нет `initialize()`/`shutdown()`** — создаётся лениво, разрушается при выходе из процесса.
- API: `allocate(size_t)`, `deallocate(void*, size_t)`, `getCurrentAllocated()`, `getPeakAllocated()`, `getAllocationCount()`, leak tracking (`setLeakTrackingEnabled`, `dumpLeaks`), расширенная статистика (`setExtendedStatsEnabled`, `getHistogram`, `dumpStats`).
- **Потокобезопасен:** мутации — под write-lock, чтение — под read-lock (`thread::RWLocker`).
- **`deallocate` требует точного размера** аллокации (влияет на статистику); `nullptr` игнорируется. Двойное освобождение/неверный размер — UB.
- Внутренние трекеры (`LeakTracker`, `StatsCollector`) создаются лениво и живут до конца процесса.

### Bootstrap-исключения (системный `::operator new/delete` в обход себя)

Через самого себя аллоцировать нельзя, поэтому в `impl/globalAllocator.cpp` есть 6 мест с системным `new/delete` (все помечены комментарием `bootstrap exception`):
1. создание `RWLocker` в конструкторе (lock ещё nullptr);
2. `allocate()` — это и есть аллокатор (`::operator new(size, std::nothrow)`);
3. `deallocate()` — парный `::operator delete`;
4. ленивое создание `LeakTracker` под write-lock (SRWLOCK нереентерабелен + самоссылка трекера);
5. то же для `StatsCollector`;
6. деструктор — `::operator delete` для трекеров и лока.

### Allocator — type-erased обёртка

- API: `allocate/deallocate/clone`, копирование (shared через `share()`) и перемещение.
- Хранение: SBO **56 байт** + указатель на `IAllocatorImpl`. Комментарий в шапке про «итого 64 байта» не сходится с фактической раскладкой на x64.
- **Известное ограничение (зафиксировано HACK/TODO в коде):** для **stateful** аллокаторов `share()`/`deepCopy()` возвращают `nullptr` — копия/клон такого аллокатора становится «мёртвой» (`allocate` → `nullptr`). Для stateless — работает (heap-копия через GlobalAllocator).
- `AllocatorTraits<T>::isStateless` по умолчанию `false`; stateless помечены `DefaultAllocatorImpl`, `MallocAllocatorImpl`; stateful — `PoolAllocatorImpl`, `DebugAllocator`.

### Аллокаторы модуля

| Аллокатор | Особенности |
|-----------|-------------|
| `DefaultAllocator` | Прокси к GlobalAllocator; в debug автоматически оборачивается в `DebugAllocator` |
| `DebugAllocator<T>` | Header+guard+poison; **+40 байт** на аллокацию; **не thread-safe**; включается при `BLIB_DEBUG` (см. `AUTO_DEBUG_ALLOCATOR.md`) |
| `MallocAllocator` | `std::malloc/free`, мимо статистики GlobalAllocator; stateless |
| `PoolAllocator` | Чанки + intrusive free list; **не thread-safe**; `allocate` принимает только точный `blockSize`; в debug `blockSize += 40`; явного alignment нет |
| `StdAllocatorAdapter<T>` | Мост для STL-контейнеров; **не владеет** `Allocator*` (время жизни обеспечивает вызывающий) |

---

## Потоки и синхронизация

| Примитив | Контракт |
|----------|----------|
| `thread::RWLocker` | Read/write lock (`SRWLOCK` на Windows, `pthread_rwlock_t` на Linux). **Нереентерабелен** (повторный захват = дедлок). Метод разблокировки чтения называется `readUnock` (опечатка в публичном API). Non-copyable/non-movable |
| `MutexLocker<Mutex_t>` | RAII-обёртка с контрактом `lock()/unlock()` у мьютекса |
| `LocklessProducerConcumerCircleQueue<T>` | **SPSC** lock-free (атомики, acquire/release). При переполнении `push` возвращает `false` (элемент не теряется). `reset()` **не освобождает** старый буфер (утечка; в MPSC-версии исправлено) |
| `MultiProducerSingleConsumerCircleQueue<T>` | **MPSC** на `RWLocker`: продюсеры сериализуются write-lock, консюмер — read-lock. **Ровно один консюмер**. При переполнении — drop-oldest, `push` всегда успешен. Используется `ConsoleOutput` |

Общее: эффективная ёмкость очередей = запрошенная (sentinel-слот); ожидания в sound — busy-spin.

---

## Инварианты

- `GlobalAllocator` — единственная точка учёта памяти; служебные контейнеры Scene/Pool в beng тоже идут через `StdAllocatorAdapter`.
- Аллокаторы **возвращают `nullptr` при ошибке** (исключений в проекте нет); комментарий `StdAllocatorAdapter` про `std::bad_alloc` устарел.
- `PoolAllocator::deallocate` с неверным размером — тихий no-op; `allocate` с неверным — `nullptr`.
- `GlobalAllocator::allocate(0)` не определён; `DebugAllocator`/`MallocAllocator` при 0 возвращают `nullptr`.
- Потокобезопасность: `GlobalAllocator`/`DefaultAllocator`/`MallocAllocator` — да; `DebugAllocator`/`PoolAllocator`/`SBO`/`StdAllocatorAdapter` — нет.
- `SBO` не вызывает деструктор автоматически — обязателен явный `destroy()`.

---

## Подводные камни

- Копирование stateful `Allocator` даёт «мёртвый» аллокатор (см. выше) — проверено тестом как известный дефект.
- `RWLocker` нереентерабелен: `dumpLeaks`/`dumpStats` читают флаги вне лока (гонка при параллельном toggle).
- SPSC `reset()` теряет старый буфер; дефолтный конструктор SPSC сразу аллоцирует 1 элемент.
- `DebugAllocator` пишет back-guard через `reinterpret_cast<buint64*>` — при `size % 8 != 0` запись невыровнена; реакция на ошибку — `abort` (на MSVC — намеренный access violation для SEH-тестов).
- `GlobalAllocator::currentAllocated -= size` без защиты от underflow при неверном размере.
- Чанки `PoolAllocator` и карты трекеров используют `std::vector`/`std::unordered_map` со стандартным аллокатором (не всё идёт через GlobalAllocator).

---

## TODO

- [ ] Обсудить каталог известных багов модуля (отдельная задача).
- [ ] Реализовать `share()/deepCopy()` для stateful-аллокаторов (ref-counting) — сейчас HACK/TODO.
- [ ] Потокобезопасный `PoolAllocator`, кастомный alignment, кэш `getApproximateFreeBlocks`.
- [ ] Починить утечку в SPSC `reset()`.
- [ ] Актуализировать комментарии: размер `Allocator`, `constructInHeap`, `StdAllocatorAdapter` про исключения.
- [ ] Добавить `initialize/shutdown` для GlobalAllocator (если понадобится контроль порядка).

---

## Связанные доки

- `../BLIB.md` — общая карта и философия blib.
- `memory/AUTO_DEBUG_ALLOCATOR.md` — авто-debug-wrapping (DebugAllocator), не дублируется здесь.
- `../core/CORE.md` — потребитель аллокаторов и очередей.
- `AGENTS.md` — правила аллокаций и логирования.
