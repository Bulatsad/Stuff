# NETWORK — blib-network

> Слой: `blib`. TCP/UDP сокеты (winsock), адресная иерархия (Mac/IPv4/IPv6/подсети/эндпоинты), эмуляция таблицы маршрутизации (Router). **Статус: Windows-only; TCP-часть доведена до рабочего состояния (2026-09-28), адресные типы переработаны (2026-10-01), Router — IPv4+IPv6, форматы linux/windows (2026-10-01); UDP — недоделан (в проде не использовать).**
> Шпаргалка по устройству. **Обновлять при изменениях кода модуля** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-10-01

---

## Назначение и границы

- Тонкая обёртка над winsock: адреса, сокеты, TCP-клиент/листенер, UDP.
- **Адресная иерархия** (`namespace blib::network::address`): конкретные типы `Mac`/`IPv4`/`IPv6`, подсети `IPv4Subnet`/`IPv6Subnet`, универсальный variant-`Address`, эндпоинты `Tcp`/`Udp` (адрес + порт). Все типы — value-семантики (POD, тривиальное копирование, ноль аллокаций).
- **`Router`** (`blib::network`) — эмуляция таблицы маршрутизации: загрузка маршрутов из текстовых форматов linux (`ip route show`) / windows (`route print`) и выбор маршрута `route(destination)` (следующий хоп + интерфейс + метрика). Сетевых пакетов не отправляет — только решает маршрутизацию (эмуляция логики роутера).
- **Реализация только Windows** (`impl/win/`); на не-Windows модуль не собирается (`FATAL_ERROR` в корневом CMake). Парсеры/форматтеры адресов и Router платформонезависимы (живут в `impl/win/` по исторической раскладке).
- Собственных потоков нет — всё синхронное; неблокирующий режим (`setBlocking(false)`) + опрос вызывающим (паттерн PIE: сервер/клиент опрашивают сокеты в своём tick).
- **TCP-часть доведена (2026-09-28):** NetworkError, корректные send/recv, WouldBlock-контракт. UDP остаётся недоделанным (см. TODO).

---

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Адресные типы (Mac/IPv4/IPv6/Subnet/Address/Tcp/Udp) | `address.h`, `impl/win/address.cpp` |
| Таблица маршрутизации (Router, Route, RouteResult) | `router.h`, `impl/win/router.cpp` |
| Базовый сокет, статусы, ошибки, инициализация | `socket.h`, `impl/win/socket.cpp` |
| TCP-клиент | `tcpSocket.h`, `impl/win/tcpSocket.cpp` |
| TCP-сервер | `tcpListener.h`, `impl/win/tcpListener.cpp` |
| UDP | `udpSocket.h`, `impl/win/udpSocket.cpp` |
| Маппинг enum ↔ WinAPI, конвертеры sockaddr | `impl/win/winNetworkutil.h/.cpp` |
| Тесты | `blib/test/src/impl/testNetwork.cpp` (группа `network`: loopback + адреса + Router) |
| CMake | `CMakeLists.txt` |

Потребители: `beng` (`server`/`client`/`editor` — loopback, PIE), `src/misc/vochat` (закомментированные примеры), `gravelands` (через beng).

---

## API

