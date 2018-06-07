#include "PacketPool.h"
#include <string>

PacketPool::PacketPool()
{

}

PacketPool::~PacketPool()
{

}

PacketPool* PacketPool::Instance = nullptr;
uint32_t PacketPool::MapSize = 0;

PacketPool* PacketPool::GetInstance(uint32_t mapSize)
{
	if (!Instance)
	{
		Instance = new PacketPool();
		MapSize = mapSize;
	}
	return Instance;
}

void PacketPool::DeleteInstance()
{
	if (Instance->ListMap.empty())
		return;

	std::lock_guard<std::mutex> lock(Instance->PoolMutex);
	while (!Instance->ListMap.empty())
	{
		std::map<std::string, PacketList*>::iterator iter = Instance->ListMap.begin();
		delete iter->second;
		Instance->ListMap.erase(iter);
		--Instance->MapSize;
	}
}

void PacketPool::PutPacket(EvoPacket** packet)
{
	std::string tag = (*packet)->GetPacketTag();

	std::lock_guard<std::mutex> lock(PoolMutex);

	std::map<std::string, PacketList*>::iterator iter = ListMap.find(tag);
	PacketList* packetList = nullptr;
	if (iter == ListMap.end())
	{
		if (ListMap.size() >= MapSize)
		{
			delete *packet;
			*packet = nullptr;
			return;
		}
		packetList = new PacketList((*packet)->GetPacketSize(), (*packet)->GetPacketType(), PACKET_LIST_MAX_SIZE);
		if (!packetList)
		{
			delete *packet;
			*packet = nullptr;
			return;
		}
		ListMap.insert(make_pair(tag, packetList));
	}
	else
		packetList = iter->second;
	
	packetList->PutPacket((*packet));
	*packet = nullptr;
}

EvoPacket* PacketPool::GetPacket(std::string tag, EvoPacket::TypeEnum type, int dataSize)
{
	std::lock_guard<std::mutex> lock(PoolMutex);
	std::map<std::string, PacketList*>::iterator iter = ListMap.find(tag);
	if (iter == ListMap.end())
	{
		if (ListMap.size() >= MapSize)
			return nullptr;
		PacketList* packetList = new PacketList(dataSize, type, PACKET_LIST_MAX_SIZE);
		if (!packetList)
			return nullptr;
		ListMap.insert(make_pair(tag, packetList));
		return nullptr;
	}

	return iter->second->GetPacket();
}

PacketList* PacketPool::GetPacketList(std::string tag, int size, EvoPacket::TypeEnum type)
{
	std::lock_guard<std::mutex> lock(PoolMutex);
	std::map<std::string, PacketList*>::iterator iter = ListMap.find(tag);
	PacketList* packetList = nullptr;
	if (iter == ListMap.end())
		packetList = new PacketList(size, type, PACKET_LIST_MAX_SIZE);
	else
		packetList = iter->second;

	return packetList;
}