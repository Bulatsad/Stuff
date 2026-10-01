#include <blib/network/address.h>
#include <blib/core/console/console.h>

#include <cstdlib>  // std::abort — для __blib_fatal в to*()
#include <cstring>

// Статические адреса-шаблоны (value-семантика: копирование тривиально).
// NoneIPv4 сохранён из старого API как синоним BroadcastIPv4 (255.255.255.255).
const blib::network::address::Address blib::network::address::Address::AnyIPv4 =
    blib::network::address::Address(blib::network::address::IPv4{ 0, 0, 0, 0 });
const blib::network::address::Address blib::network::address::Address::NoneIPv4 =
    blib::network::address::Address(blib::network::address::IPv4{ 255, 255, 255, 255 });
const blib::network::address::Address blib::network::address::Address::LocalhostIPv4 =
    blib::network::address::Address(blib::network::address::IPv4{ 127, 0, 0, 1 });
const blib::network::address::Address blib::network::address::Address::BroadcastIPv4 =
    blib::network::address::Address(blib::network::address::IPv4{ 255, 255, 255, 255 });
const blib::network::address::Address blib::network::address::Address::AnyIPv6 =
    blib::network::address::Address(blib::network::address::IPv6{});
const blib::network::address::Address blib::network::address::Address::LocalhostIPv6 =
    blib::network::address::Address(blib::network::address::IPv6{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 });

namespace blib
{
    namespace network
    {
        namespace address
        {
            namespace
            {
                constexpr char hexAlphabet[] = "0123456789abcdef";

                /**
                 * Записать десятичное число (без нуль-терминатора).
                 * @return курсор после последней записанной цифры
                 */
                char* appendDecimal(_Out char* cursor, buint32 value)
                {
                    // buint32 максимум 4294967295 — 10 цифр
                    constexpr buint32 decimalDigitsMax = 10;
                    char digits[decimalDigitsMax];
                    buint32 count = 0;
                    do
                    {
                        digits[count] = static_cast<char>('0' + (value % 10));
                        ++count;
                        value /= 10;
                    } while (value != 0);

                    // Цифры записаны в обратном порядке — разворачиваем
                    while (count > 0)
                    {
                        --count;
                        *cursor = digits[count];
                        ++cursor;
                    }
                    return cursor;
                }

                /**
                 * Значение hex-цифры ('0'-'9', 'a'-'f', 'A'-'F').
                 * @return false — символ не hex
                 */
                bool hexDigitValue(_In char c, _Out buint32& outValue)
                {
                    if (c >= '0' && c <= '9')
                    {
                        outValue = static_cast<buint32>(c - '0');
                        return true;
                    }
                    if (c >= 'a' && c <= 'f')
                    {
                        outValue = static_cast<buint32>(c - 'a') + 10;
                        return true;
                    }
                    if (c >= 'A' && c <= 'F')
                    {
                        outValue = static_cast<buint32>(c - 'A') + 10;
                        return true;
                    }
                    return false;
                }

                /**
                 * Записать байт двумя hex-цифрами в нижнем регистре ("0a").
                 */
                char* appendHexByte(_Out char* cursor, buint8 value)
                {
                    *cursor = hexAlphabet[value >> 4];
                    ++cursor;
                    *cursor = hexAlphabet[value & 0x0F];
                    ++cursor;
                    return cursor;
                }

                /**
                 * Записать 16-битное слово hex-цифрами без ведущих нулей
                 * (правило RFC 5952: группы печатаются в нижнем регистре).
                 */
                char* appendHexWord(_Out char* cursor, buint16 value)
                {
                    bool started = false;
                    for (buint32 shift = 12; shift > 0; shift -= 4)
                    {
                        const buint32 digit = (static_cast<buint32>(value) >> shift) & 0x0F;
                        if (digit != 0)
                        {
                            started = true;
                        }
                        if (started)
                        {
                            *cursor = hexAlphabet[digit];
                            ++cursor;
                        }
                    }
                    // Младшая цифра печатается всегда
                    *cursor = hexAlphabet[value & 0x0F];
                    ++cursor;
                    return cursor;
                }

                /**
                 * Разобрать десятичное число, занимающее ВСЮ строку.
                 * @return false — пустая строка / нецифровой символ /
                 *         переполнение buint32
                 */
                bool parseDecimal(_In const char* str, _Out buint32& outValue)
                {
                    if (str == nullptr || *str == '\0')
                    {
                        return false;
                    }
                    buint32 value = 0;
                    const char* p = str;
                    while (*p != '\0')
                    {
                        if (*p < '0' || *p > '9')
                        {
                            return false;
                        }
                        const buint32 digit = static_cast<buint32>(*p - '0');
                        if (value > (buint32Max - digit) / 10)
                        {
                            return false; // переполнение
                        }
                        value = value * 10 + digit;
                        ++p;
                    }
                    outValue = value;
                    return true;
                }

