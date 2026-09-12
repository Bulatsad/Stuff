# CORE — blib-core

> Слой: `blib`. Переносимое ядро: math, console, streams, алгоритмы, PDL, утилиты.
> Шпаргалка по инвариантам и граблям. **Обновлять при изменениях кода модуля** (см. AGENTS.md, «Документация модулей»).

---

## Назначение и границы

- Переносимое ядро blib: математика, консоль/логирование, потоки ввода-вывода, строки/папки, алгоритмы (hash, compression, DFT/FFT), PDL-парсер, iterator/linkedList.
- Зависит только от `blib-system` (аллокаторы, очереди, синхронизация).
- Не знает о графике/звуке/сети. Графическое консольное окно — в `blib-graphics` (`ConsoleWindow`).
- Не в этом доке: аллокаторы и очереди (`system/SYSTEM.md`), рендер (`graphics/GRAPHICS.md`).

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Math: vector/matrix/quaternion/angle | `src/blib/core/math/*.h` (+ `impl/*.inl`) |
| Консоль и логи | `src/blib/core/console/console.h`, `impl/console.cpp`; макросы `__blib_log_*` в `console.h` |
| Потоки ввода-вывода | `istream.h`, `ostream.h`, `memoryStream.h`, `fileStream.h`, `sliceStream.h`, `stdStreamAdapter.h` |
| Бинарный ввод-вывод, endian | `binaryReader.h`, `binaryWriter.h`, `endian.h` |
| Строки/папки | `string.h`, `folder.h/.cpp` |
| Hash | `algorithm/hash/md5Hasher.h`, `crc32Hasher.h`, `ihasher.h` |
| Сжатие | `algorithm/compression/huffmanCompressor.h`, `icompressor.h` |
| DFT/FFT | `algorithm/dft.h`, `algorithm/dftExp.h` |
| PDL | `pdl/pdl.h`, `pdl/standard.txt`, `pdl/demo*.pdl` |
| Iterator / LinkedList | `iterator.h`, `linkedList.h` |
| Прочее | `flags.h`, `unsafeslicer.h`, `alignedAllocator.h`, `bytearray.h`, `time.h` (legacy) |

---

## Math (`blib::math`)

- **Vector<T, N>** (2/3/4): union `data[N]` / `x,y,z,w`; операторы, `magnitude`, `normalize`, `cross` (3D), `dot`, `lerp`. Копирование из вектора другого размера разрешено только «вниз» (`static_assert(size <= rhsSize)`).
- **Matrix<T, W, H>**: identity по умолчанию; `initializer_list` (недостающее — identity, лишнее игнорируется); `+ - *` (со `static_assert` на размерности), `Transpose()`, `loadIdentity()`.
- **Quaternion<T>**: конструкторы (w,x,y,z)/(axis)/(angle+axis), `normalize/conjugate/inverse`, свободные `rotate/dot/nlerp/operator*`.
- **Angle**: `AngleDegree<T>` (нормализация `[0,360)`), `AngleRadian<T>` (`[0,2π)`), `toRadian()`, `toDergee()` (**опечатка в публичном API**).
- **Константы**: `PI`, `piDiv180`, `c180DivPi`, `piDiv360`; тригонометрия-обёртки в `trigonometry.h`.

### Грабли math

- **Матрицы хранятся по столбцам** (`data[столбец][строка]`), несмотря на комментарий «stores matrix as array of lines(rows)». Трансляция — `data[0..2][3]`.
- **Два пути умножения матриц**: `Matrix::operator*` и свободный `transformMatrix.h::mul()` реализованы с разным порядком индексов. В коде смешаны оба; при написании нового кода — не смешивать и сверяться с вызывающими.
- `normalize` нулевого вектора делит на ноль (тест — только smoke); `magnitude` аккумулирует во `float` независимо от `Type`.
- Assimp-конверсии (`Vector3f::loadFromAssimp`, `Quaternion::loadFromAssimp`) существуют только при `COMPILE_ASSIMP_COMPATIBLE`.

---

## Console (`blib::console`)

