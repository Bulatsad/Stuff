#include <blib/network/router.h>

#include <blib/core/console/console.h>
#include <blib/core/fileStream.h>

#include <cstring>

namespace blib
{
    namespace network
    {
        namespace
        {
            // Контейнер маршрутов (все векторы таблиц используют
            // StdAllocatorAdapter поверх Router::containerAllocator)
            typedef std::vector<Route, blib::memory::StdAllocatorAdapter<Route>> RouteVector;

            // Максимум записей Interface List (route print)
            constexpr buint32 maxInterfaceEntries = 32;

            // -------------------------------------------------------
            // Общие помощники текстового разбора (работают по [begin,
            // end) — нуль-терминатор не требуется)
            // -------------------------------------------------------

            bool isSpace(char c)
            {
                return c == ' ' || c == '\t' || c == '\r';
            }

            const char* skipSpaces(_In const char* p, _In const char* end)
            {
                while (p < end && isSpace(*p))
                {
                    ++p;
                }
                return p;
            }

            /**
             * Следующий токен строки (пробелы между токенами
             * пропускаются). @return false — токенов больше нет
             */
            bool nextToken(_In_Out const char*& p, _In const char* lineEnd,
                _Out const char*& outBegin, _Out const char*& outEnd)
            {
                p = skipSpaces(p, lineEnd);
                if (p >= lineEnd)
                {
                    return false;
                }
                outBegin = p;
                while (p < lineEnd && !isSpace(*p))
                {
                    ++p;
                }
                outEnd = p;
                return true;
            }

            bool tokenEquals(_In const char* begin, _In const char* end, _In const char* literal)
            {
                const buint32 len = static_cast<buint32>(end - begin);
                return std::strlen(literal) == len && std::memcmp(begin, literal, len) == 0;
            }

            /**
             * Десятичное число, занимающее токен целиком (переполнение
             * buint32 — отказ).
             */
            bool tokenDecimal(_In const char* begin, _In const char* end, _Out buint32& outValue)
            {
                if (begin >= end)
                {
                    return false;
                }
                buint32 value = 0;
                for (const char* p = begin; p < end; ++p)
                {
                    const char c = *p;
                    if (c < '0' || c > '9')
                    {
                        return false;
                    }
                    const buint32 digit = static_cast<buint32>(c - '0');
                    if (value > (buint32Max - digit) / 10)
                    {
                        return false;
                    }
                    value = value * 10 + digit;
                }
                outValue = value;
                return true;
            }

            /**
             * Скопировать токен в нуль-терминированный буфер
             * (для передачи в address::*::fromString).
             */
            bool tokenToBuffer(_In const char* begin, _In const char* end,
                _Out char* buffer, _In buint32 bufferSize)
            {
                const buint32 len = static_cast<buint32>(end - begin);
                if (len == 0 || len >= bufferSize)
                {
                    return false;
                }
                std::memcpy(buffer, begin, len);
                buffer[len] = '\0';
                return true;
            }

            /**
             * Разобрать токен в адрес через диспетчер Address::fromString.
             */
            bool tokenToAddress(_In const char* begin, _In const char* end, _Out address::Address& out)
            {
                char buffer[address::maxAddressStringLength];
                if (!tokenToBuffer(begin, end, buffer, address::maxAddressStringLength))
                {
                    return false;
                }
                bool ok = false;
                const address::Address parsed = address::Address::fromString(buffer, &ok);
                if (!ok)
                {
                    return false;
                }
                out = parsed;
                return true;
            }

            bool isIpAddressType(_In address::AddressType type)
            {
                return type == address::AddressType::IPv4 || type == address::AddressType::IPv6;
            }

            void copyInterfaceName(_Out char* dest, _In const char* begin, _In const char* end)
            {
                buint32 len = static_cast<buint32>(end - begin);
                if (len > interfaceNameMax - 1)
                {
                    len = interfaceNameMax - 1;
                }
                for (buint32 i = 0; i < len; ++i)
                {
                    dest[i] = begin[i];
                }
                dest[len] = '\0';
            }