                /**
                 * Общий парсер эндпоинта "ip:port": формы "1.2.3.4:8080"
                 * и "[2001:db8::1]:8080". Голый IPv6 без скобок НЕ
                 * принимается: последнее ':' неоднозначно отделяет порт
                 * ("::1" — адрес "::" с портом 1 или адрес "::1"?).
                 */
                bool parseEndpointString(_In const char* str, _Out Address& outIp, _Out buint16& outPort)
                {
                    if (str == nullptr)
                    {
                        return false;
                    }

                    if (*str == '[')
                    {
                        // Скобочная форма: "[ipv6]:port"
                        const char* closeBracket = strchr(str + 1, ']');
                        if (closeBracket == nullptr || closeBracket[1] != ':')
                        {
                            return false;
                        }
                        const buint32 ipLength = static_cast<buint32>(closeBracket - (str + 1));
                        if (ipLength == 0 || ipLength >= maxAddressStringLength - 1)
                        {
                            return false;
                        }
                        char ipPart[maxAddressStringLength];
                        memcpy(ipPart, str + 1, ipLength);
                        ipPart[ipLength] = '\0';

                        IPv6 ipv6;
                        if (!ipv6.fromString(ipPart))
                        {
                            return false;
                        }
                        buint32 portValue = 0;
                        if (!parseDecimal(closeBracket + 2, portValue) || portValue > buint16Max)
                        {
                            return false;
                        }
                        outIp = Address(ipv6);
                        outPort = static_cast<buint16>(portValue);
                        return true;
                    }

                    // Бесскобочная форма: "1.2.3.4:8080" (только IPv4)
                    const char* lastColon = strrchr(str, ':');
                    if (lastColon == nullptr || lastColon[1] == '\0')
                    {
                        return false;
                    }
                    const buint32 ipLength = static_cast<buint32>(lastColon - str);
                    if (ipLength == 0 || ipLength >= maxAddressStringLength - 1)
                    {
                        return false;
                    }
                    char ipPart[maxAddressStringLength];
                    memcpy(ipPart, str, ipLength);
                    ipPart[ipLength] = '\0';

                    IPv4 ipv4;
                    if (!ipv4.fromString(ipPart))
                    {
                        return false;
                    }
                    buint32 portValue = 0;
                    if (!parseDecimal(lastColon + 1, portValue) || portValue > buint16Max)
                    {
                        return false;
                    }
                    outIp = Address(ipv4);
                    outPort = static_cast<buint16>(portValue);
                    return true;
                }

                /**
                 * Общий форматтер эндпоинта: "1.2.3.4:8080" /
                 * "[2001:db8::1]:8080" (IPv6 — в скобках, однозначно).
                 */
                bool endpointToString(_In const Address& ip, _In buint16 port,
                    _Out char* buffer, _In buint32 bufferSize)
                {
                    if (buffer == nullptr)
                    {
                        return false;
                    }

                    // Форматируем в локальный буфер гарантированного
                    // размера, затем проверяем влезание и копируем
                    char local[maxAddressStringLength];
                    char* cursor = local;

                    if (ip.getType() == AddressType::IPv6)
                    {
                        *cursor = '[';
                        ++cursor;
                        if (!ip.toString(cursor, maxAddressStringLength - 1))
                        {
                            return false;
                        }
                        cursor += strlen(cursor);
                        *cursor = ']';
                        ++cursor;
                    }
                    else if (ip.getType() == AddressType::IPv4)
                    {
                        if (!ip.toString(cursor, maxAddressStringLength))
                        {
                            return false;
                        }
                        cursor += strlen(cursor);
                    }
                    else
                    {
                        // UNDEFINED/Mac — не IP-эндпоинт
                        return false;
                    }

                    *cursor = ':';
                    ++cursor;
                    cursor = appendDecimal(cursor, port);
                    *cursor = '\0';

                    const buint32 length = static_cast<buint32>(cursor - local);
                    if (bufferSize <= length)
                    {
                        return false;
                    }
                    memcpy(buffer, local, length + 1);
                    return true;
                }
            }

            // -------------------------------------------------------
            // Mac
            // -------------------------------------------------------

