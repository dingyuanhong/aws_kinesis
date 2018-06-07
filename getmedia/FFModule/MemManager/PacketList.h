#ifndef _PACKET_LIST_H__
#define _PACKET_LIST_H__

#include <list>
#include <string.h>

#include "EvoPacket.h"

class PacketList {

	friend class PacketPool;

private:
	PacketList(uint32_t dataSize, EvoPacket::TypeEnum type, uint32_t maxLen);
	~PacketList();
	uint32_t GetDataSize();
	EvoPacket::TypeEnum GetPacketType();
	void PutPacket(EvoPacket* packet);
	EvoPacket* GetPacket();

private:
	std::list<EvoPacket*> PktList;
	uint32_t DataSize = 0;
	EvoPacket::TypeEnum PacketType;
	uint32_t ListMaxLen = 0;
	uint32_t ListCurLen = 0;
};

#endif//_PACKET_LIST_H__