            // -------------------------------------------------------
            // Построчный обход документа
            // -------------------------------------------------------

            struct DocLine
            {
                const char* begin;
                const char* end;
            };

            struct LineWalker
            {
                const char* pos;
                const char* end;

                bool next(_Out DocLine& out)
                {
                    if (this->pos >= this->end)
                    {
                        return false;
                    }
                    out.begin = this->pos;
                    const char* newline = this->pos;
                    while (newline < this->end && *newline != '\n')
                    {
                        ++newline;
                    }
                    out.end = newline;
                    this->pos = (newline < this->end) ? newline + 1 : newline;
                    return true;
                }
            };

            bool lineContains(_In const DocLine& line, _In const char* needle)
            {
                const buint32 needleLen = static_cast<buint32>(std::strlen(needle));
                if (needleLen == 0 || static_cast<buint32>(line.end - line.begin) < needleLen)
                {
                    return false;
                }
                for (const char* p = line.begin; p + needleLen <= line.end; ++p)
                {
                    if (std::memcmp(p, needle, needleLen) == 0)
                    {
                        return true;
                    }
                }
                return false;
            }

            // -------------------------------------------------------
            // Linux: вывод "ip route show"
            //
            //   default via 192.168.1.1 dev eth0 proto dhcp metric 100
            //   192.168.1.0/24 dev eth0 proto kernel scope link src 192.168.1.5
            //   10.0.0.0/8 via 10.0.0.1 dev eth1
            //   2001:db8::/64 dev eth0 metric 256
            //   default via fe80::1 dev eth0 proto ra metric 1024
            //
            // Распознаются токены: via/dev/metric/src/onlink; прочие
            // (proto/scope/expires/mtu/...) игнорируются. Нераспознанные
            // строки пропускаются с warning (реальные таблицы содержат
            // служебные маршруты). Не-маршрутные типы (blackhole,
            // unreachable, broadcast, local, multicast, ...) пропускаются.
            // -------------------------------------------------------

            bool isNonForwardRouteType(_In const char* begin, _In const char* end)
            {
                static const char* nonForwardTypes[] = {
                    "blackhole", "unreachable", "prohibit", "throw",
                    "broadcast", "local", "multicast", "nat", "anycast"
                };
                for (buint32 i = 0; i < sizeof(nonForwardTypes) / sizeof(nonForwardTypes[0]); ++i)
                {
                    if (tokenEquals(begin, end, nonForwardTypes[i]))
                    {
                        return true;
                    }
                }
                return false;
            }

