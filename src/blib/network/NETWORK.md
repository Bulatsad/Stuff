# NETWORK — blib-network

> Слой: `blib`. TCP/UDP сокеты (winsock). **Статус: Windows-only, недоделан — в проде не использовать.**
> Шпаргалка по устройству. **Обновлять при изменениях кода модуля** (см. AGENTS.md, «Документация модулей»).

---

## Назначение и границы

- Тонкая обёртка над winsock: адреса, сокеты, TCP-клиент/листенер, UDP.
- **Реализация только Windows** (`impl/win/`); на не-Windows модуль не собирается (`FATAL_ERROR` в корневом CMake).
- Собственных потоков нет — всё синхронное, блокирующее (select/polling/таймаутов нет).
- Модуль **недоделан**: часть операций не выполняет реальную работу, ошибки Winsock не транслируются. Каталог багов — отдельная задача (см. TODO).

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Адрес | `address.h`, `impl/win/address.cpp` |
| Базовый сокет, статусы, инициализация | `socket.h`, `impl/win/socket.cpp` |
| TCP-клиент | `tcpSocket.h`, `impl/win/tcpSocket.cpp` |
| TCP-сервер | `tcpListener.h`, `impl/win/tcpListener.cpp` |
| UDP | `udpSocket.h`, `impl/win/udpSocket.cpp` |
| Маппинг enum ↔ WinAPI | `impl/win/winNetworkutil.h/.cpp` |
| CMake | `CMakeLists.txt` |

Потребитель: `src/vochat` (линкует `blib-network` PUBLIC).

---

## API

- **`Address`**: `fromIPv4(str, ok)`, `setPort(int)`, `getType()`, `__getHandler()`; статические `AnyIPv4`, `NoneIPv4`, `LocalhostIPv4`, `BroadcastIPv4`. Владеет платформенным `ctx` (`new`).
- **`Socket`** (базовый, некопируемый): `create(AddressType, SocketType, SocketProtocol)`, `setBlocking(bool)`, `bind(Address&)`, `close()`, `destroy()`.
- **`TcpSocket`**: `connect(Address&)`, `send(const void*, int)`, `recv(void*, int&)`.
- **`TcpListener`**: `listen(int backlog = 16)`, `accept(TcpSocket&)`.
- **`UdpSocket`**: `send(Address&, const void*, int)`, `recv(Address&, void*, int&)`.
- **`InitBlibSocket()`** — глобальная функция инициализации winsock (`WSAStartup`); вызывать до создания сокетов.
- **Статусы:** `SocketStatus {OK, Partial, Disconnected, Error}`. Отдельного `NetworkError` нет; коды `WSAGetLastError()` не сохраняются.
- **Enum-типы:** `AddressType` (IPv4/IPv6/…), `SocketType` (`Stram` — опечатка вместо `Stream`, `Dgram`, …), `SocketProtocol` (значения enum не равны `IPPROTO_*`, маппинг по имени).

---

## Поток данных

**TCP-клиент:** `InitBlibSocket()` → `TcpSocket(type)` (создаёт `SOCK_STREAM`/`IPPROTO_TCP`, сразу blocking) → `Address::fromIPv4` + `setPort` → `connect` → `send` (цикл до полной отправки) / `recv`.

**TCP-сервер:** `TcpListener(type)` → `bind` → `listen(backlog)` → `accept(TcpSocket&)` (адрес клиента отбрасывается).

**UDP:** `UdpSocket(type)` (`SOCK_DGRAM`/`IPPROTO_UDP`, blocking) → `bind` → `sendto`/`recvfrom` (адрес отправителя копируется только для IPv4).

**Режим:** все конструкторы по умолчанию blocking; `setBlocking` использует `ioctlsocket(FIONBIO)`. Non-blocking-ошибки (`WSAEWOULDBLOCK`) специально не обрабатываются.

---

## Инварианты

- **Владение:** `Socket::ctx` — `new`/`delete` платформенного хендла; `Address::ctx` — `new`, но **деструктора у `Address` нет** (утечка на каждое создание; копирование поверхностное — копии делят один `ctx`).
- **Thread-safety отсутствует** — сокеты рассчитаны на один поток; синхронизации внутри нет.
- `WSAStartup` вызывается только явным `InitBlibSocket()`; автоматической инициализации при создании сокета нет; `WSACleanup` в проекте не вызывается.
- `Socket` некопируем/неперемещаем; `TcpSocket`/`UdpSocket` владеют `Socket` по значению; `TcpListener::accept` заполняет переданный сокет через `create(void*)`.
- Лимитов/констант нет; `backlog` по умолчанию 16; `setPort(int)` без валидации; `send/recv` принимают `int`.

---

## Подводные камни

- Модуль недоделан: операции для семейств, отличных от IPv4, могут возвращать `OK`, ничего не сделав; ошибки не сохраняются — не использовать в проде.
- `setBlocking` возвращает инвертированную «успешность» (`ioctlsocket` возвращает 0 при успехе).
- Деструктор `Socket` безусловно вызывает `close()` — у сокета по умолчанию `ctx == nullptr`.
- `send` при ошибке может вернуть `Partial` и зациклиться на нулевой отправке; `recv` не записывает фактическое число принятых байт.
- `Address::setPort` не переводит порт в сетевой порядок байт; `fromIPv4` при ошибке не возвращает заранее (вызывающий должен проверять `ok`).
- Размер `Address::ctx` рассчитан на `sockaddr` (IPv4); для IPv6 буфера может не хватить.
- Реальный пример использования — закомментированные `main()` в `vochat/src/main.cpp`; тестов у модуля нет.

---

## TODO

- [ ] Довести модуль до рабочего состояния: реальные bind/connect для всех семейств, трансляция ошибок (enum `NetworkError` + `WSAGetLastError`), порядок байт, деструкторы/владение.
- [ ] Обсудить каталог известных багов модуля (отдельная задача).
- [ ] `WSACleanup` и защита от повторного `InitBlibSocket`.
- [ ] Исправить опечатки публичного API (`Stram`, `szie`).
- [ ] Тесты (сейчас нет).

---

## Связанные доки

- `../BLIB.md` — общая карта и философия blib.
- `../core/CORE.md` — streams/console (зависимости).
- `../system/SYSTEM.md` — аллокаторы, потоки.
- `AGENTS.md` — правила проекта.
