#include <blib/network/impl/win/winNetworKUtil.h>
#include <blib/inline.h>

#include <WinSock2.h>


blib::network::AddressType blibWinApiToBlib(ADDRESS_FAMILY af)
{
	switch (af)
	{
	case AF_INET:
		return blib::network::AddressType::IPv4;
	case AF_INET6:
		return blib::network::AddressType::IPv6;
	case AF_APPLETALK:
		return blib::network::AddressType::AppleTalk;
	case AF_NETBIOS:
		return blib::network::AddressType::NetBios;
	case AF_IRDA:
		return blib::network::AddressType::IRDA;
	case AF_BTH:
		return blib::network::AddressType::Bluetooth;
	default:
		return blib::network::AddressType::UNDEFINED;
	}
}
