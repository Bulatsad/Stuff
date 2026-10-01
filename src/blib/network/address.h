#pragma once

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>

namespace blib
{
    namespace network
    {
        /**
         * Адресные типы сети: конкретные адреса (Mac, IPv4, IPv6),
         * подсети (IPv4Subnet, IPv6Subnet), универсальный вариант
         * Address и транспортные эндпоинты (Tcp, Udp — адрес + порт).
         *
         * Инварианты:
         * - Все типы — value-семантики: POD-хранилище, тривиальное
         *   копирование, ноль аллокаций и деструкторов;
         * - IP-адреса хранят байты в сетевом порядке (первый байт —
         *   старший октет); порт — host-порядок, перевод в сетевой
         *   (htons/ntohs) — только на границе winsock (impl/win);
         * - Address — вариант (union + тег): активный член определяет
         *   getType(); to*() при несовпадении тега — fatal (контракт:
         *   вызывающий обязан проверить getType()).
         *
         * См. NETWORK.md (модульный док).
         */
        namespace address
        {
            class Address;
            class Mac;
            class IPv4;
            class IPv6;
            class IPv4Subnet;
            class IPv6Subnet;
            class Tcp;
            class Udp;

            /**
             * Тип адреса (значение тега варианта Address).
             *
             * Объявлен в неймспейсе, а не вложен в Address: конкретные
             * типы (union-члены Address) обязаны видеть его ДО своего
             * определения, а вложенный в неполный класс тип недоступен.
             * Внутри Address — алиас Address::Type (синтаксис обращения
             * к значениям: Address::Type::IPv4).
             */
            enum class AddressType : buint8
            {
                Mac,
                IPv4,
                IPv6,
                IPv4Subnet,
                IPv6Subnet,

                UNDEFINED,   // пустой вариант (Address по умолчанию)

                END_OF_ENUM
            };

            // Размеры представлений в байтах
            constexpr buint32 macBytesCount = 6;
            constexpr buint32 ipv4BytesCount = 4;
            constexpr buint32 ipv6BytesCount = 16;

            // Максимальные длины префиксов подсетей (в битах)
            constexpr buint32 ipv4PrefixMax = 32;
            constexpr buint32 ipv6PrefixMax = 128;

            // Длины строковых представлений (без нуль-терминатора)
            constexpr buint32 macStringLength = 17;    // "aa:bb:cc:dd:ee:ff"
            constexpr buint32 ipv4StringLength = 15;   // "255.255.255.255"
            constexpr buint32 ipv6StringLength = 45;   // максимум IPv6 (сжатый, RFC 5952)

            // Гарантированно достаточный буфер для любого toString
            // (худший случай: IPv6 в скобках + ":65535" или "/128")
            constexpr buint32 maxAddressStringLength = 64;

            /**
             * MAC-адрес (6 байт). Строка: "aa:bb:cc:dd:ee:ff"
             * (hex в нижнем регистре, разделитель ':').
             */
            class Mac
            {
            public:
                // Байты в порядке записи (bytes[0] — старший байт)
                buint8 bytes[macBytesCount] = {};

                bool operator==(_In const Mac& other) const
                {
                    for (buint32 i = 0; i < macBytesCount; ++i)
                    {
                        if (this->bytes[i] != other.bytes[i])
                        {
                            return false;
                        }
                    }
                    return true;
                }

                bool operator!=(_In const Mac& other) const
                {
                    return !(*this == other);
                }

                AddressType getType() const { return AddressType::Mac; }

                /**
                 * Форматировать в буфер ("aa:bb:cc:dd:ee:ff").
                 *
                 * @return false — буфер nullptr или не вмещает строку
                 *         с нуль-терминатором (объект не изменяется)
                 */
                bool toString(_Out char* buffer, _In buint32 bufferSize) const;