            bool Mac::toString(_Out char* buffer, _In buint32 bufferSize) const
            {
                if (buffer == nullptr || bufferSize <= macStringLength)
                {
                    return false;
                }
                char* cursor = buffer;
                for (buint32 i = 0; i < macBytesCount; ++i)
                {
                    if (i != 0)
                    {
                        *cursor = ':';
                        ++cursor;
                    }
                    cursor = appendHexByte(cursor, this->bytes[i]);
                }
                *cursor = '\0';
                return true;
            }

            bool Mac::fromString(_In const char* str)
            {
                *this = Mac(); // сброс к нулевому адресу при неудаче
                if (str == nullptr)
                {
                    return false;
                }

                char separator = '\0';
                buint32 groupIndex = 0;
                buint32 digitCount = 0;
                buint32 value = 0;
                const char* p = str;
                char c = '\0';
                while (true)
                {
                    c = *p;
                    buint32 digitValue = 0;
                    if (hexDigitValue(c, digitValue))
                    {
                        ++digitCount;
                        if (digitCount > 2)
                        {
                            return false; // больше 2 цифр в группе
                        }
                        value = (value << 4) | digitValue;
                        ++p;
                        continue;
                    }
                    if (c == ':' || c == '-')
                    {
                        // Группа обязана быть ровно из 2 цифр;
                        // разделителей максимум 5 (групп — 6)
                        if (digitCount != 2 || groupIndex >= macBytesCount - 1)
                        {
                            return false;
                        }
                        if (separator == '\0')
                        {
                            separator = c;
                        }
                        if (c != separator)
                        {
                            return false; // ':' и '-' не смешиваются
                        }
                        this->bytes[groupIndex] = static_cast<buint8>(value);
                        ++groupIndex;
                        value = 0;
                        digitCount = 0;
                        ++p;
                        continue;
                    }
                    break; // '\0' или невалидный символ
                }

                // Последняя (шестая) группа — без хвостового разделителя
                if (c == '\0' && digitCount == 2 && groupIndex == macBytesCount - 1)
                {
                    this->bytes[groupIndex] = static_cast<buint8>(value);
                    return true;
                }
                return false;
            }

            // -------------------------------------------------------
            // IPv4
            // -------------------------------------------------------

            bool IPv4::toString(_Out char* buffer, _In buint32 bufferSize) const
            {
                if (buffer == nullptr || bufferSize <= ipv4StringLength)
                {
                    return false;
                }
                char* cursor = buffer;
                for (buint32 i = 0; i < ipv4BytesCount; ++i)
                {
                    if (i != 0)
                    {
                        *cursor = '.';
                        ++cursor;
                    }
                    cursor = appendDecimal(cursor, this->bytes[i]);
                }
                *cursor = '\0';
                return true;
            }

            bool IPv4::fromString(_In const char* str)
            {
                *this = IPv4(); // сброс к 0.0.0.0 при неудаче
                if (str == nullptr || *str == '\0')
                {
                    return false;
                }

                buint8 octets[ipv4BytesCount] = {};
                buint32 octetIndex = 0;
                buint32 value = 0;
                buint32 digitCount = 0;
                const char* p = str;
                char c = '\0';
                while (true)
                {
                    c = *p;
                    if (c >= '0' && c <= '9')
                    {
                        ++digitCount;
                        value = value * 10 + static_cast<buint32>(c - '0');
                        if (value > 255)
                        {
                            return false; // октет больше 255
                        }
                        ++p;
                        continue;
                    }
                    if (c == '.')
                    {
                        // Разделитель: до и после обязаны быть цифры;
                        // разделителей максимум 3
                        if (digitCount == 0 || octetIndex >= ipv4BytesCount - 1)
                        {
                            return false;
                        }
                        octets[octetIndex] = static_cast<buint8>(value);
                        ++octetIndex;
                        value = 0;
                        digitCount = 0;
                        ++p;
                        continue;
                    }
                    break; // '\0' или невалидный символ
                }

                // Последний октет — без хвостовой точки
                if (c == '\0' && digitCount > 0 && octetIndex == ipv4BytesCount - 1)
                {
                    octets[octetIndex] = static_cast<buint8>(value);
                    for (buint32 i = 0; i < ipv4BytesCount; ++i)
                    {
                        this->bytes[i] = octets[i];
                    }
                    return true;
                }
                return false;
            }

            // -------------------------------------------------------
            // IPv6
            // -------------------------------------------------------

