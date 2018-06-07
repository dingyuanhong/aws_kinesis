#ifndef _PACKET_POOL_H__
#define _PACKET_POOL_H__

#include <map>
#include <mutex>

#include "PacketList.h"

#define PACKET_LIST_MAX_SIZE 100

class PacketPool {
public:
	static PacketPool* GetInstance(uint32_t mapSize);
	static void DeleteInstance();
	EvoPacket* GetPacket(std::string tag, EvoPacket::TypeEnum type, int dataSize);
	void PutPacket(EvoPacket** packet);

private:
	PacketPool();
	~PacketPool();
	std::map<std::string, PacketList*> ListMap;
	static uint32_t MapSize;
	std::mutex PoolMutex;
	PacketList* GetPacketList(std::string tag, int size, EvoPacket::TypeEnum type);

	static PacketPool* Instance;
};

#endif//_PACKET_POOL_H__