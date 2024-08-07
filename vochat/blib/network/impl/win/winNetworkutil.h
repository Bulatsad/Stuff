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
int blibToWinApi(const blib::network::AddressType af);

blib::network::AddressType blibWinApiToBlib(ADDRESS_FAMILY af);