                /**
                 * Разобрать из строки ("aa:bb:cc:dd:ee:ff"; допускается
                 * разделитель ':' или '-', но не смешанный). При неудаче
                 * объект сбрасывается к нулевому адресу.
                 */
                bool fromString(_In const char* str);
            };

            /**
             * IPv4-адрес (4 байта). Строка: "1.2.3.4" (десятичные
             * октеты 0-255, разделитель '.').
             */
            class IPv4
            {
            public:
                // Байты в сетевом порядке (bytes[0] — старший октет)
                buint8 bytes[ipv4BytesCount] = {};

                bool operator==(_In const IPv4& other) const
                {
                    for (buint32 i = 0; i < ipv4BytesCount; ++i)
                    {
                        if (this->bytes[i] != other.bytes[i])
                        {
                            return false;
                        }
                    }
                    return true;
                }

                bool operator!=(_In const IPv4& other) const
                {
                    return !(*this == other);
                }

                AddressType getType() const { return AddressType::IPv4; }

                /**
                 * Форматировать в буфер ("1.2.3.4").
                 *
                 * @return false — буфер nullptr или не вмещает строку
                 *         с нуль-терминатором
                 */
                bool toString(_Out char* buffer, _In buint32 bufferSize) const;

                /**
                 * Разобрать из строки ("1.2.3.4"). При неудаче объект
                 * сбрасывается к нулевому адресу (0.0.0.0).
                 */
                bool fromString(_In const char* str);
            };

            /**
             * IPv6-адрес (16 байт). Строка: hex-группы (1-4 цифры),
             * сжатие "::" по RFC 5952 (нижний регистр, без ведущих
             * нулей, сжимается самый длинный прогон нулевых групп).
             */
            class IPv6
            {
            public:
                // Байты в сетевом порядке (bytes[0] — старший байт)
                buint8 bytes[ipv6BytesCount] = {};

                bool operator==(_In const IPv6& other) const
                {
                    for (buint32 i = 0; i < ipv6BytesCount; ++i)
                    {
                        if (this->bytes[i] != other.bytes[i])
                        {
                            return false;
                        }
                    }
                    return true;
                }

                bool operator!=(_In const IPv6& other) const
                {
                    return !(*this == other);
                }

                AddressType getType() const { return AddressType::IPv6; }

                /**
                 * Форматировать в буфер (сжатая форма RFC 5952,
                 * например "2001:db8::1").
                 *
                 * @return false — буфер nullptr или не вмещает строку
                 *         с нуль-терминатором
                 */
                bool toString(_Out char* buffer, _In buint32 bufferSize) const;

                /**
                 * Разобрать из строки (полная или сжатая форма, одно
                 * "::"). При неудаче объект сбрасывается к нулевому
                 * адресу (::).
                 */
                bool fromString(_In const char* str);
            };

            /**
             * IPv4-подсеть: адрес сети + длина префикса в битах (0-32).
             * Строка: "1.2.3.0/24".
             */
            class IPv4Subnet
            {
            public:
                IPv4 network;
                buint8 prefix = 0;

                bool operator==(_In const IPv4Subnet& other) const
                {
                    return this->network == other.network && this->prefix == other.prefix;
                }

                bool operator!=(_In const IPv4Subnet& other) const
                {
                    return !(*this == other);
                }

                AddressType getType() const { return AddressType::IPv4Subnet; }

                /**
                 * Проверка вхождения адреса в подсеть: старшие prefix
                 * бит адреса совпадают с адресом сети.
                 */
                bool isInSubnet(_In const IPv4& ip) const;

                /**
                 * Форматировать в буфер ("1.2.3.0/24").
                 *
                 * @return false — буфер nullptr или не вмещает строку
                 *         с нуль-терминатором
                 */
                bool toString(_Out char* buffer, _In buint32 bufferSize) const;

                /**
                 * Разобрать из строки ("1.2.3.0/24", префикс 0-32).
                 * При неудаче объект сбрасывается (нулевая сеть, prefix 0).
                 */
                bool fromString(_In const char* str);
            };