            void parseLinuxRoutes(_In const char* data, _In buint32 size,
                _Out RouteVector& outV4, _Out RouteVector& outV6)
            {
                LineWalker walker = { data, data + size };
                DocLine line;
                buint32 lineNumber = 0;
                while (walker.next(line))
                {
                    ++lineNumber;
                    const char* p = skipSpaces(line.begin, line.end);
                    if (p >= line.end)
                    {
                        continue; // пустая строка
                    }

                    const char* firstBegin = nullptr;
                    const char* firstEnd = nullptr;
                    if (!nextToken(p, line.end, firstBegin, firstEnd))
                    {
                        continue;
                    }

                    Route route{};

                    // Первый токен: "default" или CIDR "ip/prefix"
                    const bool isDefault = tokenEquals(firstBegin, firstEnd, "default");
                    bool haveSubnet = false;
                    if (!isDefault)
                    {
                        if (isNonForwardRouteType(firstBegin, firstEnd))
                        {
                            continue; // служебный маршрут — не пересылка
                        }
                        address::Address subnet;
                        if (!tokenToAddress(firstBegin, firstEnd, subnet))
                        {
                            __blib_log_warning("Router: linux: line %u: not a route (skipped)", lineNumber);
                            continue;
                        }
                        if (subnet.getType() != address::AddressType::IPv4Subnet &&
                            subnet.getType() != address::AddressType::IPv6Subnet)
                        {
                            __blib_log_warning("Router: linux: line %u: destination is not a subnet (skipped)", lineNumber);
                            continue;
                        }
                        route.destination = subnet;
                        haveSubnet = true;
                    }

                    // Опции строки
                    address::Address via;
                    address::Address src;
                    bool haveVia = false;
                    bool haveSrc = false;
                    bool onlink = false;
                    buint32 metric = 0;

                    const char* tokBegin = nullptr;
                    const char* tokEnd = nullptr;
                    while (nextToken(p, line.end, tokBegin, tokEnd))
                    {
                        if (tokenEquals(tokBegin, tokEnd, "via"))
                        {
                            const char* vBegin = nullptr;
                            const char* vEnd = nullptr;
                            address::Address gateway;
                            if (nextToken(p, line.end, vBegin, vEnd) &&
                                tokenToAddress(vBegin, vEnd, gateway) &&
                                isIpAddressType(gateway.getType()))
                            {
                                via = gateway;
                                haveVia = true;
                            }
                        }
                        else if (tokenEquals(tokBegin, tokEnd, "dev"))
                        {
                            const char* nBegin = nullptr;
                            const char* nEnd = nullptr;
                            if (nextToken(p, line.end, nBegin, nEnd))
                            {
                                copyInterfaceName(route.iface.name, nBegin, nEnd);
                            }
                        }
                        else if (tokenEquals(tokBegin, tokEnd, "metric"))
                        {
                            const char* mBegin = nullptr;
                            const char* mEnd = nullptr;
                            buint32 metricValue = 0;
                            if (nextToken(p, line.end, mBegin, mEnd) &&
                                tokenDecimal(mBegin, mEnd, metricValue))
                            {
                                metric = metricValue;
                            }
                        }
                        else if (tokenEquals(tokBegin, tokEnd, "src"))
                        {
                            const char* sBegin = nullptr;
                            const char* sEnd = nullptr;
                            address::Address local;
                            if (nextToken(p, line.end, sBegin, sEnd) &&
                                tokenToAddress(sBegin, sEnd, local) &&
                                isIpAddressType(local.getType()))
                            {
                                src = local;
                                haveSrc = true;
                            }
                        }
                        else if (tokenEquals(tokBegin, tokEnd, "onlink"))
                        {
                            onlink = true;
                        }
                        // Прочие токены (proto/scope/expires/mtu/...) —
                        // игнорируются: на маршрут не влияют
                    }

                    // "default" — семейство определяем по шлюзу (via),
                    // иначе по локальному адресу (src); без них маршрут
                    // неоднозначен и пропускается
                    if (!haveSubnet)
                    {
                        address::AddressType family = address::AddressType::UNDEFINED;
                        if (haveVia)
                        {
                            family = via.getType();
                        }
                        else if (haveSrc)
                        {
                            family = src.getType();
                        }
                        if (family != address::AddressType::IPv4 && family != address::AddressType::IPv6)
                        {
                            __blib_log_warning("Router: linux: line %u: default route without via/src (skipped)", lineNumber);
                            continue;
                        }
                        if (family == address::AddressType::IPv4)
                        {
                            route.destination = address::Address(address::IPv4Subnet{ address::IPv4{}, 0 });
                        }
                        else
                        {
                            route.destination = address::Address(address::IPv6Subnet{ address::IPv6{}, 0 });
                        }
                    }

                    const bool isV4Route = route.destination.getType() == address::AddressType::IPv4Subnet;

                    // Шлюз обязан совпадать по семейству с подсетью
                    if (haveVia)
                    {
                        const bool viaMatches = isV4Route
                            ? via.getType() == address::AddressType::IPv4
                            : via.getType() == address::AddressType::IPv6;
                        if (!viaMatches)
                        {
                            __blib_log_warning("Router: linux: line %u: gateway family mismatch (skipped)", lineNumber);
                            continue;
                        }
                        route.gateway = via;
                    }
                    if (onlink)
                    {
                        // Сеть подключена напрямую — шлюза нет
                        route.gateway = address::Address();
                    }

                    // Локальный адрес интерфейса (src) — только своего
                    // семейства
                    if (haveSrc)
                    {
                        const bool srcMatches = isV4Route
                            ? src.getType() == address::AddressType::IPv4
                            : src.getType() == address::AddressType::IPv6;
                        if (srcMatches)
                        {
                            route.iface.localAddress = src;
                        }
                    }

                    route.metric = metric;

                    if (isV4Route)
                    {
                        outV4.push_back(route);
                    }
                    else
                    {
                        outV6.push_back(route);
                    }
                }
            }