            bool IPv6::toString(_Out char* buffer, _In buint32 bufferSize) const
            {
                if (buffer == nullptr || bufferSize <= ipv6StringLength)
                {
                    return false;
                }

                constexpr buint32 groupCountMax = 8;
                buint16 groups[groupCountMax];
                for (buint32 i = 0; i < groupCountMax; ++i)
                {
                    groups[i] = static_cast<buint16>(
                        (static_cast<buint16>(this->bytes[i * 2]) << 8) |
                        static_cast<buint16>(this->bytes[i * 2 + 1]));
                }

                // Embedded-IPv4 (RFC 4291): первые 96 бит — нули, хвост
                // 32 бита печатается десятичной dotted-формой, как
                // inet_ntop. По RFC 5952 (sec. 4) dotted-хвост — ТОЛЬКО
                // у IPv4-mapped (bytes[10..11] = ff ff), иначе
                // "::1" выглядел бы как "::0.0.0.1". IPv4-compatible
                // (00 00) печатается обычным hex.
                const bool isMappedIpv4 =
                    groups[0] == 0 && groups[1] == 0 && groups[2] == 0 &&
                    groups[3] == 0 && groups[4] == 0 && groups[5] == 0xFFFF;
                if (isMappedIpv4)
                {
                    char* cursor = buffer;
                    *cursor = ':';
                    ++cursor;
                    *cursor = ':';
                    ++cursor;
                    cursor = appendHexWord(cursor, 0xFFFF);
                    *cursor = ':';
                    ++cursor;
                    const buint8 tailBytes[ipv4BytesCount] = {
                        static_cast<buint8>(groups[6] >> 8),
                        static_cast<buint8>(groups[6] & 0xFF),
                        static_cast<buint8>(groups[7] >> 8),
                        static_cast<buint8>(groups[7] & 0xFF)
                    };
                    for (buint32 i = 0; i < ipv4BytesCount; ++i)
                    {
                        if (i != 0)
                        {
                            *cursor = '.';
                            ++cursor;
                        }
                        cursor = appendDecimal(cursor, tailBytes[i]);
                    }
                    *cursor = '\0';
                    return true;
                }

                // Самый длинный прогон нулевых групп (минимум 2);
                // при равных длинах — первый (RFC 5952). Прогон
                // заменяется "::".
                buint32 bestStart = 0;
                buint32 bestLength = 0;
                buint32 runStart = 0;
                buint32 runLength = 0;
                for (buint32 i = 0; i < groupCountMax; ++i)
                {
                    if (groups[i] == 0)
                    {
                        ++runLength;
                    }
                    else
                    {
                        runStart = i + 1;
                        runLength = 0;
                    }
                    if (runLength > bestLength)
                    {
                        bestStart = i + 1 - runLength;
                        bestLength = runLength;
                    }
                }
                if (bestLength < 2)
                {
                    bestStart = 0;
                    bestLength = 0; // сжимать нечего
                }

                char* cursor = buffer;
                buint32 i = 0;
                while (i < groupCountMax)
                {
                    if (bestLength >= 2 && i == bestStart)
                    {
                        *cursor = ':';
                        ++cursor;
                        *cursor = ':';
                        ++cursor;
                        i += bestLength;
                        if (i >= groupCountMax)
                        {
                            break; // "::" в самом конце
                        }
                        continue; // разделитель перед следующей группой не нужен
                    }
                    cursor = appendHexWord(cursor, groups[i]);
                    ++i;
                    if (i < groupCountMax && !(bestLength >= 2 && i == bestStart))
                    {
                        *cursor = ':';
                        ++cursor;
                    }
                }
                *cursor = '\0';
                return true;
            }

