# beng-server — headless-серверное ядро движка

> Слой: `beng` (таргет `beng-server`, каталог `src/beng/server`). Шпаргалка по серверному ядру и рефлексивной репликации.
> Не дублирует правила проекта (`AGENTS.md`) и roadmap (`ARCHITECTURE.md`) — только ссылается.
> **Обновлять при любом изменении кода beng-server или репликации beng-core** (см. AGENTS.md, «Документация модулей»).
> Сверено: 2026-09-30

---

## Назначение и границы

- **beng-server** — сетевой слой движка: игра-агностичное headless-ядро сервера (`ServerApplication`) и сетевой клиент репликации (`ReplicationClient`) — без рендера/звука/ввода. Игра подключается интерфейсом `IServerGame` (типы, системы, контент, игроки, кодек команд) — патерн хуков `EditorApplication` (см. BENG.md).
- **Рефлексивная репликация** (кодек/схема — в beng-core, `replication*`): состояние мира возится по сети через рефлексию компонентов — игра НЕ пишет сетевой кодек состояния, только помечает поля (`FunctionField::replicated`) и пишет кодек КОМАНД (непрозрачный payload для движка).
- Что НЕ здесь: клиентское зеркало применяется `ReplicationClientState` (beng-core), клиентская оболочка (окно/рендер) — `beng-client` (Фаза 3); формат сцен — `sceneSaveFormat.h` (BENG.md).

## Ключевые файлы (навигация)

| Что нужно | Где |
|-----------|-----|
| Хук-интерфейс игры (`IServerGame`) | `iServerGame.h` |
| Ядро (frame-API, тикрейт, цикл сети) | `serverApplication.h/.cpp` |
| Сеть сервера (слоты, фрейминг, очередь отправки) | `networkServer.h/.cpp` |
| Сетевой клиент репликации (connect/Welcome/снапшоты) | `replicationClient.h/.cpp` |
| Серверная репликация (зеркала, дельты) | `replicationManager.h/.cpp` |
| Персистентность мира (save/load/reset) | `worldManager.h/.cpp` |
| Схема репликации (wire-типы, хеши) | `beng/core/replicationSchema.h/.cpp` |
| Кодек (поля, Welcome, снапшоты) | `beng/core/replicationCodec.h/.cpp` |
| Фреймер TCP-потока | `beng/core/replicationFramer.h/.cpp` |
| Клиентское зеркало (apply) | `beng/core/replicationClientState.h/.cpp` |
| Тесты | `beng/test/src/impl/testReplication.cpp`, `testServer.cpp` (группы `replication`, `server`) |

## Инварианты и поток данных

### Цикл кадра сервера (`ServerApplication::tick`)

```
poll (accept/recv/отправка очередей)
  → события подключения: зеркало клиента сброшено + onClientJoin → Welcome
  → события отключения: onClientLeave
  → команды: onClientCommand(clientId, payload, size)  [payload — кодек игры]
→ аккумулятор реального dt → фиксированные тики scene.update(fixedDelta)
   (TransformSystem −100, затем системы игры; maxTicksPerFrame = 8 — защита
   от спирали догона, остаток отбрасывается)
→ после КАЖДОГО тика — снапшот каждому клиенту:
   ReplicationManager.buildSnapshot → encodeReplicationSnapshot → очередь отправки
```

- **Тикрейт** — `IServerGame::getTickRate()`; фикс. шаг = 1/tickRate. Рендера/ввода нет.
- **Сервер не доверяет клиенту:** команды — ввод (не состояние), применяется только к юниту игрока (решение игры).

### Провод (формат v1, все числа little-endian)

- Пакет: `[type:u8][payloadSize:u16][payload]`; типы `ReplicationPacketType {Command=1, Snapshot=2, Welcome=3}`; `maxReplicationPacketBytes` = 2048.
- **Welcome**: `tickRate:u32, playerEntityId:u64, wireTypeCount:u8, wireType*{nameLen:u8, name, schemaHash:u32}`.
- **Снапшот**: `tickNumber:u32, entityCount:u16, entity*`:
  - `entityId:u64, flags:u8 (bit0=full, bit1=destroy)`;
  - destroy — только entityId; иначе `componentCount:u8, component*{wireTypeId:u8, full:u8, fieldCount:u8, поля}`;
  - full-компонент — значения всех полей схемы подряд; delta — пары `(fieldIndex:u8, value)`;
  - значение поля — `[kind:u8][payload]` (Float 4 / Int 4 / Bool 1 / Vector3 12 / Entity 8).
- **Wire-тип** — компактный id типа на проводе (= порядковый номер реплицируемого типа в реестре сцены); соответствие wire-id ↔ стабильное имя — в Welcome. Локальные ComponentType на клиенте и сервере МОГУТ не совпадать (порядок регистрации разный) — маппинг по именам.
- **Хеш схемы типа** — FNV-1a 32: имя типа + имена/kind'ы реплицируемых полей. Рассинхрон набора полей → `ReplicationClientState::acceptWelcome` = false (сессия не открывается — протокол не эволюционирует молча).

### Серверная репликация (модель «зеркало на сервере»)

