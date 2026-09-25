# SYSTEM — blib-system

> Слой: `blib`, нижний модуль. Память, потоки, синхронизация, базовые заголовки.
> Шпаргалка по инвариантам и граблям. **Обновлять при изменениях кода модуля** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-09-25

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
| Базовый интерфейс type erasure | `src/blib/system/memory/itypeErased.h` |
| Интерфейс владения аллокатором | `src/blib/system/memory/iallocatorAware.h` |
| SBO-хранилище | `src/blib/system/memory/sbo.h` |
| Дефолтный/debug/malloc/pool/aligned | `src/blib/system/memory/defaultAllocator.h`, `allocators/debugAllocator.h`, `allocators/mallocAllocator.h`, `allocators/poolAllocator.h`, `allocators/alignedAllocator.h` |
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
- **Копирование stateful (ref-counting, исправлено 2026-09-24):** `share()` разделяет состояние: при первом копировании аллокатор **переезжает** из inline-хранилища обёртки в heap-контрольный блок `SharedState { std::atomic<buint32> refCount; AllocatorType allocator; }` (промоция) — работает и для move-only аллокаторов, т.к. состояние не копируется. Последующие копии — `refCount++` + маленькая heap-обёртка. Последний владелец разрушает аллокатор и возвращает контрольный блок GlobalAllocator'у (размер известен статически). Счётчик атомарный → копирование/уничтожение `Allocator` потокобезопасны (сам аллокатор — нет).
- **Хранение stateful-обёртки:** `union { AllocatorType allocator; SharedState* sharedState; }` + `bool isShared`. Уникальный владелец держит аллокатор inline (маленькие — в SBO, ноль heap-аллокаций); shared-режим — указатель. Copy/move-конструкторы обёртки удалены (перенос только bytewise через SBO-Allocator или placement new — нет self-указателей, инвариант SBO сохранён).
- **`clone()`/`deepCopy()`:** независимая копия через copy-ctor `AllocatorType` — работает для copy-constructible (в debug `DefaultAllocator`/`MallocAllocator` = `DebugAllocator<...>` копируемы); для move-only (`PoolAllocatorImpl`) — `nullptr` + stderr, клон «мёртвый» (задокументированное ограничение).
- **`share()` не const:** промоция мутирует impl-источник (легально из const-контекста `Allocator`, т.к. impl указывает на не-const объект). Копирование из `const Allocator&` валидно.
- `AllocatorTraits<T>::isStateless` по умолчанию `false`; stateless помечены `DefaultAllocatorImpl`, `MallocAllocatorImpl`; stateful — `PoolAllocatorImpl`, `DebugAllocator`.

### ITypeErased — базовый интерфейс type erasure