            bool IPv6::fromString(_In const char* str)
            {
                *this = IPv6(); // сброс к :: при неудаче
                if (str == nullptr || *str == '\0')
                {
                    return false;
                }

                constexpr buint32 groupCountMax = 8;
                constexpr buint32 noCompression = groupCountMax; // маркер отсутствия "::"

                buint16 groups[groupCountMax] = {};
                buint32 groupCount = 0;
                buint32 compressionAt = noCompression;
                buint32 value = 0;
                buint32 digitCount = 0;
                // Начало текущей группы: точка embedded-IPv4 хвоста
                // ("::ffff:192.168.1.1") перечитывает хвост с первой
                // цифры — hex-накопление до точки не годится (октеты
                // десятичные)
                const char* groupStart = str;
                bool hasIpv4Tail = false;
                IPv4 ipv4Tail;
                const char* p = str;
                char c = '\0';
                while (true)
                {
                    c = *p;
                    buint32 digitValue = 0;
                    if (hexDigitValue(c, digitValue))
                    {
                        if (digitCount == 0)
                        {
                            groupStart = p;
                        }
                        ++digitCount;
                        if (digitCount > 4)
                        {
                            return false; // больше 4 цифр в группе
                        }
                        value = (value << 4) | digitValue;
                        ++p;
                        continue;
                    }
                    if (c == '.')
                    {
                        // Embedded-IPv4 хвост (RFC 4291): от начала
                        // текущей группы и до конца строки — десятичный
                        // IPv4 (fromString сам требует '\0' после).
                        // Хвост занимает ПОСЛЕДНИЕ 2 группы (32 бита).
                        if (digitCount == 0 || hasIpv4Tail)
                        {
                            return false; // точка без цифр / второй хвост
                        }
                        if (!ipv4Tail.fromString(groupStart))
                        {
                            return false;
                        }
                        hasIpv4Tail = true;
                        digitCount = 0;
                        value = 0;
                        break; // хвост — конец строки
                    }
                    if (c == ':')
                    {
                        // Была ли законченная группа слева от ':'
                        // (проверяем ДО обнуления счётчика ниже)
                        const bool hadGroup = (digitCount > 0);
                        if (hadGroup)
                        {
                            // Фиксируем законченную группу
                            if (groupCount >= groupCountMax)
                            {
                                return false; // больше 8 групп
                            }
                            groups[groupCount] = static_cast<buint16>(value);
                            ++groupCount;
                            value = 0;
                            digitCount = 0;
                        }
                        if (p[1] == ':')
                        {
                            // "::" — ровно один раз
                            if (compressionAt != noCompression)
                            {
                                return false;
                            }
                            compressionAt = groupCount;
                            ++p; // пропускаем второй ':'
                        }
                        else if (!hadGroup)
                        {
                            // Одиночный ':' без группы слева ("1:" в
                            // конце или ":1" в начале) — невалидно
                            return false;
                        }
                        else
                        {
                            // После одиночного ':' обязана начаться
                            // группа ("1:2:3:4:5:6:7:8:" — невалидно)
                            buint32 nextDigit = 0;
                            if (!hexDigitValue(p[1], nextDigit))
                            {
                                return false;
                            }
                        }
                        ++p;
                        continue;
                    }
                    break; // '\0' или невалидный символ
                }

                if (!hasIpv4Tail && c != '\0')
                {
                    return false; // невалидный символ
                }
                if (!hasIpv4Tail && digitCount > 0)
                {
                    // Хвостовая группа без ':' после неё
                    if (groupCount >= groupCountMax)
                    {
                        return false;
                    }
                    groups[groupCount] = static_cast<buint16>(value);
                    ++groupCount;
                }

                // Количество групп: хвост считается за 2 группы.
                // Без "::" групп ровно 8; с "::" — меньше 8
                if (hasIpv4Tail)
                {
                    if (compressionAt == noCompression)
                    {
                        if (groupCount + 2 != groupCountMax)
                        {
                            return false; // ровно 6 hex-групп + хвост
                        }
                    }
                    else if (groupCount + 2 >= groupCountMax)
                    {
                        return false; // с "::" групп меньше 8
                    }
                }
                else
                {
                    if (compressionAt == noCompression)
                    {
                        if (groupCount != groupCountMax)
                        {
                            return false;
                        }
                    }
                    else if (groupCount >= groupCountMax)
                    {
                        return false;
                    }
                }

                // Сборка 16 байт из 8 групп (big-endian)
                buint16 finalGroups[groupCountMax] = {};
                if (hasIpv4Tail)
                {
                    // Явные hex-группы; нули заполняют промежуток "::";
                    // хвост — последние 2 группы
                    if (compressionAt == noCompression)
                    {
                        for (buint32 i = 0; i < groupCount; ++i)
                        {
                            finalGroups[i] = groups[i];
                        }
                    }
                    else
                    {
                        const buint32 zerosCount = groupCountMax - 2 - groupCount;
                        // Группы до "::"
                        for (buint32 i = 0; i < compressionAt; ++i)
                        {
                            finalGroups[i] = groups[i];
                        }
                        // Группы после "::" — перед хвостом
                        for (buint32 i = compressionAt; i < groupCount; ++i)
                        {
                            finalGroups[i + zerosCount] = groups[i];
                        }
                    }
                    finalGroups[groupCountMax - 2] = static_cast<buint16>(
                        (static_cast<buint16>(ipv4Tail.bytes[0]) << 8) |
                        static_cast<buint16>(ipv4Tail.bytes[1]));
                    finalGroups[groupCountMax - 1] = static_cast<buint16>(
                        (static_cast<buint16>(ipv4Tail.bytes[2]) << 8) |
                        static_cast<buint16>(ipv4Tail.bytes[3]));
                }
                else
                {
                    if (compressionAt == noCompression)
                    {
                        for (buint32 i = 0; i < groupCountMax; ++i)
                        {
                            finalGroups[i] = groups[i];
                        }
                    }
                    else
                    {
                        const buint32 zerosCount = groupCountMax - groupCount;
                        // Группы до "::"
                        for (buint32 i = 0; i < compressionAt; ++i)
                        {
                            finalGroups[i] = groups[i];
                        }
                        // Нули занимают [compressionAt, compressionAt + zerosCount)
                        // Группы после "::" — в хвост
                        for (buint32 i = compressionAt; i < groupCount; ++i)
                        {
                            finalGroups[i + zerosCount] = groups[i];
                        }
                    }
                }

                for (buint32 i = 0; i < groupCountMax; ++i)
                {
                    this->bytes[i * 2] = static_cast<buint8>(finalGroups[i] >> 8);
                    this->bytes[i * 2 + 1] = static_cast<buint8>(finalGroups[i] & 0xFF);
                }
                return true;
            }

