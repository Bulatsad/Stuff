#pragma once

#include <blib/network/socket.h>
#include <blib/network/address.h>

#include <blib/inline.h>

#include <WinSock2.h>

int blibToWinApi(const blib::network::SocketType type);
int blibToWinApi(const blib::network::SocketProtocol protocol);
int blibToWinApi(const blib::network::AddressType af);

blib::network::AddressType blibWinApiToBlib(ADDRESS_FAMILY af);

__blib_inline int blibToWinApi(const blib::network::SocketType type)
{
	switch (type)
	{
	case blib::network::SocketType::Stram:
		return SOCK_STREAM;
	case blib::network::SocketType::Dgram:
		return SOCK_DGRAM;
	case blib::network::SocketType::Raw:
		return SOCK_RAW;
	case blib::network::SocketType::RDM:
		return SOCK_RDM;
	case blib::network::SocketType::SeqPacket:
		return SOCK_SEQPACKET;
	default:
		return 0;
	}
}

__blib_inline int blibToWinApi(const blib::network::SocketProtocol protocol)
{
	switch (protocol)
	{
	case blib::network::SocketProtocol::ICMP:
		return IPPROTO_ICMP;
	case blib::network::SocketProtocol::IGMP:
		return IPPROTO_IGMP;
	case blib::network::SocketProtocol::RFCOMM:
		//return BTHPROTO_RFCOMM;
		return 0;
	case blib::network::SocketProtocol::TCP:
		return IPPROTO_TCP;
	case blib::network::SocketProtocol::UDP:
		return IPPROTO_UDP;
	case blib::network::SocketProtocol::ICMPv6:
		return IPPROTO_ICMPV6;
	default:
		return 0;
	}
}

__blib_inline int blibToWinApi(const blib::network::AddressType af)
{
	switch (af)
	{
	case blib::network::AddressType::IPv4:
		return AF_INET;
	case blib::network::AddressType::IPv6:
		return AF_INET6;
	case blib::network::AddressType::AppleTalk:
		return AF_APPLETALK;
	case blib::network::AddressType::NetBios:
		return AF_NETBIOS;
	case blib::network::AddressType::IRDA:
		return AF_IRDA;
	case blib::network::AddressType::Bluetooth:
		return AF_BTH;
	default:
		return AF_UNSPEC;
	}
}