            // -------------------------------------------------------
            // Windows: вывод "route print"
            //
            // Interface List (id ... имя), затем секции
            // "IPv4 Route Table" (destination netmask gateway interface
            // metric; gateway "On-link" — сеть подключена напрямую) и
            // "IPv6 Route Table" (if metric destination gateway).
            //
            // Ожидается вывод английской локали (ключевые слова секций
            // не локализованы).
            // -------------------------------------------------------

            struct InterfaceEntry
            {
                buint32 id = 0;
                char name[interfaceNameMax] = {};
            };

            /**
             * Префикс по IPv4-маске (число ведущих единиц; 255.0.0.0 → 8).
             * @return false — маска не префиксная (нули между единицами)
             */
            bool ipv4MaskToPrefix(_In const address::IPv4& mask, _Out buint8& outPrefix)
            {
                buint32 prefix = 0;
                bool seenZero = false;
                for (buint32 i = 0; i < address::ipv4BytesCount; ++i)
                {
                    const buint8 byteValue = mask.bytes[i];
                    for (buint32 bit = 0; bit < 8; ++bit)
                    {
                        const bool isOne = (byteValue & (0x80 >> bit)) != 0;
                        if (isOne)
                        {
                            if (seenZero)
                            {
                                return false;
                            }
                            ++prefix;
                        }
                        else
                        {
                            seenZero = true;
                        }
                    }
                }
                outPrefix = static_cast<buint8>(prefix);
                return true;
            }

            void parseInterfaceLine(_In const DocLine& line, _Out InterfaceEntry* entries,
                _In buint32 maxEntries, _In_Out buint32& count, _In buint32 lineNumber)
            {
                const char* p = skipSpaces(line.begin, line.end);

                // Индекс интерфейса: число до "..."
                buint32 id = 0;
                bool haveDigit = false;
                while (p < line.end && *p >= '0' && *p <= '9')
                {
                    id = id * 10 + static_cast<buint32>(*p - '0');
                    haveDigit = true;
                    ++p;
                }
                if (!haveDigit || p + 3 > line.end || p[0] != '.' || p[1] != '.' || p[2] != '.')
                {
                    __blib_log_warning("Router: windows: line %u: not an interface entry (skipped)", lineNumber);
                    return;
                }
                p += 3;

                // Имя — после последовательности "......"
                const char* dots = nullptr;
                for (const char* q = p; q + 6 <= line.end; ++q)
                {
                    if (q[0] == '.' && q[1] == '.' && q[2] == '.' &&
                        q[3] == '.' && q[4] == '.' && q[5] == '.')
                    {
                        dots = q;
                        break;
                    }
                }
                if (dots == nullptr)
                {
                    __blib_log_warning("Router: windows: line %u: interface name not found (skipped)", lineNumber);
                    return;
                }
                const char* nameBegin = dots + 6;
                const char* nameEnd = line.end;
                while (nameEnd > nameBegin && isSpace(nameEnd[-1]))
                {
                    --nameEnd;
                }
                if (count >= maxEntries)
                {
                    __blib_log_warning("Router: windows: too many interfaces (skipped)");
                    return;
                }
                entries[count].id = id;
                copyInterfaceName(entries[count].name, nameBegin, nameEnd);
                ++count;
            }

