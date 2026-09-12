# SOUND — blib-sound

> Слой: `blib`. Запись/воспроизведение звука (WinMM). **Статус: Windows-only, недоделан — в проде не использовать.**
> Шпаргалка по устройству. **Обновлять при изменениях кода модуля** (см. AGENTS.md, «Документация модулей»).

---

## Назначение и границы

- Низкоуровневый доступ к звуковым устройствам Windows (WinMM: `waveOut*`/`waveIn*`) плюс тонкие высокоуровневые обёртки.
- **Реализация только Windows** (`impl/win/`); на не-Windows модуль не собирается (`FATAL_ERROR` в корневом CMake).
- FFT в модуле нет — потребитель (`vochat`) берёт `blib::fourierTransform` из `../core/CORE.md` и работает с буферами сам.
- Модуль **недоделан**: ошибки WinMM наружу не транслируются, часть API/сценариев не доведена. Каталог багов — отдельная задача (см. TODO).

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Формат/устройство | `soundFormat.h`, `impl/soundFormat.cpp`, `soundDevice.h`, `impl/soundDevice.cpp` |
| Буфер сэмплов | `soundBuffer.h` (header-only шаблон + typedef) |
| Кадр real-time | `realTimeSoundFrame.h`, `impl/win/realTimeSoundFrame.cpp` |
| Низкоуровневый плеер/рекордер | `realTimeSoundPlayer.h`, `realTimeSoundRecorder.h`, `impl/win/realTimeSoundPlayer.cpp`, `impl/win/realTimeSoundRecorder.cpp` |
| Высокоуровневые обёртки | `soundPlayer.h`, `impl/soundPlayer.cpp`, `soundRecorder.h`, `impl/win/soundRecorder.cpp` |
| Маппинг форматов WinMM | `impl/win/winSoundUtil.h/.cpp` |
| CMake | `CMakeLists.txt` |

Потребитель: `src/vochat` (линкует `blib-sound` PUBLIC).

---

## API

- **`SoundFormat`**: `channel`, `bitRate`, `sampleRate`; `isEmpty()` — true, если любой параметр 0. **Внимание:** порядок аргументов конструктора `(channel, sampleRate, bitRate)` не совпадает с порядком полей.
- **`SoundDevice`**: `name` + `supportedFormats` (`SoundFormats`); default-ctor удалён.
- **`SoundBufferTemplate<Allocator = CacheAlignedAllocator<uint8_t>>`**: владеет сэмплами (`getData/getSize/getFormat`), есть ctor из внешней памяти `(format, size, payloadSize, pdata)`. Размер вычисляется из длительности (мс) × sampleRate × bitRate/8 × channel.
- **`RealTimeSoundPlayer` / `RealTimeSoundRecorder`** — frame-API: `getDevices/select/setFormat/setBufferInfo/open/close`; обмен буферами через `acquireBuffer()` / `releaseBuffer(frame)`; у рекордера ещё `start/stop/isBufferReady`. Устройство по умолчанию — `WAVE_MAPPER`.
- **`SoundPlayer` / `SoundRecorder`** — высокоуровневые: владеют `std::thread`-ом обработки и накопленными `SoundBuffers`; `SoundPlayer::play/setData/stop`, `SoundRecorder::start/stop/getBuffer`.

---

## Поток данных

**Воспроизведение:** `setFormat` → `setBufferInfo(count, ms)` создаёт `SoundBuffer` + `WAVEHDR` и раскладывает кадры по очередям → `open()` (`waveOutOpen` CALLBACK_FUNCTION + `waveOutPrepareHeader`) → поток `soundOutProc` забирает кадр из очереди пользователя, копирует данные и отдаёт `waveOutWrite` → колбэк `WOM_DONE` возвращает кадр в очередь.

**Запись:** `open()` (`waveInOpen` + `waveInPrepareHeader` + `waveInAddBuffer`) → `waveInStart` → колбэк `WIM_DATA` кладёт готовый кадр в очередь пользователя → поток `soundInProc` копирует его в накопительный вектор и снова ставит буфер в драйвер.

**Очереди:** `LocklessProducerConcumerCircleQueue<RealTimeSoundFrame>` (SPSC, см. `../system/SYSTEM.md`) — пользовательский поток с одной стороны, WinMM-колбэк с другой; ожидания — busy-spin.

---

## Инварианты

- **Владение:** `SoundBuffer` владеет своей памятью (аллокатор по умолчанию — `CacheAlignedAllocator` из `core/alignedAllocator.h`, не GlobalAllocator). `RealTimeSoundFrame` — невладеющая пара (заголовок + буфер).
- **Thread-safety:** `RealTimeSoundPlayer/Recorder` рассчитаны на SPSC-обмен (поток пользователя ↔ колбэк WinMM); `SoundPlayer::playing` — `atomic<bool>`, `SoundRecorder::capturing` — обычный `bool`.
- `open()` обязателен перед `acquire/release`; формат с `isEmpty()` → `open` вернёт `false`.
- Лимитов/констант у формата нет; количество и длительность буферов задаются `setBufferInfo` (высокоуровневый `SoundPlayer` использует фиксированные значения).
- Ошибки WinMM (`MMRESULT`) проверяются, но наружу не передаются: API возвращает `bool`/`void`, отдельного `SoundError` нет.

---

## Подводные камни

- Модуль недоделан: ожидайте незакрытые устройства, гонки и утечки — не использовать в проде.
- `SoundBuffer`, созданный над **чужой памятью** (ctor из указателя), в деструкторе всё равно освобождает её своим аллокатором — нельзя оборачивать внешние буферы без понимания этого.
- `SoundBuffer::setSize` меняет ёмкость (`size`), а не длину записанных данных (`payloadSize`) — в рекордере это уже отмечалось как подозрительное место.
- Высокоуровневые обёртки владеют `std::thread` (создаётся `new`), жизненный цикл/повторные вызовы не защищены.
- `setBufferInfo` при повторном вызове не очищает старые буферы/заголовки.
- `getDevices()` возвращает `const SoundDevices` по значению (блокирует перемещение — историческая мелочь).
- Часть файлов лежит в `impl/win/`, хотя платформо-независима (`realTimeSoundFrame.cpp`, `soundRecorder.cpp`).

---

## TODO

- [ ] Довести модуль до рабочего состояния: трансляция ошибок WinMM (enum `SoundError`), корректный порядок `close/unprepare`, деструкторы, отказ от `new`.
- [ ] Обсудить каталог известных багов модуля (отдельная задача).
- [ ] Убрать фиксированные значения буферов (`10 × 1000 мс`) в именованные константы/параметры.
- [ ] Мёртвый код: `sound.h` + пустые `impl/win/sound.cpp`, `impl/win/soundBuffer.cpp`.
- [ ] Разнести платформо-независимые файлы из `impl/win/`.

---

## Связанные доки

- `../BLIB.md` — общая карта и философия blib.
- `../core/CORE.md` — FFT/DFT, streams, console.
- `../system/SYSTEM.md` — SPSC-очереди (`circleQueue.h`) и аллокаторы.
- `AGENTS.md` — правила проекта.