            // -------------------------------------------------------
            // IPv4Subnet
            // -------------------------------------------------------

            bool IPv4Subnet::isInSubnet(_In const IPv4& ip) const
            {
                if (this->prefix > ipv4PrefixMax)
                {
                    return false; // испорченный инвариант (не бывает в норме)
                }

                const buint32 ipValue =
                    (static_cast<buint32>(ip.bytes[0]) << 24) |
                    (static_cast<buint32>(ip.bytes[1]) << 16) |
                    (static_cast<buint32>(ip.bytes[2]) << 8) |
                    static_cast<buint32>(ip.bytes[3]);
                const buint32 networkValue =
                    (static_cast<buint32>(this->network.bytes[0]) << 24) |
                    (static_cast<buint32>(this->network.bytes[1]) << 16) |
                    (static_cast<buint32>(this->network.bytes[2]) << 8) |
                    static_cast<buint32>(this->network.bytes[3]);

                // prefix == 0 — совпадает всё (сдвиг на 32 — UB, отдельно)
                const buint32 mask =
                    (this->prefix == 0) ? 0 : (buint32Max << (ipv4PrefixMax - this->prefix));
                return (ipValue & mask) == (networkValue & mask);
            }

            bool IPv4Subnet::toString(_Out char* buffer, _In buint32 bufferSize) const
            {
                if (buffer == nullptr)
                {
                    return false;
                }
                // "255.255.255.255/32" — 18 символов + нуль-терминатор
                constexpr buint32 ipv4SubnetStringLength = ipv4StringLength + 1 + 2;
                if (bufferSize <= ipv4SubnetStringLength)
                {
                    return false;
                }
                if (!this->network.toString(buffer, bufferSize))
                {
                    return false;
                }
                char* cursor = buffer + strlen(buffer);
                *cursor = '/';
                ++cursor;
                cursor = appendDecimal(cursor, this->prefix);
                *cursor = '\0';
                return true;
            }

            bool IPv4Subnet::fromString(_In const char* str)
            {
                *this = IPv4Subnet(); // сброс (нулевая сеть, prefix 0)
                if (str == nullptr)
                {
                    return false;
                }
                const char* slash = strchr(str, '/');
                if (slash == nullptr || slash[1] == '\0')
                {
                    return false;
                }
                const buint32 ipLength = static_cast<buint32>(slash - str);
                if (ipLength == 0 || ipLength >= maxAddressStringLength - 1)
                {
                    return false;
                }
                char ipPart[maxAddressStringLength];
                memcpy(ipPart, str, ipLength);
                ipPart[ipLength] = '\0';

                IPv4 network;
                if (!network.fromString(ipPart))
                {
                    return false;
                }
                buint32 prefixValue = 0;
                if (!parseDecimal(slash + 1, prefixValue) || prefixValue > ipv4PrefixMax)
                {
                    return false;
                }
                this->network = network;
                this->prefix = static_cast<buint8>(prefixValue);
                return true;
            }

            // -------------------------------------------------------
            // IPv6Subnet
            // -------------------------------------------------------