            const InterfaceEntry* findInterface(_In const InterfaceEntry* entries,
                _In buint32 count, _In buint32 id)
            {
                for (buint32 i = 0; i < count; ++i)
                {
                    if (entries[i].id == id)
                    {
                        return &entries[i];
                    }
                }
                return nullptr;
            }

            void parseIPv4RouteLine(_In const DocLine& line, _Out RouteVector& outV4, _In buint32 lineNumber)
            {
                // destination, netmask, gateway, interface, metric
                const char* tokenBegin[5] = {};
                const char* tokenEnd[5] = {};
                const char* p = skipSpaces(line.begin, line.end);
                for (buint32 i = 0; i < 5; ++i)
                {
                    if (!nextToken(p, line.end, tokenBegin[i], tokenEnd[i]))
                    {
                        __blib_log_warning("Router: windows: line %u: not an IPv4 route row (skipped)", lineNumber);
                        return;
                    }
                }

                char buffer[address::maxAddressStringLength];

                address::IPv4 destination;
                if (!tokenToBuffer(tokenBegin[0], tokenEnd[0], buffer, address::maxAddressStringLength) ||
                    !destination.fromString(buffer))
                {
                    __blib_log_warning("Router: windows: line %u: bad IPv4 destination (skipped)", lineNumber);
                    return;
                }
                address::IPv4 mask;
                if (!tokenToBuffer(tokenBegin[1], tokenEnd[1], buffer, address::maxAddressStringLength) ||
                    !mask.fromString(buffer))
                {
                    __blib_log_warning("Router: windows: line %u: bad IPv4 netmask (skipped)", lineNumber);
                    return;
                }
                buint8 prefix = 0;
                if (!ipv4MaskToPrefix(mask, prefix))
                {
                    __blib_log_warning("Router: windows: line %u: non-contiguous netmask (skipped)", lineNumber);
                    return;
                }

                Route route{};
                route.destination = address::Address(address::IPv4Subnet{ destination, prefix });

                if (tokenEquals(tokenBegin[2], tokenEnd[2], "On-link"))
                {
                    // Сеть подключена напрямую — шлюза нет
                }
                else
                {
                    address::IPv4 gateway;
                    if (!tokenToBuffer(tokenBegin[2], tokenEnd[2], buffer, address::maxAddressStringLength) ||
                        !gateway.fromString(buffer))
                    {
                        __blib_log_warning("Router: windows: line %u: bad IPv4 gateway (skipped)", lineNumber);
                        return;
                    }
                    route.gateway = address::Address(gateway);
                }

                // Колонка Interface — локальный адрес интерфейса
                address::IPv4 interfaceAddress;
                if (!tokenToBuffer(tokenBegin[3], tokenEnd[3], buffer, address::maxAddressStringLength) ||
                    !interfaceAddress.fromString(buffer))
                {
                    __blib_log_warning("Router: windows: line %u: bad interface address (skipped)", lineNumber);
                    return;
                }
                route.iface.localAddress = address::Address(interfaceAddress);

                if (!tokenDecimal(tokenBegin[4], tokenEnd[4], route.metric))
                {
                    __blib_log_warning("Router: windows: line %u: bad metric (skipped)", lineNumber);
                    return;
                }

                outV4.push_back(route);
            }

