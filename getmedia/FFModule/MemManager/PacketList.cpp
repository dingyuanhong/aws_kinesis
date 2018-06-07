#include "PacketList.h"

PacketList::PacketList(uint32_t dataSize, EvoPacket::TypeEnum type, uint32_t maxLen)
	: DataSize(dataSize)
	, PacketType(type)
	, ListMaxLen(maxLen)
{
}

PacketList::~PacketList()
{
	std::list<EvoPacket*>::iterator iter;
	while (!PktList.empty())
	{
		iter = PktList.begin();
		delete *iter;
		PktList.pop_front();
	}
}

uint32_t PacketList::GetDataSize()
{
	return DataSize;
}

EvoPacket::TypeEnum PacketList::GetPacketType()
{
	return PacketType;
}

void PacketList::PutPacket(EvoPacket* packet)
{
	if (PktList.size() >= ListMaxLen)
	{
		delete packet;
		return;
	}

	++ListCurLen;
	PktList.push_back(packet);
}

EvoPacket* PacketList::GetPacket()
{
	if (PktList.empty())
		return nullptr;

	--ListCurLen;
	auto iter = PktList.front();
	PktList.pop_front();
	return iter;
}