            bool IPv6Subnet::isInSubnet(_In const IPv6& ip) const
            {
                if (this->prefix > ipv6PrefixMax)
                {
                    return false; // испорченный инвариант (не бывает в норме)
                }

                // Побайтово: полные байты префикса + битовая маска остатка
                const buint32 fullBytes = this->prefix / 8;
                const buint32 remainingBits = this->prefix % 8;
                for (buint32 i = 0; i < fullBytes; ++i)
                {
                    if (ip.bytes[i] != this->network.bytes[i])
                    {
                        return false;
                    }
                }
                if (remainingBits != 0)
                {
                    const buint8 mask = static_cast<buint8>(0xFF << (8 - remainingBits));
                    if ((ip.bytes[fullBytes] & mask) != (this->network.bytes[fullBytes] & mask))
                    {
                        return false;
                    }
                }
                return true;
            }

            bool IPv6Subnet::toString(_Out char* buffer, _In buint32 bufferSize) const
            {
                if (buffer == nullptr)
                {
                    return false;
                }
                // "ffff:...:ffff/128" — максимум 45 + 4 символа
                constexpr buint32 ipv6SubnetStringLength = ipv6StringLength + 1 + 3;
                if (bufferSize <= ipv6SubnetStringLength)
                {
                    return false;
                }
                if (!this->network.toString(buffer, bufferSize))
                {
                    return false;
                }
                char* cursor = buffer + strlen(buffer);
                *cursor = '/';
                ++cursor;
                cursor = appendDecimal(cursor, this->prefix);
                *cursor = '\0';
                return true;
            }

            bool IPv6Subnet::fromString(_In const char* str)
            {
                *this = IPv6Subnet(); // сброс (нулевая сеть, prefix 0)
                if (str == nullptr)
                {
                    return false;
                }
                const char* slash = strchr(str, '/');
                if (slash == nullptr || slash[1] == '\0')
                {
                    return false;
                }
                const buint32 ipLength = static_cast<buint32>(slash - str);
                if (ipLength == 0 || ipLength >= maxAddressStringLength - 1)
                {
                    return false;
                }
                char ipPart[maxAddressStringLength];
                memcpy(ipPart, str, ipLength);
                ipPart[ipLength] = '\0';

                IPv6 network;
                if (!network.fromString(ipPart))
                {
                    return false;
                }
                buint32 prefixValue = 0;
                if (!parseDecimal(slash + 1, prefixValue) || prefixValue > ipv6PrefixMax)
                {
                    return false;
                }
                this->network = network;
                this->prefix = static_cast<buint8>(prefixValue);
                return true;
            }

            // -------------------------------------------------------
            // Address
            // -------------------------------------------------------

            Address::Address()
                : tag(AddressType::UNDEFINED)
                , storage() // zero-init: union-конструктор инициализирует первый член
            {
            }

            Address::Address(_In const Mac& mac)
                : tag(AddressType::Mac)
                , storage()
            {
                this->storage.mac = mac;
            }

            Address::Address(_In const IPv4& ipv4)
                : tag(AddressType::IPv4)
                , storage()
            {
                this->storage.ipv4 = ipv4;
            }

            Address::Address(_In const IPv6& ipv6)
                : tag(AddressType::IPv6)
                , storage()
            {
                this->storage.ipv6 = ipv6;
            }

            Address::Address(_In const IPv4Subnet& subnet)
                : tag(AddressType::IPv4Subnet)
                , storage()
            {
                this->storage.ipv4Subnet = subnet;
            }

            Address::Address(_In const IPv6Subnet& subnet)
                : tag(AddressType::IPv6Subnet)
                , storage()
            {
                this->storage.ipv6Subnet = subnet;
            }

            Mac Address::toMac() const
            {
                if (__blib_unlikely(this->tag != AddressType::Mac))
                {
                    __blib_fatal("Address::toMac: stored type is not Mac (tag=%d)",
                        static_cast<int>(this->tag));
                }
                return this->storage.mac;
            }

            IPv4 Address::toIPv4() const
            {
                if (__blib_unlikely(this->tag != AddressType::IPv4))
                {
                    __blib_fatal("Address::toIPv4: stored type is not IPv4 (tag=%d)",
                        static_cast<int>(this->tag));
                }
                return this->storage.ipv4;
            }

            IPv6 Address::toIPv6() const
            {
                if (__blib_unlikely(this->tag != AddressType::IPv6))
                {
                    __blib_fatal("Address::toIPv6: stored type is not IPv6 (tag=%d)",
                        static_cast<int>(this->tag));
                }
                return this->storage.ipv6;
            }