            void parseIPv6RouteLine(_In const DocLine& line, _In const InterfaceEntry* entries,
                _In buint32 entryCount, _Out RouteVector& outV6, _In buint32 lineNumber)
            {
                // if, metric, destination/prefix, gateway
                const char* tokenBegin[4] = {};
                const char* tokenEnd[4] = {};
                const char* p = skipSpaces(line.begin, line.end);
                for (buint32 i = 0; i < 4; ++i)
                {
                    if (!nextToken(p, line.end, tokenBegin[i], tokenEnd[i]))
                    {
                        __blib_log_warning("Router: windows: line %u: not an IPv6 route row (skipped)", lineNumber);
                        return;
                    }
                }

                buint32 interfaceId = 0;
                if (!tokenDecimal(tokenBegin[0], tokenEnd[0], interfaceId))
                {
                    __blib_log_warning("Router: windows: line %u: bad interface index (skipped)", lineNumber);
                    return;
                }

                Route route{};

                if (!tokenDecimal(tokenBegin[1], tokenEnd[1], route.metric))
                {
                    __blib_log_warning("Router: windows: line %u: bad metric (skipped)", lineNumber);
                    return;
                }

                address::Address destination;
                if (!tokenToAddress(tokenBegin[2], tokenEnd[2], destination) ||
                    destination.getType() != address::AddressType::IPv6Subnet)
                {
                    __blib_log_warning("Router: windows: line %u: bad IPv6 destination (skipped)", lineNumber);
                    return;
                }
                route.destination = destination;

                if (tokenEquals(tokenBegin[3], tokenEnd[3], "On-link"))
                {
                    // Сеть подключена напрямую — шлюза нет
                }
                else
                {
                    char buffer[address::maxAddressStringLength];
                    address::IPv6 gateway;
                    if (!tokenToBuffer(tokenBegin[3], tokenEnd[3], buffer, address::maxAddressStringLength) ||
                        !gateway.fromString(buffer))
                    {
                        __blib_log_warning("Router: windows: line %u: bad IPv6 gateway (skipped)", lineNumber);
                        return;
                    }
                    route.gateway = address::Address(gateway);
                }

                // Имя интерфейса — из Interface List по индексу
                // (локальный адрес в IPv6-таблице route print не печатается)
                const InterfaceEntry* entry = findInterface(entries, entryCount, interfaceId);
                if (entry != nullptr)
                {
                    std::memcpy(route.iface.name, entry->name, interfaceNameMax);
                }

                outV6.push_back(route);
            }

            void parseWindowsRoutes(_In const char* data, _In buint32 size,
                _Out RouteVector& outV4, _Out RouteVector& outV6)
            {
                InterfaceEntry interfaces[maxInterfaceEntries] = {};
                buint32 interfaceCount = 0;

                enum class Section : buint8
                {
                    None,
                    Interfaces,
                    IPv4,
                    IPv6
                };
                Section section = Section::None;
                bool tableHeaderSeen = false;

                LineWalker walker = { data, data + size };
                DocLine line;
                buint32 lineNumber = 0;
                while (walker.next(line))
                {
                    ++lineNumber;

                    // Смена секции — приоритетнее всего
                    if (lineContains(line, "Interface List"))
                    {
                        section = Section::Interfaces;
                        tableHeaderSeen = false;
                        continue;
                    }
                    if (lineContains(line, "IPv4 Route Table"))
                    {
                        section = Section::IPv4;
                        tableHeaderSeen = false;
                        continue;
                    }
                    if (lineContains(line, "IPv6 Route Table"))
                    {
                        section = Section::IPv6;
                        tableHeaderSeen = false;
                        continue;
                    }
                    if (lineContains(line, "Persistent Routes"))
                    {
                        section = Section::None;
                        tableHeaderSeen = false;
                        continue;
                    }

                    const char* trimmed = skipSpaces(line.begin, line.end);
                    if (trimmed >= line.end)
                    {
                        continue; // пустая строка
                    }
                    if (*trimmed == '=')
                    {
                        // Разделитель "===...": завершает список интерфейсов
                        // и таблицы маршрутов. Сразу после заголовка секции
                        // ("IPv4 Route Table\n=====") разделитель секцию НЕ
                        // завершает — иначе заголовок таблицы будет потерян.
                        if (section == Section::Interfaces || tableHeaderSeen)
                        {
                            section = Section::None;
                            tableHeaderSeen = false;
                        }
                        continue;
                    }

                    switch (section)
                    {
                    case Section::Interfaces:
                        parseInterfaceLine(line, interfaces, maxInterfaceEntries, interfaceCount, lineNumber);
                        break;
                    case Section::IPv4:
                        if (!tableHeaderSeen)
                        {
                            tableHeaderSeen = lineContains(line, "Network Destination");
                            continue;
                        }
                        parseIPv4RouteLine(line, outV4, lineNumber);
                        break;
                    case Section::IPv6:
                        if (!tableHeaderSeen)
                        {
                            tableHeaderSeen = lineContains(line, "Network Destination");
                            continue;
                        }
                        parseIPv6RouteLine(line, interfaces, interfaceCount, outV6, lineNumber);
                        break;
                    default:
                        break; // строка вне секций — игнорируем
                    }
                }
            }