- **`Console` — синглтон** (`instance()`), единственная точка логирования и ввода в проекте.
- Реестры: `registerCommand(name, help, callback)` и `registerVariable(name, default, flags, onChanged)`; дубликаты не перезаписываются (warning). Указатель на переменную стабилен (узел `std::map`).
- `execute(line)`: токенизация (`tokenizeLine`, кавычки без эскейпов), эхо в историю, затем команда → cvar (печать/`set`) → error. Встроенные `help`, `list`.
- История: `historyUp/historyDown`, дедуп подряд идущих, лимит 100. Автодополнение: `complete()` + `commonPrefix()`.
- Вывод: `ConsoleOutput` на **MPSC-очереди** (drop-oldest, capacity 2048), `add` — из любого потока, читает ровно один консюмер; `setStdoutEcho` (Warning/Error → stderr).
- **Макросы:** `__blib_log_info/warning/error` — всегда; `__blib_log_debug` вырезается в release; `log*Format` — printf-стиль (стековый буфер 512 байт, длинные строки обрезаются).
- **Потокобезопасность:** `log()` — из любого потока; регистрация/`execute`/история/дополнение — предполагают один (main/UI) поток.
- `__blib_return_error`/`__blib_fatal` определены в `src/blib/config.h` и требуют подключённого `console.h`.

---

## Streams (`blib::core`)

- **Интерфейсы:** `IStream` (`canSeek/seek/tell/size`), `IInputStream::read` (0 == EOF, частичное чтение допустимо), `IOutputStream::write`.
- **Type-erased `InputStream`/`OutputStream`:** move-only; владеемый источник кладётся в heap через **GlobalAllocator** + placement new; `borrow()` — невладеющая ссылка (время жизни обеспечивает вызывающий). SBO осознанно не используется.
- **Реализации:** `MemoryStream` (поверх `std::vector`, строгий seek), `FileStream` (move-only, `OpenMode`-флаги, `readAll`), `SliceStream` (read-only окно над чужой памятью), `StdInputStreamAdapter`/`StdOutputStreamAdapter` (невладеющие обёртки над `std::istream/ostream`, без seek).
- **`BinaryReader`/`BinaryWriter`:** типизированные `readU8…readU64LE/BE`, `readString/writeString` (нулевой терминатор), `readBytes/writeBytes`; владеют или берут взаймы поток.
- **`endian.h`:** `swapU16/32/64`, `readU*LE/BE`, `writeU*LE/BE` — безопасны для невыровненного доступа.
- Не thread-safe (синхронизация снаружи). `FileStream::size()` — кэш, внешние изменения файла не отслеживаются.

---

## Строки, папки, утилиты

- `string.h`: `StringList`, `split`, `replace`, шаблонный `contains`. **Нюанс:** `split` сдвигает позицию на 1, а не на длину разделителя (для многобуквенных разделителей поведение нестандартное).
- `Folder`: нормализует `\`→`/`; если путь не папка — поднимается на уровень вверх. `getAllEntries`/`isFolder` реализованы только под Win32 (на других платформах — пустой список/false). Возвращает в том числе `.` и `..`.
- `Flags<Enum, Storage>`: побитовые флаги с `isUp/isDown`.
- `UnsafeSlicer<T>` / `ConstUnsafeSlicer<T>`: доступ по `base + index*stride`; используется `Image`.
- `alignedAllocator.h` (лежит в core, но по смыслу — память): `AlignedAllocator`/`CacheAlignedAllocator` на `_aligned_malloc`; **не через GlobalAllocator**, нигде не используется.
- `bytearray.h`: `typedef std::vector<buint8> ByteArray` (без `#pragma once`).
- `time.h`: legacy-класс `Time` на `clock()`, нигде не подключён (в beng свой `beng::Time`).

---

## Алгоритмы

### Hash (`algorithm/hash/`)

- `IHasher`: `algorithmName()`, `digestSize()`, `hash(IInputStream&, IOutputStream&)`; вход читается от текущей позиции до EOF (non-seekable допустим), выход — ровно `digestSize()` байт.
- `Md5Hasher` — RFC 1321, 16 байт, без аллокаций. `Crc32Hasher` — IEEE 802.3, 4 байта LE, есть буферный API `hashInit/update/hashFinal/hashBuffer`.
- Не thread-safe (ленивая CRC-таблица формально не синхронизирована, но детерминирована).

### Compression (`algorithm/compression/`)

