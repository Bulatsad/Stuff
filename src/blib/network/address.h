#pragma once

#include <blib/blibint.h>
#include <blib/config.h>

namespace blib
{
    namespace network
    {
        enum class AddressType
        {
            IPv4,
            IPv6,
            AppleTalk,
            NetBios,
            IRDA,
            Bluetooth,

            UNDEFINED,

            END_OF_ENUM
        }; 

        /**
         * Address — IP-адрес + порт (value-тип: копирование глубокое,
         * каждая копия владеет своим платформенным хендлом).
         */
        class Address
        {
        public:
            /**
             * Разобрать IPv4-строку ("127.0.0.1"). При ошибке
             * @param ok (если передан) получает false, возвращается
             * пустой адрес (ANY, порт 0)
             */
            static Address fromIPv4(const char* str, bool* ok = nullptr);

            Address();
            Address(AddressType _type);

            Address(const Address& other);
            Address& operator=(const Address& other);
            Address(Address&& other) noexcept;
            Address& operator=(Address&& other) noexcept;
            ~Address();

            /**
             * Порт (переводится в сетевой порядок байт внутри).
             */
            void setPort(int port);

            AddressType getType() const;
            void* __getHandler();

            // Статические данные НЕ наследуют атрибут класса (MSVC) —
            // макрос данных __blib_data_api: dllimport у потребителей,
            // пусто в сборке blib-network (см. blib/config.h)
            static const __blib_data_api Address AnyIPv4;
            static const __blib_data_api Address NoneIPv4;
            static const __blib_data_api Address LocalhostIPv4;
            static const __blib_data_api Address BroadcastIPv4;
        private:
            void* ctx;
        };
    }
}
