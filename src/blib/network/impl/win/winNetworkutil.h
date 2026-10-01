#pragma once

#include <blib/network/socket.h>
#include <blib/network/address.h>

#include <blib/inline.h>

#include <WinSock2.h>

typedef SOCKET platform_socket_handler_t;
typedef sockaddr platform_socket_address_handler_t;
typedef sockaddr_in platform_socket_internet_address_handler_t;

#define __blib_cast_socket_handler(handler) (reinterpret_cast<platform_socket_handler_t*>(handler))
#define __blib_cast_address_handler(handler) (reinterpret_cast<platform_socket_address_handler_t*>(handler))
#define __blib_cast_internet_address_handler(handler) (reinterpret_cast<platform_socket_internet_address_handler_t*>(handler))

int blibToWinApi(const blib::network::SocketType type);
int blibToWinApi(const blib::network::SocketProtocol protocol);
int blibToWinApi(const blib::network::address::AddressType af);

blib::network::address::AddressType blibWinApiToBlib(ADDRESS_FAMILY af);

/**
 * Конвертация IP-адреса + порта в sockaddr_in (только IPv4 —
 * контракт Socket::bind/connect; IPv6 — TODO, см. NETWORK.md).
 * Порт переводится в сетевой порядок байт (htons) внутри.
 *
 * @return false — адрес не является IPv4
 */
bool blibToSockaddr(_In const blib::network::address::Address& ip, _In buint16 port,
    _Out sockaddr_in& out);

/**
 * Конвертация sockaddr_in в IP-адрес + порт (порт из сетевого
 * порядка байт, ntohs внутри).
 *
 * @return false — семейство не IPv4 (out не изменяется частично:
 *         адрес сбрасывается к UNDEFINED, порт к 0)
 */
bool blibFromSockaddr(_In const sockaddr_in& in,
    _Out blib::network::address::Address& outIp, _Out buint16& outPort);