- Non-template база, от которой наследники строят свои type-erased API (первый планируемый потребитель — `TypeErasedAllocator`). Header-only, файл `itypeErased.h`.
- Механика: erased-объект живёт в куче через член-`Allocator`; операции конкретного типа — не-захватывающие fn-ptr (`DestructorFn`, `CopyConstructorFn`, `MoveConstructorFn` — "vtable" без виртуальных вызовов на объект). Памятью управляет сам `ITypeErased` (аллокатор и размер у него), поэтому лямбды ничего не захватывают.
- API: protected-конструкторы (пустой; от `Allocator&&`), protected `construct<T>(args...)` (allocate → placement new → установка всех трёх fn-ptr; при OOM — stderr + `false`, объект пуст), protected `destroyErased()`, protected `copyConstructFrom`/`moveConstructFrom` (см. ниже), public `isEmpty()` и виртуальный деструктор.
- **База — копируемый/перемещаемый value-тип**: copy/move ctor'ы и `operator=` реализованы на хелперах. `copyConstructFrom` — свежая аллокация через **свой** аллокатор + copy-ctor `T` (кража чужого `pdata` недопустима: `deallocate` обязан идти через выделивший аллокатор). `moveConstructFrom` — то же + `destroyErased()` источника после переноса (источник пуст). Self-assign guard в обоих `operator=` (иначе `destroyErased()` обнулил бы источник до чтения). При OOM или недоступной операции объект становится пустым (при move источник не тронут). Копия/перемещение пустого — валидная пустая.
- **Аллокатор при copy/move не переносится** (дизайнерское решение): копия использует свой DefaultAllocator (дефект копирования stateful-`Allocator` исправлен ref-counting'ом, но ITypeErased намеренно не разделяет аллокатор источника); move — тоже свой, т.к. источник уничтожается через его собственный аллокатор.
- Наследники с дополнительным состоянием (например, `type_info`) дополняют copy/move собственными ctor'ами/`operator=`: база копирует только erased-содержимое, производные члены — забота наследника.
- fn-ptr copy/move заполняются только если `std::is_copy/move_constructible<T>` (`if constexpr`) — move-only/non-movable типы работают, недоступная операция даёт `nullptr` + `false` из хелпера.
- Инварианты: `pdata != nullptr ⟺ pDestructor != nullptr`; повторный `construct`/`copyConstructFrom`/`moveConstructFrom` уничтожает старое содержимое; конструкторы `T` не бросают; alignment — ответственность аллокатора.
- Наследуемые конструкторы сохраняют protected-доступ базы: `using ITypeErased::ITypeErased` в public-секции наследника доступ не меняет (по стандарту) — нужны явные публичные ctor'ы.

### IAllocatorAware — интерфейс владения аллокатором

- Non-template база для классов, обязанных проводить **все** свои динамические аллокации через собственный аллокатор (контракт документальный). Header-only, файл `iallocatorAware.h`.
- Механика: член-`Allocator` по значению (SBO); дефолт — `DefaultAllocator` → поведение наследника без настройки не отличается от прямого GlobalAllocator. `setAllocator(const Allocator&)` — копирование = share() (общее состояние impl через ref-counting, работает и для stateful после фикса 2026-09-24).
- API: public `setAllocator`/`getAllocator` (const и не-const), виртуальный dtor (конвенция; ничем не владеет — уничтожение через конкретный тип), protected `allocate`/`deallocate` — единственный легальный путь аллокаций наследника.
- Контракт наследника: `setAllocator` — до первой аллокации (`deallocate` обязан идти через выделивший аллокатор); конструкторы не аллоцируют динамическую память (аллокатор настраивается после конструирования) — требования кеша ресурсов blib-core (`RESOURCE_MANAGER.md`).

### Аллокаторы модуля

| Аллокатор | Особенности |
|-----------|-------------|
| `DefaultAllocator` | Прокси к GlobalAllocator; в debug автоматически оборачивается в `DebugAllocator` |
| `DebugAllocator<T>` | Header+guard+poison; **+40 байт** на аллокацию; **не thread-safe**; включается при `BLIB_DEBUG` (см. `AUTO_DEBUG_ALLOCATOR.md`) |
| `MallocAllocator` | `std::malloc/free`, мимо статистики GlobalAllocator; stateless |
| `PoolAllocator` | Чанки + intrusive free list + **инлайн-битмап** (1 бит на блок); **не thread-safe**; `allocate` принимает только точный `blockSize`; в debug `blockSize += 40`; явного alignment нет; **итерация по занятым блокам** O(N) (begin()/end(), const-версии; в debug пробрасывается через `DebugAllocator` с поправкой адресов на debug-offset); release-детекты double-free/чужого ptr — warning в stderr + no-op |
| `AlignedAllocator<Alignment>` | Stateless; гарантированное выравнивание блоков по `Alignment` (степень двойки, ≥ 16); бэкинг — **только GlobalAllocator** (overallocation + Header с исходным ptr/размером); overhead: 16 байт header + до `Alignment - 1`; **сознательно без DebugAllocator** (front-guard сдвигает адрес на 32 байта и ломает выравнивание); alias `CacheLineAlignedAllocator` = `AlignedAllocator<__blib_cache_size>` (потребитель — `blib-sound`, SoundBufferTemplate) |
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
- Stateful-обёртка `AllocatorImplWrapper<A,false>`: активный член union определяется `isShared`; в unique-режиме аллокатор inline (разрушается dtor'ом обёртки), в shared — dtor декрементит счётчик, последний владелец разрушает аллокатор контрольного блока и возвращает блок GlobalAllocator'у. `refCount` не может уйти в ноль, пока есть живой владелец.
- `PoolAllocator::deallocate`: неверный размер — тихий no-op; `allocate` с неверным — `nullptr`. Чужой/невыровненный указатель и double-free — warning в stderr + no-op (в debug ловит `DebugAllocator` abort'ом раньше); диагностика в stderr — blib-system не имеет Console.
- Свободен ли блок, `PoolAllocator` определяет по инлайн-битмапу чанка (1 бит на блок, в начале буфера чанка) — O(1); free list нужен только для O(1) `allocate`. `allocate`/`deallocate` находят чанк по диапазону адресов — O(C), C — число чанков, скан с конца.
- Итератор `PoolAllocator` обходит **только занятые** блоки (свободные пропускаются по free list); пустой/полностью свободный пул даёт `begin() == end()`; порядок — чанки по порядку выделения, внутри чанка по возрастанию адресов; `*it` возвращает тот же адрес, что и `allocate()` (в debug — через offset-проброс в `DebugAllocator`), поэтому каждый элемент можно деаллоцировать.
- Итераторы `PoolAllocator` инвалидируются `allocate()` (возможен `push_back` в `chunks`), деструктором и перемещением пула; инкремент на `end()` — UB.
- `GlobalAllocator::allocate(0)` не определён; `DebugAllocator`/`MallocAllocator` при 0 возвращают `nullptr`.
- Потокобезопасность: `GlobalAllocator`/`DefaultAllocator`/`MallocAllocator`/`AlignedAllocator` — да; `DebugAllocator`/`PoolAllocator`/`SBO`/`StdAllocatorAdapter` — нет.
- `SBO` не вызывает деструктор автоматически — обязателен явный `destroy()`.

---

## Подводные камни

- Первое копирование stateful-`Allocator` (промоция) — **2 heap-аллокации** (контрольный блок + обёртка копии), последующие — 1; `clone()`/`deepCopy()` — 1 (для copyable). Источник при промоции мутируется (union-режим меняется), но остаётся работоспособным; const-копирование легально.
- `share()` stateful при OOM контрольного блока откатывается: память обёртки возвращается, источник остаётся уникальным владельцем (nullptr → копия «мёртвая» только при OOM).
- `clone()` move-only stateful (`PoolAllocatorImpl`) даёт «мёртвый» аллокатор + stderr — ограничение, а не баг (независимая копия move-only состояния невыразима).
- `RWLocker` нереентерабелен: `dumpLeaks`/`dumpStats` читают флаги вне лока (гонка при параллельном toggle).
- SPSC `reset()` теряет старый буфер; дефолтный конструктор SPSC сразу аллоцирует 1 элемент.
- `DebugAllocator` пишет back-guard через `reinterpret_cast<buint64*>` — при `size % 8 != 0` запись невыровнена; реакция на ошибку — `abort` (на MSVC — намеренный access violation для SEH-тестов).
- `GlobalAllocator::currentAllocated -= size` без защиты от underflow при неверном размере.
- Чанки `PoolAllocator` аллоцируются контейнером `chunks` через `StdAllocatorAdapter` → `chunkAllocator` (DefaultAllocator/GlobalAllocator); карты трекеров `GlobalAllocator` всё ещё используют `std::unordered_map` со стандартным аллокатором (не всё идёт через GlobalAllocator).
- Move-ctor `PoolAllocatorImpl` намеренно НЕ использует vector move-ctor для `chunks`: `StdAllocatorAdapter` хранит указатель на `chunkAllocator`, и vector move-ctor скопировал бы адрес члена `other` (висячий указатель). Перемещение — только через move-assign в теле (propagate-трейты не объявлены → аллокатор остаётся свой). **Грабли MSVC:** move-assign вектора с неравными аллокаторами memmove'ит элементы, но НЕ опустошает источник — после него обязателен явный `other.chunks.clear()`, иначе деструктор moved-from пула освободит чанки второй раз (double free → повреждение кучи).
- Итерация `PoolAllocator` — O(N): занятость блока проверяется битом в инлайн-битмапе чанка за O(1). Поиск чанка по указателю в `allocate`/`deallocate` — O(C), скан `chunks` с конца (LIFO-блоки чаще из свежих чанков); C обычно 1–4.
- `deallocate` валидирует указатель: попадание в область битмапа или между блоками внутри чанка — warning + no-op (без проверки испортился бы битмап/соседние блоки).
- `DebugAllocator` не имеет begin/end-итераторов сам по себе: итерация доступна только через underlying (`PoolAllocatorImpl`), а `DebugAllocator::begin()/end()` пробрасывают её и сдвигают `*it` на `sizeof(Header) + GUARD_SIZE` — без этого адреса указывали бы на header, а не на пользовательскую область.
- `ITypeErased`: `typeid`-сравнение в наследниках (как в тестовом `TypeErasedValue`) работает только в пределах одного модуля — при `blib` shared типы из разных DLL не совпадут (та же грабля, что у `AnyIterator`). Наследуемые конструкторы базы сохраняют protected-доступ: `using ITypeErased::ITypeErased` в public-секции наследника доступ не меняет. Копирование move-only `T` даёт пустую копию + stderr (copy-ctor не может вернуть ошибку — вызывающий проверяет пустоту). Copy/move/assign аллоцируют и не имеют сильной гарантии: при OOM объект становится пустым (исключений в проекте нет). Self-assign защищён guard'ом (`this != &other`). `moveConstructFrom` сбрасывает только содержимое базы — производные члены (типа `type_info` в тестовом `CopyableValue`) наследник сбрасывает сам.

---

## TODO

- [ ] Обсудить каталог известных багов модуля (отдельная задача).
- [x] Реализовать `share()/deepCopy()` для stateful-аллокаторов (ref-counting) — сделано 2026-09-24 (lazy-промоция, union-хранение; `deepCopy` для move-only остаётся `nullptr` + stderr).
- [ ] Потокобезопасный `PoolAllocator`, кастомный alignment у `PoolAllocator` (у `AlignedAllocator` alignment есть, 2026-09-25), кэш `getApproximateFreeBlocks`.
- [ ] Конвертировать `PoolAllocatorImpl` с голых `size_t` на blib-типы (затронет публичные сигнатуры и вызовы из `ComponentPool`).
- [ ] Починить утечку в SPSC `reset()`.
- [ ] Актуализировать комментарии: размер `Allocator`, `constructInHeap`, `StdAllocatorAdapter` про исключения.
- [ ] Добавить `initialize/shutdown` для GlobalAllocator (если понадобится контроль порядка).
- [ ] Реализовать `TypeErasedAllocator` на базе `ITypeErased` — первый потребитель инфраструктуры.

---

## Связанные доки

- `../BLIB.md` — общая карта и философия blib.
- `memory/AUTO_DEBUG_ALLOCATOR.md` — авто-debug-wrapping (DebugAllocator), не дублируется здесь.
- `../core/CORE.md` — потребитель аллокаторов и очередей.
- `AGENTS.md` — правила аллокаций и логирования.