- **`NetworkError`** (`socket.h`): enum-код модуля (`None = 0, Unknown, NotInitialized, CreateFailed, BindFailed, ListenFailed, ConnectFailed, SendFailed, RecvFailed, WouldBlock, Closed`) — детализирующая причина последнего отказа; читается через `getLastError()` у `Socket`/`TcpSocket`/`TcpListener`.
- **`SocketStatus`**: `{OK, Partial, Disconnected, WouldBlock, Error}`. `WouldBlock` — неблокирующая операция не готова: повторить позже (НЕ ошибка). `Partial` — отправлена часть данных (прогресс — через `sentOut`).
- **Адресные типы** (`namespace blib::network::address`; все — POD value-типы с `operator==/!=`, `bool toString(char*, buint32) const`, `bool fromString(const char*)`, `getType()`):
  - `AddressType` — enum тегов (`Mac, IPv4, IPv6, IPv4Subnet, IPv6Subnet, UNDEFINED`); внутри `Address` — алиас `Address::Type` (вложенным enum быть не может: union-члены Address должны видеть его до своего определения — вложенный тип неполного класса недоступен).
  - `Mac` (6 байт), `IPv4` (4 байта), `IPv6` (16 байт) — байты в сетевом порядке; строки: `"aa:bb:cc:dd:ee:ff"`, `"1.2.3.4"`, `"2001:db8::1"` (RFC 5952: сжимается самый длинный прогон нулевых групп, при равных — первый). `IPv6` дополнительно принимает embedded-IPv4 хвост (`::ffff:192.168.1.1`, `0:0:0:0:0:ffff:1.2.3.4`); печатается dotted ТОЛЬКО IPv4-mapped (RFC 5952 sec. 4), compatible — обычным hex.
  - `IPv4Subnet`/`IPv6Subnet` — `{ network; buint8 prefix; }` (0-32/0-128), `bool isInSubnet(ip) const`, строки `"1.2.3.0/24"` / `"2001:db8::/32"`.
  - `Address` — union+тег вариант; конструкторы из всех типов; `toMac()/toIPv4()/toIPv6()/toIPv4Subnet()/toIPv6Subnet()` — при несовпадении тега **fatal** (контракт: проверять `getType()`); `static fromString(str, ok)` — диспетчер: подсеть по `/`, затем Mac → IPv6 → IPv4 (Mac первым: его форма — подмножество синтаксиса IPv6); статики `AnyIPv4/NoneIPv4/LocalhostIPv4/BroadcastIPv4/AnyIPv6/LocalhostIPv6`.
  - `Tcp`/`Udp` — эндпоинты `{ Address ip; buint16 port; }` + `getIP()/getPort()/setPort()`; строки `"1.2.3.4:8080"` / `"[2001:db8::1]:443"`.
- **`Router`** (`blib::network`; некопируем/неперемещаем — контейнеры держат указатель на member-аллокатор):
  - `RouteTableFormat` — `Linux` (вывод `ip route show`), `Windows` (вывод `route print`, английская локаль), `Json` (TODO).
  - `RouterError` — `None = 0, FileNotFound, EmptyTable, InvalidData, NotSupported`.
  - `loadFromFile(path, format)` / `loadFromString(text, format)` — загрузка таблицы; успех **заменяет** текущие маршруты, ошибка — не трогает (парсинг во временные таблицы). Ноль распознанных маршрутов → `EmptyTable`; суммарно > `routerMaxRoutesTotal` (1024) → `InvalidData`.
  - `route(destination, out)` — перегрузки `IPv4`/`IPv6` + диспетчер по `Address` (прочие теги — false). Longest-prefix-match по таблице СВОЕГО семейства; равный префикс — меньшая метрика; полное равенство — первый добавленный. `RouteResult`: `nextHop` — шлюз или сам адресат (on-link), `iface` (имя + локальный адрес интерфейса), `metric`. Нет маршрута — false, `out` не трогается.
  - `getRouteCount()` (суммарно IPv4+IPv6), `clear()`.
  - `Route` — `{ Address destination (IPv4Subnet|IPv6Subnet); Address gateway (IPv4|IPv6|UNDEFINED=on-link); RouteInterface iface; buint32 metric; }`; `RouteInterface` — `{ char name[interfaceNameMax=64]; Address localAddress (UNDEFINED — неизвестен); }` — всё POD, value-семантика.
- **`Socket`**: `create(address::AddressType, SocketType, SocketProtocol)`, `setBlocking(bool)` (возвращает УСПЕХ — семантика исправлена), `setTcpNoDelay(bool)` — TCP_NODELAY (отключает алгоритм Нейгла для real-time трафика; см. «Подводные камни»), `bind(const address::Address& ip, buint16 port)`, `close`, `destroy`, `getLastError`; хендл — через `GlobalAllocator` (без new/delete).
- **`TcpSocket`**: `connect(const address::Tcp&)` — в неблокирующем режиме WouldBlock = «в процессе» (повторять; WSAEISCONN → OK); `send(data, size, sentOut)` — цикл до полной отправки, при WouldBlock прогресс в `sentOut` (вызывающий продолжает с того же места!); `recv(data, size)` — `size` in/out: фактически принятые байты (частичный приём — НЕ ошибка, TCP — поток); `setTcpNoDelay(bool)` — проброс на `Socket`.
- **`TcpListener`**: `listen(backlog = 16)`, `accept(TcpSocket&)` — WouldBlock = «подключений нет» (неблокирующий режим); `open(address::AddressType)` — (пере)создать слушающий сокет (повторный запуск сервера: `close` закрыл хендл — bind на нём не сработает), `close()` — закрыть (идемпотентно, память хендла освобождает следующий `open`/деструктор).
- **`InitBlibSocket()`** — `bool`, идемпотентна; вызывать до создания сокетов. `WSACleanup` не вызывается (процесс живёт долго).