            /**
             * IPv6-подсеть: адрес сети + длина префикса в битах (0-128).
             * Строка: "2001:db8::/64".
             */
            class IPv6Subnet
            {
            public:
                IPv6 network;
                buint8 prefix = 0;

                bool operator==(_In const IPv6Subnet& other) const
                {
                    return this->network == other.network && this->prefix == other.prefix;
                }

                bool operator!=(_In const IPv6Subnet& other) const
                {
                    return !(*this == other);
                }

                AddressType getType() const { return AddressType::IPv6Subnet; }

                /**
                 * Проверка вхождения адреса в подсеть: старшие prefix
                 * бит адреса совпадают с адресом сети.
                 */
                bool isInSubnet(_In const IPv6& ip) const;

                /**
                 * Форматировать в буфер ("2001:db8::/64").
                 *
                 * @return false — буфер nullptr или не вмещает строку
                 *         с нуль-терминатором
                 */
                bool toString(_Out char* buffer, _In buint32 bufferSize) const;

                /**
                 * Разобрать из строки ("2001:db8::/64", префикс 0-128).
                 * При неудаче объект сбрасывается (нулевая сеть, prefix 0).
                 */
                bool fromString(_In const char* str);
            };

            /**
             * Address — универсальный вариант над конкретными типами
             * (Mac, IPv4, IPv6, IPv4Subnet, IPv6Subnet).
             *
             * Value-семантика: POD-union + тег, копирование тривиально.
             * Преобразование в конкретный тип — методы toMac()/toIPv4()/
             * toIPv6()/toIPv4Subnet()/toIPv6Subnet(); при несовпадении
             * тега — fatal (проверять getType() заранее).
             */
            class Address
            {
            public:
                /**
                 * Алиас типа внутрь класса (Address::Type::IPv4): сам
                 * enum объявлен в неймспейсе — см. комментарий над
                 * AddressType выше.
                 */
                using Type = AddressType;

                Address();
                explicit Address(_In const Mac& mac);
                explicit Address(_In const IPv4& ipv4);
                explicit Address(_In const IPv6& ipv6);
                explicit Address(_In const IPv4Subnet& subnet);
                explicit Address(_In const IPv6Subnet& subnet);

                /**
                 * Тег активного члена варианта.
                 */
                Type getType() const { return this->tag; }

                /**
                 * Преобразование к конкретному типу. Контракт: тег
                 * обязан совпадать (проверять через getType());
                 * при несовпадении — fatal.
                 */
                Mac toMac() const;
                IPv4 toIPv4() const;
                IPv6 toIPv6() const;
                IPv4Subnet toIPv4Subnet() const;
                IPv6Subnet toIPv6Subnet() const;

                bool operator==(_In const Address& other) const;
                bool operator!=(_In const Address& other) const
                {
                    return !(*this == other);
                }

                /**
                 * Форматировать в буфер (формат зависит от тега).
                 *
                 * @return false — буфер nullptr, не вмещает строку
                 *         или тег UNDEFINED (форматировать нечего)
                 */
                bool toString(_Out char* buffer, _In buint32 bufferSize) const;

                /**
                 * Разобрать из строки с автоматическим распознаванием:
                 *   - "ip/prefix" — подсеть (IPv4Subnet/IPv6Subnet);
                 *   - "aa:bb:cc:dd:ee:ff" — Mac;
                 *   - "2001:db8::1" — IPv6;
                 *   - "1.2.3.4" — IPv4.
                 * Распознавание: подсеть по '/', затем Mac, затем IPv6
                 * (обе формы содержат ':'), затем IPv4.
                 *
                 * @param ok если передан — получает true при успехе
                 * @return при ошибке — пустой адрес (UNDEFINED)
                 */
                static Address fromString(_In const char* str, _Out_opt bool* ok = nullptr);

