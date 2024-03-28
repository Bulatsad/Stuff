#pragma once

#include <blib/network/socket.h>
#include <blib/network/address.h>

int blibToWinApi(const blib::network::SocketType type);
int blibToWinApi(const blib::network::SocketProtocol protocol);
int blibToWinApi(const blib::network::AddressType af);
