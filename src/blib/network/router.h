#pragma once

#include <vector>

#include <blib/blibint.h>
#include <blib/config.h>
#include <blib/utilmacro.h>

#include <blib/network/address.h>
#include <blib/system/memory/allocator.h>
#include <blib/system/memory/stdAllocatorAdapter.h>

namespace blib
{
    namespace network
    {
        // Максимальная длина имени интерфейса (без нуль-терминатора):
        // с запасом на длинные имена Windows ("Intel(R) Ethernet ...")
        constexpr buint32 interfaceNameMax = 64;

        // Лимит суммарного числа маршрутов (IPv4 + IPv6) в таблице —
        // защита от некорректного/раздутого ввода
        constexpr buint32 routerMaxRoutesTotal = 1024;

        /**
         * Формат входного текста таблицы маршрутизации (loadFromFile/
         * loadFromString).
         */
        enum class RouteTableFormat : buint8
        {
            Linux,   // вывод "ip route show" (default/via/dev/metric/src/onlink)
            Windows, // вывод "route print" (Interface List + IPv4/IPv6 Route Table)
            Json,    // custom JSON — TODO (схема не утверждена)

            END_OF_ENUM
        };

        /**
         * RouterError — ошибки загрузки таблицы маршрутизации
         * (enum-код модуля, паттерн проекта: None = 0 — успех).
         */
        enum class RouterError : buint32
        {
            None = 0,
            FileNotFound,  // loadFromFile: файл не открылся
            EmptyTable,    // документ не содержит ни одного маршрута
            InvalidData,   // превышен routerMaxRoutesTotal / повреждённый ввод
            NotSupported   // формат ещё не реализован (Json — TODO)
        };

        /**
         * Интерфейс, через который маршрут отправляет пакет.
         *
         * Value-семантика: POD, тривиальное копирование. Имя интерфейса
         * ("eth0", "Ethernet") — из форматов, где оно есть (linux dev,
         * windows Interface List); пустая строка — неизвестно.
         * localAddress — локальный адрес самого интерфейса (linux src,
         * windows колонка Interface); UNDEFINED — неизвестен.
         */
        struct RouteInterface
        {
            char name[interfaceNameMax] = {};
            address::Address localAddress = {};
        };

        /**
         * Одна строка таблицы маршрутизации.
         *
         * Value-семантика: POD, тривиальное копирование, ноль аллокаций.
         * destination — IPv4Subnet | IPv6Subnet; gateway — IPv4 | IPv6
         * (следующий хоп); UNDEFINED — on-link (сеть подключена напрямую).
         */
        struct Route
        {
            address::Address destination = {};
            address::Address gateway = {};
            RouteInterface iface = {};
            buint32 metric = 0;
        };

        /**
         * Результат поиска маршрута (Router::route): куда отправлять
         * пакет и через какой интерфейс.
         *
         * nextHop — шлюз маршрута либо (on-link) сам адресат: пакет
         * отправляется напрямую в сеть адресата.
         */
        struct RouteResult
        {
            address::Address nextHop = {};
            RouteInterface iface = {};
            buint32 metric = 0;
        };

        /**
         * Router — эмуляция таблицы маршрутизации хоста.
         *
         * Назначение:
         * - Загрузить таблицу маршрутов из текстовых форматов linux
         *   ("ip route show") или windows ("route print"); JSON — TODO;
         * - Эмулировать выбор маршрута: route(destination) возвращает
         *   следующий хоп (шлюз или сам адресат при on-link) и интерфейс,
         *   через который пакет отправляется.
         *
         * Выбор маршрута — longest-prefix-match по своей таблице
         * (IPv4-адрес ищется только в IPv4-таблице, IPv6 — в IPv6-таблице);
         * при равном префиксе побеждает меньшая метрика, при полном
         * равенстве — маршрут, добавленный первым (стабильный порядок).
         *
         * Семантика load: успешная загрузка ЗАМЕНЯЕТ текущие таблицы;
         * при ошибке прежние маршруты остаются нетронутыми.
         *
         * Память: контейнеры маршрутов — через member-Allocator
         * (DefaultAllocator → GlobalAllocator); копирование/перемещение
         * запрещены (векторы держат указатель на аллокатор-член).
         * Не thread-safe (как сокеты): один поток.
         *
         * См. NETWORK.md (модульный док).
         */
        class Router
        {
        public:
            Router();
            ~Router();

            Router(const Router&) = delete;
            Router(Router&&) = delete;
            Router& operator=(const Router&) = delete;
            Router& operator=(Router&&) = delete;

            /**
             * Загрузить таблицу маршрутизации из файла.
             *
             * @param path   Путь к файлу
             * @param format Формат содержимого (RouteTableFormat)
             * @return RouterError::None при успехе; прежние таблицы
             *         при ошибке не меняются
             */
            RouterError loadFromFile(_In const char* path, _In RouteTableFormat format);

            /**
             * Загрузить таблицу маршрутизации из строки (для тестов,
             * встроенных ресурсов и headless-конфигураций).
             *
             * @param text   Текст таблицы (нуль-терминированный)
             * @param format Формат содержимого (RouteTableFormat)
             * @return RouterError::None при успехе; прежние таблицы
             *         при ошибке не меняются
             */
            RouterError loadFromString(_In const char* text, _In RouteTableFormat format);

            /**
             * Найти маршрут для IPv4-адресата (longest-prefix-match).
             *
             * @param destination Адрес назначения пакета
             * @param out         Результат: nextHop + интерфейс + метрика
             * @return false — маршрута нет (нет подходящей записи или
             *         таблица пуста); out не изменяется
             */
            bool route(_In const address::IPv4& destination, _Out RouteResult& out) const;

            /**
             * Найти маршрут для IPv6-адресата (longest-prefix-match).
             *
             * @param destination Адрес назначения пакета
             * @param out         Результат: nextHop + интерфейс + метрика
             * @return false — маршрута нет (нет подходящей записи или
             *         таблица пуста); out не изменяется
             */
            bool route(_In const address::IPv6& destination, _Out RouteResult& out) const;

            /**
             * Диспетчер по типу адреса: IPv4 → IPv4-таблица,
             * IPv6 → IPv6-таблица; прочие теги (Mac, подсети,
             * UNDEFINED) — false.
             */
            bool route(_In const address::Address& destination, _Out RouteResult& out) const;

            /**
             * Суммарное число маршрутов в таблицах (IPv4 + IPv6).
             */
            buint32 getRouteCount() const;

            /**
             * Очистить обе таблицы.
             */
            void clear();

        private:
            // Загрузка из буфера фиксированного размера (единый путь
            // для файла и строки). Парсеры не требуют нуль-терминатора.
            RouterError loadFromBuffer(_In const char* data, _In buint32 size, _In RouteTableFormat format);

            // Аллокатор служебных контейнеров. Объявлен ПЕРЕД векторами:
            // они хранят указатель на него. (Канон: scene.h.)
            blib::memory::Allocator containerAllocator;

            // Таблицы маршрутов по семействам адресов
            std::vector<Route, blib::memory::StdAllocatorAdapter<Route>> ipv4Routes;
            std::vector<Route, blib::memory::StdAllocatorAdapter<Route>> ipv6Routes;
        };
    }
}