---

## Поток данных

**TCP-клиент (неблокирующий):** `InitBlibSocket()` → `TcpSocket(address::AddressType::IPv4)` → `setBlocking(false)` → `address::Tcp` (ip + порт, напр. `fromString("127.0.0.1:8080")` или `ip = Address::LocalhostIPv4; port = ...`) → цикл `connect` (WouldBlock → повтор) → `send` (проверять WouldBlock + `sentOut`) / `recv` (WouldBlock — данных нет).

**TCP-сервер:** `TcpListener(address::AddressType::IPv4)` → `setBlocking(false)` → `bind(Tcp)` → `listen` → цикл `accept` (WouldBlock — нет подключений). Повторный запуск (после `shutdown`'а): `open(IPv4)` → `setBlocking` → `bind` → `listen`.

**Адресная граница:** IP-байты хранятся в сетевом порядке, порт — в host-порядке; перевод в/из сетевого порядка байт (htons/ntohs) — только в конвертерах `blibToSockaddr`/`blibFromSockaddr` (`impl/win/winNetworkutil`).

**Загрузка таблицы:** `loadFromFile` (FileStream::readAll → байты) / `loadFromString` (strlen) → `loadFromBuffer(data, size, format)` — парсеры работают по диапазону `[begin, end)` без нуль-терминатора; разобранные строки ложатся во временные векторы, при успехе — swap с текущими (атомарность), при пустоте/лимите — ошибка.

**Маршрутизация:** `route(dest)` → проход таблицы своего семейства: `isInSubnet(dest)` → кандидат с максимальным `prefix`; при равном префиксе — минимальная метрика; `nextHop` = `gateway` или (on-link) сам `dest`.

**Режим:** конструкторы по умолчанию blocking; `setBlocking` — `ioctlsocket(FIONBIO)`.

---

## Инварианты

- **Владение:** `Socket::ctx` — память через `GlobalAllocator`; `Socket` некопируем/неперемещаем. Адресные типы — POD (union+тег): копирование тривиально, деструкторов/аллокаций нет.
- **Variant-инвариант:** активный член union определяется тегом; `to*()` на несовпавшем теге — fatal (не recoverable: вызывающий обязан проверить `getType()`); union-конструктор zero-init'ит через первый член (NSDMI членов удаляют default-ctor union'а в C++17).
- **Сброс при неудаче `fromString`:** объект приводится к default-состоянию (нулевой адрес/UNDEFINED/порт 0) — парсить в неинициализированный объект безопасно.
- **Контракт send при WouldBlock:** вызывающий обязан продолжать отправку с той же точки (`data + sentOut`) — иначе поток байт повредится.
- **Инварианты Router:** `destination` маршрута — всегда подсеть своего семейства, `gateway` — IP своего семейства или UNDEFINED (on-link) — обеспечивает парсер, `route()` доверяет (fatal при нарушении). IPv4-адресат ищется только в IPv4-таблице, IPv6 — только в IPv6-таблице. Успешная загрузка заменяет таблицы; неудачная (в т.ч. `EmptyTable`/`InvalidData`) оставляет прежние.
- **Thread-safety отсутствует** — сокеты и Router рассчитаны на один поток.
- `WSAStartup` — только явным `InitBlibSocket()` (идемпотентна).

---

## Подводные камни