            // -------------------------------------------------------
            // Поиск маршрута: longest-prefix-match, при равном префиксе —
            // меньшая метрика, при полном равенстве — первый добавленный
            // -------------------------------------------------------

            bool findRouteIPv4(_In const RouteVector& routes, _In const address::IPv4& destination, _Out RouteResult& out)
            {
                bool found = false;
                buint32 bestPrefix = 0;
                buint32 bestMetric = 0;
                Route best{};
                for (buint32 i = 0; i < routes.size(); ++i)
                {
                    const Route& route = routes[i];
                    const address::IPv4Subnet subnet = route.destination.toIPv4Subnet();
                    if (!subnet.isInSubnet(destination))
                    {
                        continue;
                    }
                    const buint32 prefix = subnet.prefix;
                    if (!found || prefix > bestPrefix || (prefix == bestPrefix && route.metric < bestMetric))
                    {
                        best = route;
                        bestPrefix = prefix;
                        bestMetric = route.metric;
                        found = true;
                    }
                }
                if (!found)
                {
                    return false;
                }
                if (isIpAddressType(best.gateway.getType()))
                {
                    out.nextHop = best.gateway;
                }
                else
                {
                    // on-link: пакет уходит напрямую адресату
                    out.nextHop = address::Address(destination);
                }
                out.iface = best.iface;
                out.metric = best.metric;
                return true;
            }

            bool findRouteIPv6(_In const RouteVector& routes, _In const address::IPv6& destination, _Out RouteResult& out)
            {
                bool found = false;
                buint32 bestPrefix = 0;
                buint32 bestMetric = 0;
                Route best{};
                for (buint32 i = 0; i < routes.size(); ++i)
                {
                    const Route& route = routes[i];
                    const address::IPv6Subnet subnet = route.destination.toIPv6Subnet();
                    if (!subnet.isInSubnet(destination))
                    {
                        continue;
                    }
                    const buint32 prefix = subnet.prefix;
                    if (!found || prefix > bestPrefix || (prefix == bestPrefix && route.metric < bestMetric))
                    {
                        best = route;
                        bestPrefix = prefix;
                        bestMetric = route.metric;
                        found = true;
                    }
                }
                if (!found)
                {
                    return false;
                }
                if (isIpAddressType(best.gateway.getType()))
                {
                    out.nextHop = best.gateway;
                }
                else
                {
                    // on-link: пакет уходит напрямую адресату
                    out.nextHop = address::Address(destination);
                }
                out.iface = best.iface;
                out.metric = best.metric;
                return true;
            }
        }

        // -------------------------------------------------------
        // Router
        // -------------------------------------------------------

        Router::Router()
            : containerAllocator()
            , ipv4Routes(blib::memory::StdAllocatorAdapter<Route>(&this->containerAllocator))
            , ipv6Routes(blib::memory::StdAllocatorAdapter<Route>(&this->containerAllocator))
        {
        }

        Router::~Router() = default;