- Per-client зеркало = последнее ОТПРАВЛЕННОЕ состояние (сущности + значения полей). После тика `buildSnapshot` диффует сцену с зеркалом: неизвестная сущность → full; изменённые поля → delta; исчезнувшая → destroy; неизменённое не шлётся. Зеркало обновляется тем, что реально отправлено (TCP надёжен — ресинк не нужен).
- **Дроп очереди** (переполнение при WouldBlock на медленном клиенте): очередь сбрасывается + `needsFullSnapshot` → следующий снапшот полный (зеркало пересоздаётся). TCP-упорядоченность сохраняет корректность.
- Зеркала — **в куче через GlobalAllocator** (~1 МБ суммарно: стек не резиновый — см. «Подводные камни»).
- Удаление КОМПОНЕНТА у живой сущности не реплицируется (MVP); сущности — только целиком (destroy).
- Признак существования сущности в зеркале — наличие Transform (инвариант сцены: Transform есть у всех).

### Клиентское зеркало (`ReplicationClientState`, beng-core; сеть — `ReplicationClient`)

- **`ReplicationClient`** (beng-server): неблокирующий connect (пересоздание сокета — close+destroy+create, паттерн `TcpListener::open`), фрейминг, Welcome → `acceptWelcome` (сверка схем), снапшоты → decode → apply к mirror-сцене, команды — непрозрачный payload, детект дисконнекта; декодированный снапшот — в куче (GlobalAllocator).
- Сущности воспроизводят СЕРВЕРНЫЕ EntityID: `Scene::createEntityWithId` (монотонные ID, пропуск невиданных диапазонов; коллизия id → отказ записи с warning).
- Компоненты создаются/обновляются через рефлексию (`IComponentField::setValue`) — без compile-time T; full-патч обязан покрывать ВСЕ поля схемы (fieldCount сверяется), kind поля сверяется.
- `parent` у Transform НЕ реплицируется (родитель может не существовать на клиенте) — иерархии зеркал TODO.

### WorldManager

- `saveWorld`/`loadWorld` — файловый round-trip `Scene::save/load` (формат sceneSaveFormat); load сбрасывает сцену (`Scene::reset` — реестр типов жив) и после успеха зовёт `Scene::verify`.
- `buildWorld` — сброс + `IServerGame::onWorldBuild` (контент мира; вызывается и после неудачного load как фолбэк).

## Подводные камни / известные баги

- **Стек и размеры структур:** ДЕКОДИРОВАННЫЙ снапшот материализуется на стеке вызывающего (apply/тесты) — капы подобраны под ~128 КБ (`maxSnapshotEntities=64`, `maxSnapshotComponentsPerEntity=8`, `maxReplicatedFields=6`); рост мира за капы — снапшот не собирается (warning + пропуск). Зеркала сервера — в куче (GlobalAllocator), массивы патчей `ServerApplication` — члены (экземпляр в тонком exe на стеке — держать в уме).
- **`TcpSocket` без типа:** дефолтный конструктор НЕ создаёт сокет — connect/send на нём = AV. Клиент обязан `TcpSocket(AddressType::IPv4)`; accept заполняет сокет вызывающего (дефолтный — норм).
- **Первый tick:** `beng::Time` не имеет reset — первый `getDeltaTime()` после старта неточен; аккумулятор это поглощает (лишний тик не выполнится, если dt мал).
- **Порт занят** внешним сервером — `initialize` вернёт false (лог); перезапуск безопасен: `TcpListener::open/close` пересоздают слушающий сокет.
- **Хеш схемы — строгий контракт:** любое изменение набора/имён/kind'ов реплицируемых полей ломает совместимость клиент/сервер разных сборок (по дизайну). Менял поля — меняй версию протокола игры.
- **WorldManager::saveWorld** на сцене с несериализуемыми компонентами — `ComponentNotSerializable` (false + лог); репликация таких компонентов по-прежнему работает (рефлексия не зависит от JSON).

## TODO

- [ ] Удаление/добавление КОМПОНЕНТОВ у живой сущности в репликации (сейчас — только целиком сущности; компонент добавился — дельта его отправит, удаление — нет).
- [ ] Иерархии Transform в зеркалах (parent) — реплицировать родителя с маппингом сущностей.
- [ ] Интерполяция/предикшн на клиенте — фаза 3 (beng-client); серверная сторона к ним готова (тик-нумерация снапшотов).
- [ ] Мультиклиент уже есть структурно (слоты/зеркала per-client); интерес-менеджмент (relevancy) — по мере надобности.
- [ ] Миграция gravelands-сервера на `ServerApplication` + клиента на `ReplicationClientState` — фаза 3 (вместе с клиентской оболочкой, чтобы не ломать PIE/сеть посреди фаз).
- [ ] UDP-канал снапшотов (сейчас TCP для всего).
- [ ] Фаза 2 модульных доков: `CLIENT.md` (см. BENG.md TODO).

## Связанные доки

- `src/beng/BENG.md` — ECS-ядро, рефлексия, редактор-каркас, форматы.
- `ARCHITECTURE.md` — слои, требования к beng, roadmap (п.2 — beng-server).
- `src/blib/network/NETWORK.md` — сокеты, WouldBlock-контракт, Nagle.
- `src/blib/test/TESTING.md` — фреймворк тестов.
- `GLOSSARY.md` — термины репликации (wire-тип, зеркало, дельта).