- **Nagle vs real-time:** TCP_NODELAY по умолчанию ВЫКЛЮЧЕН системой — мелкие пакеты накапливаются (Nagle + delayed ACK дают до ~200 мс латентности на одиночном мелком пакете). Для real-time сообщений (команды ввода, снапшоты) обязательно `setTcpNoDelay(true)` на ПОДКЛЮЧЁННОМ/принятом сокете (так делает beng: сервер — после accept, клиент — после connect).
- **Переиспользование хендлов SOCKET (исправлено 2026-09-30):** ОС переиспользует значения SOCKET после closesocket. `Socket::close()` раньше оставлял хендл в ctx — повторный `close()` на закрытом сокете закрывал ЧУЖОЙ сокет, которому достался тот же номер (реальный кейс: клиент закрыл «свой» хендл → вновь созданный слушатель local-server с тем же значением падал WSAENOTSOCK 10038 на каждом accept). Теперь `close()` инвалидирует хендл (INVALID_SOCKET) ВСЕГДА — повторный close безвреден. Паттерн пересоздания сокета (`close`+`destroy`+`create`) остаётся валидным.
- **Голый IPv6 в эндпоинте неоднозначен:** `Tcp::fromString("::1:8080")` — последнее `:` может делить «адрес `::1` + порт 8080» ИЛИ «адрес `::` + порт 1» — голый IPv6 НЕ принимается; пишется только `"[::1]:8080"` (для IPv4 скобки не нужны).
- **`Address::fromString` — порядок распознавания важен:** Mac пробуется раньше IPv6 (`"aa:bb:cc:dd:ee:ff"` — валидный синтаксис и того, и другого); подсеть распознаётся по `/` до всего остального.
- **`to*()` — fatal, а не ошибка:** несовпадение тега рвёт процесс (`__blib_fatal`); проверяй `getType()` — это контракт, а не случайный ввод.
- **Embedded-IPv4 хвост неоднозначен:** `::ffff:192.168.1.1` — dotted-форма последних 4 байт; парсер принимает хвост на месте ЛЮБЫХ последних 2 групп (`::192.168.1.1`, `1:2:3:4:5:6:1.2.3.4`), а печатает dotted только mapped (`::ffff:`), иначе `::1` превратился бы в `::0.0.0.1` (RFC 5952 sec. 4).
- **Windows `route print` — только английская локаль:** ключевые слова секций (`Interface List`, `IPv4/IPv6 Route Table`, `Network Destination`, `On-link`, `Persistent Routes`) парсятся как есть — локализованный вывод не распознается. Linux — формат `ip route show` (не `/proc/net/route`).
- **Разделитель `=====` в route print** НЕ завершает секцию сразу после её заголовка (`IPv4 Route Table\n=====`) — иначе заголовок таблицы теряется; завершает только внутри таблицы (после заголовка) и Interface List.
- **`default` без `via`/`src` пропускается** (неоднозначное семейство); linux-строки с несовпадающим семейством `via`/`src` отбрасываются. Имя интерфейса в IPv4-таблице windows отсутствует (только локальный адрес в колонке Interface), в IPv6 — наоборот (имя по индексу из Interface List, локального адреса нет).
- **Router не ищет маршрут в пустой/чужой таблице:** `route(IPv4)` при пустой IPv4-таблице — false (не ошибка); маршруты семейств не смешиваются.
- **UDP недоделан**: `recv` не возвращает размер/адрес корректно, ошибки не транслируются — не использовать в проде.
- IPv6/прочие семейства: bind/connect реализованы только для IPv4 (прочие — Error).
- `send` в неблокирующем режиме при заполненных буферах — WouldBlock с частичным прогрессом (см. контракт выше).

---

## TODO

- [ ] UDP: довести до уровня TCP (NetworkError, размер/адрес отправителя, тесты).
- [ ] IPv6-поддержка сокетов (bind/connect на sockaddr_in6; адресные типы уже готовы).
- [ ] Потоковые обёртки/select для многоклиентских серверов (пока — polling).
- [ ] Router: custom JSON-формат таблицы маршрутизации (схема не утверждена; сейчас `RouteTableFormat::Json` → `RouterError::NotSupported`).

---

## Связанные доки

- `../BLIB.md` — общая карта и философия blib.
- `../core/CORE.md` — streams/console (зависимости).
- `../system/SYSTEM.md` — аллокаторы, потоки.
- `AGENTS.md` — правила проекта.
- `GLOSSARY.md` — термины (Variant-адрес, Эндпоинт, Подсеть, RFC 5952).
- `GRAVELANDS.md` — потребитель (сетевой цикл игры, PIE).
- `../../beng/server/SERVER.md`, `../../beng/client/CLIENT.md` — потребители (loopback-сервер/клиент beng).