        RouterError Router::loadFromFile(_In const char* path, _In RouteTableFormat format)
        {
            if (path == nullptr || *path == '\0')
            {
                __blib_return_error(RouterError::FileNotFound, "Router: empty file path");
            }

            blib::core::FileStream file;
            blib::core::FileStream::OpenModeFlags mode;
            mode.storage |= static_cast<buint8>(blib::core::OpenMode::Read);
            mode.storage |= static_cast<buint8>(blib::core::OpenMode::Binary);
            if (file.open(path, mode) != blib::core::FileStatus::OK)
            {
                __blib_return_error(RouterError::FileNotFound, "Router: cannot open route table file '%s'", path);
            }

            const blib::core::ByteArray bytes = file.readAll();
            if (bytes.empty())
            {
                __blib_return_error(RouterError::EmptyTable, "Router: route table file '%s' is empty", path);
            }
            return this->loadFromBuffer(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<buint32>(bytes.size()),
                format);
        }

        RouterError Router::loadFromString(_In const char* text, _In RouteTableFormat format)
        {
            if (text == nullptr || *text == '\0')
            {
                return RouterError::EmptyTable;
            }
            return this->loadFromBuffer(text, static_cast<buint32>(std::strlen(text)), format);
        }

        RouterError Router::loadFromBuffer(_In const char* data, _In buint32 size, _In RouteTableFormat format)
        {
            if (data == nullptr || size == 0)
            {
                return RouterError::EmptyTable;
            }

            // Парсим во временные таблицы (общий аллокатор — this):
            // прежние маршруты при ошибке остаются нетронутыми
            RouteVector newV4(blib::memory::StdAllocatorAdapter<Route>(&this->containerAllocator));
            RouteVector newV6(blib::memory::StdAllocatorAdapter<Route>(&this->containerAllocator));

            switch (format)
            {
            case RouteTableFormat::Linux:
                parseLinuxRoutes(data, size, newV4, newV6);
                break;
            case RouteTableFormat::Windows:
                parseWindowsRoutes(data, size, newV4, newV6);
                break;
            case RouteTableFormat::Json:
                __blib_return_error(RouterError::NotSupported,
                    "Router: json route table format is not implemented yet (TODO)");
            default:
                __blib_return_error(RouterError::InvalidData,
                    "Router: unknown route table format %d", static_cast<int>(format));
            }

            const buint64 totalRoutes = static_cast<buint64>(newV4.size()) + static_cast<buint64>(newV6.size());
            if (totalRoutes > routerMaxRoutesTotal)
            {
                __blib_return_error(RouterError::InvalidData,
                    "Router: %llu routes exceed the limit %u", totalRoutes, routerMaxRoutesTotal);
            }
            if (totalRoutes == 0)
            {
                return RouterError::EmptyTable;
            }

            this->ipv4Routes.swap(newV4);
            this->ipv6Routes.swap(newV6);
            return RouterError::None;
        }

        bool Router::route(_In const address::IPv4& destination, _Out RouteResult& out) const
        {
            return findRouteIPv4(this->ipv4Routes, destination, out);
        }

        bool Router::route(_In const address::IPv6& destination, _Out RouteResult& out) const
        {
            return findRouteIPv6(this->ipv6Routes, destination, out);
        }

        bool Router::route(_In const address::Address& destination, _Out RouteResult& out) const
        {
            switch (destination.getType())
            {
            case address::AddressType::IPv4:
                return this->route(destination.toIPv4(), out);
            case address::AddressType::IPv6:
                return this->route(destination.toIPv6(), out);
            default:
                return false; // Mac/подсети/UNDEFINED — не адресат пакета
            }
        }

        buint32 Router::getRouteCount() const
        {
            return static_cast<buint32>(this->ipv4Routes.size() + this->ipv6Routes.size());
        }

        void Router::clear()
        {
            this->ipv4Routes.clear();
            this->ipv6Routes.clear();
        }
    }
}
