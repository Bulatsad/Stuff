# NETWORK — blib-network

> Слой: `blib`. TCP/UDP сокеты (winsock). **Статус: Windows-only; TCP-часть доведена до рабочего состояния (2026-09-28), UDP — недоделан (в проде не использовать).**
> Шпаргалка по устройству. **Обновлять при изменениях кода модуля** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-09-30

---

## Назначение и границы

- Тонкая обёртка над winsock: адреса, сокеты, TCP-клиент/листенер, UDP.
- **Реализация только Windows** (`impl/win/`); на не-Windows модуль не собирается (`FATAL_ERROR` в корневом CMake).
- Собственных потоков нет — всё синхронное; неблокирующий режим (`setBlocking(false)`) + опрос вызывающим (паттерн PIE: сервер/клиент опрашивают сокеты в своём tick).
- **TCP-часть доведена (2026-09-28):** NetworkError, корректные send/recv, WouldBlock-контракт, Address — value-тип (deep copy, деструктор, htons). UDP остаётся недоделанным (см. TODO).

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Адрес | `address.h`, `impl/win/address.cpp` |
| Базовый сокет, статусы, ошибки, инициализация | `socket.h`, `impl/win/socket.cpp` |
| TCP-клиент | `tcpSocket.h`, `impl/win/tcpSocket.cpp` |
| TCP-сервер | `tcpListener.h`, `impl/win/tcpListener.cpp` |
| UDP | `udpSocket.h`, `impl/win/udpSocket.cpp` |
| Маппинг enum ↔ WinAPI | `impl/win/winNetworkutil.h/.cpp` |
| Тесты | `blib/test/src/impl/testNetwork.cpp` (группа `network`, loopback) |
| CMake | `CMakeLists.txt` |

Потребители: `src/vochat` (закомментированные примеры), `gravelands` (сервер/клиент, PIE).

---

## API

- **`NetworkError`** (`socket.h`): enum-код модуля (`None = 0, Unknown, NotInitialized, CreateFailed, BindFailed, ListenFailed, ConnectFailed, SendFailed, RecvFailed, WouldBlock, Closed`) — детализирующая причина последнего отказа; читается через `getLastError()` у `Socket`/`TcpSocket`/`TcpListener`.
- **`SocketStatus`**: `{OK, Partial, Disconnected, WouldBlock, Error}`. `WouldBlock` — неблокирующая операция не готова: повторить позже (НЕ ошибка). `Partial` — отправлена часть данных (прогресс — через `sentOut`).
- **`Address`**: value-тип (deep copy, move, деструктор); `fromIPv4(str, ok)` (ошибка → пустой адрес + ok=false), `setPort(int)` — с переводом в сетевой порядок байт; статические `AnyIPv4/NoneIPv4/LocalhostIPv4/BroadcastIPv4`.
- **`Socket`**: `create(...)`, `setBlocking(bool)` (возвращает УСПЕХ — семантика исправлена), `setTcpNoDelay(bool)` — TCP_NODELAY (отключает алгоритм Нейгла для real-time трафика; см. «Подводные камни»), `bind`, `close`, `destroy`, `getLastError`; хендл — через `GlobalAllocator` (без new/delete).
- **`TcpSocket`**: `connect(Address&)` — в неблокирующем режиме WouldBlock = «в процессе» (повторять; WSAEISCONN → OK); `send(data, size, sentOut)` — цикл до полной отправки, при WouldBlock прогресс в `sentOut` (вызывающий продолжает с того же места!); `recv(data, size)` — `size` in/out: фактически принятые байты (частичный приём — НЕ ошибка, TCP — поток); `setTcpNoDelay(bool)` — проброс на `Socket`.
- **`TcpListener`**: `listen(backlog = 16)`, `accept(TcpSocket&)` — WouldBlock = «подключений нет» (неблокирующий режим); `open(AddressType)` — (пере)создать слушающий сокет (повторный запуск сервера: `close` закрыл хендл — bind на нём не сработает), `close()` — закрыть (идемпотентно, память хендла освобождает следующий `open`/деструктор).
- **`InitBlibSocket()`** — `bool`, идемпотентна; вызывать до создания сокетов. `WSACleanup` не вызывается (процесс живёт долго).

---

## Поток данных

**TCP-клиент (неблокирующий):** `InitBlibSocket()` → `TcpSocket(IPv4)` → `setBlocking(false)` → `Address::fromIPv4` + `setPort` → цикл `connect` (WouldBlock → повтор) → `send` (проверять WouldBlock + `sentOut`) / `recv` (WouldBlock — данных нет).

**TCP-сервер:** `TcpListener(IPv4)` → `setBlocking(false)` → `bind` → `listen` → цикл `accept` (WouldBlock — нет подключений). Повторный запуск (после `shutdown`'а): `open(IPv4)` → `setBlocking` → `bind` → `listen`.

**Режим:** конструкторы по умолчанию blocking; `setBlocking` — `ioctlsocket(FIONBIO)`.

---

## Инварианты

- **Владение:** `Socket::ctx`/`Address::ctx` — память через `GlobalAllocator`; `Address` — value-тип (глубокая копия, корректный деструктор); `Socket` некопируем/неперемещаем.
- **Контракт send при WouldBlock:** вызывающий обязан продолжать отправку с той же точки (`data + sentOut`) — иначе поток байт повредится.
- **Thread-safety отсутствует** — сокеты рассчитаны на один поток.
- `WSAStartup` — только явным `InitBlibSocket()` (идемпотентна).

---

## Подводные камни

- **Nagle vs real-time:** TCP_NODELAY по умолчанию ВЫКЛЮЧЕН системой — мелкие пакеты накапливаются (Nagle + delayed ACK дают до ~200 мс латентности на одиночном мелком пакете). Для real-time сообщений (команды ввода, снапшоты) обязательно `setTcpNoDelay(true)` на ПОДКЛЮЧЁННОМ/принятом сокете (так делает gravelands: сервер — после accept, клиент — после connect).
- **Переиспользование хендлов SOCKET (исправлено 2026-09-30):** ОС переиспользует значения SOCKET после closesocket. `Socket::close()` раньше оставлял хендл в ctx — повторный `close()` на закрытом сокете закрывал ЧУЖОЙ сокет, которому достался тот же номер (реальный кейс: клиент закрыл «свой» хендл → вновь созданный слушатель local-server с тем же значением падал WSAENOTSOCK 10038 на каждом accept). Теперь `close()` инвалидирует хендл (INVALID_SOCKET) ВСЕГДА — повторный close безвреден. Паттерн пересоздания сокета (`close`+`destroy`+`create`) остаётся валидным.
- **UDP недоделан**: `recv` не возвращает размер/адрес корректно, ошибки не транслируются — не использовать в проде.
- IPv6/прочие семейства: bind/connect реализованы только для IPv4 (прочие — Error).
- `send` в неблокирующем режиме при заполненных буферах — WouldBlock с частичным прогрессом (см. контракт выше).

---

## TODO

- [ ] UDP: довести до уровня TCP (NetworkError, размер/адрес отправителя, тесты).
- [ ] IPv6-поддержка (Address-буфер уже рассчитан; bind/connect — заглушки).
- [ ] Потоковые обёртки/select для многоклиентских серверов (пока — polling).

---

## Связанные доки

- `../BLIB.md` — общая карта и философия blib.
- `../core/CORE.md` — streams/console (зависимости).
- `../system/SYSTEM.md` — аллокаторы, потоки.
- `AGENTS.md` — правила проекта.
- `GRAVELANDS.md` — потребитель (сетевой цикл игры, PIE).