- `ICompressor`: `algorithmName()`, `compressBound()`, `validateSettings`, `compress/decompress`; все аллокации — через `settings.memoryPoolForCompression`.
- `HuffmanCompressor` — формат **«BHUF» v2**: блочный, таблица (symbol,freq), paddingBits, CRC-32 на блок, stored-блок при `noCompression`. Требует `canSeek()` у **обоих** потоков при `compress` (патч заголовка через seek); `decompress` seek не требует. Версия 1 отклоняется.

### DFT/FFT (`algorithm/dft.h`, `dftExp.h`)

- `dft.h`: наивная O(n²) `fourierTransform`/`inverseFourierTransform` + `Spectre<T>` (на `std::vector`, работа через аллокатор закомментирована).
- `dftExp.h`: рекурсивные `expFastFourierTransform` (требуют длину-степень двойки) + `RotateCoef`.
- Потребитель — `vochat`. Заготовки `fdft.h`/`impl/fdft.cpp` пусты.

### PDL (`pdl/`)

- `pdl::Parser` (pimpl): `load(path)`, `parse/parseNext`, `getVar<T>` (string/int/float/bool/vector), `isCaptured`, `getLastError`.
- Грамматика грузится и валидируется (левая рекурсия отклоняется); матчинг **жадный, без откатов**; `parse` требует, чтобы корневое правило съело весь ввод; глубина матчинга ≤ 1024; привязки сбрасываются в начале каждого `parse`.
- Спецификация языка — `pdl/standard.txt`, журнал патчей — `pdl/history.txt`, примеры — `demo*.pdl` (используются в тестах через `TEST_SOURCE_DIR`).

---

## Iterator / LinkedList

- `AnyIterator<T>` — type erasure с SBO **48 байт** (heap — через GlobalAllocator); сравнение через `typeid` (разнотипные не равны). Операции над пустым итератором — UB.
- `Range<Iter>` / `makeRange(Container&)` — rvalue-контейнеры не принимаются.
- `LinkedList<T, AllocatorT>` — двусвязный список; узлы через переданный аллокатор (по умолчанию `std::allocator<LinkedListNode<T>>`). `pop(pos)` при `pos >= size` — UB.

---

## Подводные камни

- Матричная конвенция и два `operator*`/`mul` — главный источник ошибок в math (см. выше).
- `Console::logFormat`-строки обрезаются на 512 байтах; передача `std::string` в varargs — UB (см. предупреждение в `console.h`).
- `InputStream` move-only: для lvalue-источника нужен copy-ctor, для rvalue — move (проверка `static_assert`); владеемый поток всегда heap через GlobalAllocator.
- `MemoryStream` — исключение из правила аллокаций: данные в `std::vector` (стандартный аллокатор), это задокументировано в его шапке.
- `HuffmanCompressor::compress` без seekable-потоков не работает (возвращает ошибку).
- `dftExp.h` содержит свободные функции, определённые прямо в заголовке без `inline` — при включении в несколько TU возможны multiple definition; `RotateCoef` аллоцирует через `std::allocator`.
- PDL использует `new/delete` и `std::ifstream` внутри реализации (отступление от правил проекта — историческое).
- `Folder`/`split`/`toDergee` — см. нюансы выше.

---

## TODO

- [ ] Обсудить каталог известных багов модуля (отдельная задача).
- [ ] Довести FFT: пустые `fdft.*`, ODR-риск в `dftExp.h`.
- [ ] Привести PDL к правилам проекта (GlobalAllocator, без `new`, касты).
- [ ] Починить/удалить мёртвые файлы (`allocator.h`, `linkedList.cpp`, `fdft.*`, `bytearray.h` без `#pragma once`).
- [ ] Исправить опечатку `toDergee()` (ломающее изменение API — только с миграцией).
- [ ] Актуализировать комментарии (matrix rows/columns).

---

## Связанные доки

- `../BLIB.md` — общая карта и философия blib.
- `../system/SYSTEM.md` — аллокаторы и очереди (зависимость core).
- `../graphics/GRAPHICS.md` — графическое консольное окно (`ConsoleWindow`) и ассеты.
- `../test/TESTING.md` — группы тестов core.
- `AGENTS.md` — правила логирования/ошибок/памяти.
