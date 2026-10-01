#include <blib/network/impl/win/winNetworKUtil.h>
#include <blib/inline.h>

#include <WinSock2.h>

#include <cstring>

int blibToWinApi(const blib::network::SocketType type)
{
	switch (type)
	{
	case blib::network::SocketType::Stream:
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

int blibToWinApi(const blib::network::SocketProtocol protocol)
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

int blibToWinApi(const blib::network::address::AddressType af)
{
	switch (af)
	{
	case blib::network::address::AddressType::IPv4:
		return AF_INET;
	case blib::network::address::AddressType::IPv6:
		return AF_INET6;
	default:
		return AF_UNSPEC;
	}
}

blib::network::address::AddressType blibWinApiToBlib(ADDRESS_FAMILY af)
{
	switch (af)
	{
	case AF_INET:
		return blib::network::address::AddressType::IPv4;
	case AF_INET6:
		return blib::network::address::AddressType::IPv6;
	default:
		return blib::network::address::AddressType::UNDEFINED;
	}
}

bool blibToSockaddr(_In const blib::network::address::Address& ip, _In buint16 port,
	_Out sockaddr_in& out)
{
	memset(&out, 0, sizeof(out));

	if (ip.getType() != blib::network::address::AddressType::IPv4)
	{
		// IPv6 — TODO (см. NETWORK.md): сокеты пока только IPv4
		return false;
	}

	const blib::network::address::IPv4 ipv4 = ip.toIPv4();
	out.sin_family = AF_INET;
	out.sin_port = htons(port);
	// Байты IPv4 хранятся в сетевом порядке (bytes[0] — старший октет),
	// sin_addr — big-endian: копия байт-в-байт корректна
	memcpy(&out.sin_addr, ipv4.bytes, sizeof(ipv4.bytes));
	return true;
}

bool blibFromSockaddr(_In const sockaddr_in& in,
	_Out blib::network::address::Address& outIp, _Out buint16& outPort)
{
	outIp = blib::network::address::Address();
	outPort = 0;

	if (in.sin_family != AF_INET)
	{
		return false;
	}

	blib::network::address::IPv4 ipv4;
	memcpy(ipv4.bytes, &in.sin_addr, sizeof(ipv4.bytes));
	outIp = blib::network::address::Address(ipv4);
	outPort = ntohs(in.sin_port);
	return true;
}