                // Статические данные НЕ наследуют атрибут класса (MSVC) —
                // макрос данных __blib_data_api: dllimport у потребителей,
                // пусто в сборке blib-network (см. blib/config.h)
                static const __blib_data_api Address AnyIPv4;
                static const __blib_data_api Address NoneIPv4;
                static const __blib_data_api Address LocalhostIPv4;
                static const __blib_data_api Address BroadcastIPv4;
                static const __blib_data_api Address AnyIPv6;
                static const __blib_data_api Address LocalhostIPv6;

            private:
                Type tag;

                // Хранилище варианта: все члены — тривиальные POD.
                // NSDMI членов удаляют default-конструктор union'а
                // (C++17) — объявляем свой: zero-init через первый
                // член (value-initialization тривиального агрегата).
                // Активный член определяется тегом tag.
                union Storage
                {
                    Storage()
                        : mac()
                    {
                    }

                    Mac mac;
                    IPv4 ipv4;
                    IPv6 ipv6;
                    IPv4Subnet ipv4Subnet;
                    IPv6Subnet ipv6Subnet;
                } storage;
            };

            /**
             * TCP-эндпоинт: IP-адрес + порт (для bind/connect).
             * Строка: "1.2.3.4:8080" (IPv4) или "[2001:db8::1]:8080"
             * (IPv6 — только в скобках: у голого IPv6 последнее ':'
             * неоднозначно отделяет порт).
             */
            class Tcp
            {
            public:
                Address ip;      // UNDEFINED, пока не разобран/не задан
                buint16 port = 0;

                bool operator==(_In const Tcp& other) const
                {
                    return this->ip == other.ip && this->port == other.port;
                }

                bool operator!=(_In const Tcp& other) const
                {
                    return !(*this == other);
                }

                /**
                 * Тип адреса (тег ip).
                 */
                AddressType getType() const { return this->ip.getType(); }

                const Address& getIP() const { return this->ip; }
                buint16 getPort() const { return this->port; }
                void setPort(_In buint16 newPort) { this->port = newPort; }

                /**
                 * Форматировать в буфер ("1.2.3.4:8080" /
                 * "[2001:db8::1]:8080").
                 *
                 * @return false — буфер nullptr, не вмещает строку
                 *         или ip не является IP-адресом
                 */
                bool toString(_Out char* buffer, _In buint32 bufferSize) const;

                /**
                 * Разобрать из строки ("1.2.3.4:8080" или
                 * "[2001:db8::1]:8080"). При неудаче объект
                 * сбрасывается (UNDEFINED, порт 0).
                 */
                bool fromString(_In const char* str);
            };

            /**
             * UDP-эндпоинт: IP-адрес + порт (для bind/send/recv).
             * Строковый формат — как у Tcp.
             */
            class Udp
            {
            public:
                Address ip;      // UNDEFINED, пока не разобран/не задан
                buint16 port = 0;

                bool operator==(_In const Udp& other) const
                {
                    return this->ip == other.ip && this->port == other.port;
                }

                bool operator!=(_In const Udp& other) const
                {
                    return !(*this == other);
                }

                /**
                 * Тип адреса (тег ip).
                 */
                AddressType getType() const { return this->ip.getType(); }

                const Address& getIP() const { return this->ip; }
                buint16 getPort() const { return this->port; }
                void setPort(_In buint16 newPort) { this->port = newPort; }

                /**
                 * Форматировать в буфер ("1.2.3.4:8080" /
                 * "[2001:db8::1]:8080").
                 *
                 * @return false — буфер nullptr, не вмещает строку
                 *         или ip не является IP-адресом
                 */
                bool toString(_Out char* buffer, _In buint32 bufferSize) const;

                /**
                 * Разобрать из строки ("1.2.3.4:8080" или
                 * "[2001:db8::1]:8080"). При неудаче объект
                 * сбрасывается (UNDEFINED, порт 0).
                 */
                bool fromString(_In const char* str);
            };
        }
    }
}