            IPv4Subnet Address::toIPv4Subnet() const
            {
                if (__blib_unlikely(this->tag != AddressType::IPv4Subnet))
                {
                    __blib_fatal("Address::toIPv4Subnet: stored type is not IPv4Subnet (tag=%d)",
                        static_cast<int>(this->tag));
                }
                return this->storage.ipv4Subnet;
            }

            IPv6Subnet Address::toIPv6Subnet() const
            {
                if (__blib_unlikely(this->tag != AddressType::IPv6Subnet))
                {
                    __blib_fatal("Address::toIPv6Subnet: stored type is not IPv6Subnet (tag=%d)",
                        static_cast<int>(this->tag));
                }
                return this->storage.ipv6Subnet;
            }

            bool Address::operator==(_In const Address& other) const
            {
                if (this->tag != other.tag)
                {
                    return false;
                }
                switch (this->tag)
                {
                case AddressType::Mac:        return this->storage.mac == other.storage.mac;
                case AddressType::IPv4:       return this->storage.ipv4 == other.storage.ipv4;
                case AddressType::IPv6:       return this->storage.ipv6 == other.storage.ipv6;
                case AddressType::IPv4Subnet: return this->storage.ipv4Subnet == other.storage.ipv4Subnet;
                case AddressType::IPv6Subnet: return this->storage.ipv6Subnet == other.storage.ipv6Subnet;
                default:
                    return true; // UNDEFINED == UNDEFINED
                }
            }

            bool Address::toString(_Out char* buffer, _In buint32 bufferSize) const
            {
                switch (this->tag)
                {
                case AddressType::Mac:        return this->storage.mac.toString(buffer, bufferSize);
                case AddressType::IPv4:       return this->storage.ipv4.toString(buffer, bufferSize);
                case AddressType::IPv6:       return this->storage.ipv6.toString(buffer, bufferSize);
                case AddressType::IPv4Subnet: return this->storage.ipv4Subnet.toString(buffer, bufferSize);
                case AddressType::IPv6Subnet: return this->storage.ipv6Subnet.toString(buffer, bufferSize);
                default:
                    return false; // UNDEFINED — форматировать нечего
                }
            }

            Address Address::fromString(_In const char* str, _Out_opt bool* ok)
            {
                if (ok != nullptr)
                {
                    *ok = false;
                }
                if (str == nullptr || *str == '\0')
                {
                    return Address();
                }

                // "ip/prefix" — подсеть (MAC-подсетей не бывает)
                if (strchr(str, '/') != nullptr)
                {
                    IPv6Subnet ipv6Subnet;
                    if (ipv6Subnet.fromString(str))
                    {
                        if (ok != nullptr)
                        {
                            *ok = true;
                        }
                        return Address(ipv6Subnet);
                    }
                    IPv4Subnet ipv4Subnet;
                    if (ipv4Subnet.fromString(str))
                    {
                        if (ok != nullptr)
                        {
                            *ok = true;
                        }
                        return Address(ipv4Subnet);
                    }
                    return Address();
                }

                // Порядок важен: Mac пробуется первым (его форма
                // "xx:xx:xx:xx:xx:xx" — подмножество синтаксиса IPv6),
                // затем IPv6 (любая форма с ':'), затем IPv4
                Mac mac;
                if (mac.fromString(str))
                {
                    if (ok != nullptr)
                    {
                        *ok = true;
                    }
                    return Address(mac);
                }
                IPv6 ipv6;
                if (ipv6.fromString(str))
                {
                    if (ok != nullptr)
                    {
                        *ok = true;
                    }
                    return Address(ipv6);
                }
                IPv4 ipv4;
                if (ipv4.fromString(str))
                {
                    if (ok != nullptr)
                    {
                        *ok = true;
                    }
                    return Address(ipv4);
                }
                return Address();
            }

            // -------------------------------------------------------
            // Tcp
            // -------------------------------------------------------

            bool Tcp::toString(_Out char* buffer, _In buint32 bufferSize) const
            {
                return endpointToString(this->ip, this->port, buffer, bufferSize);
            }

            bool Tcp::fromString(_In const char* str)
            {
                *this = Tcp(); // сброс (UNDEFINED, порт 0)
                return parseEndpointString(str, this->ip, this->port);
            }

            // -------------------------------------------------------
            // Udp
            // -------------------------------------------------------

            bool Udp::toString(_Out char* buffer, _In buint32 bufferSize) const
            {
                return endpointToString(this->ip, this->port, buffer, bufferSize);
            }

            bool Udp::fromString(_In const char* str)
            {
                *this = Udp(); // сброс (UNDEFINED, порт 0)
                return parseEndpointString(str, this->ip, this->port);
            }
        }
    }
}
