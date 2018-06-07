#ifndef _EVO_MEMORY_H__
#define _EVO_MEMORY_H__

#include "EvoPacket.h"

class EvoMemory 
	: public EvoPacket
{
public:
	virtual ~EvoMemory();
	virtual bool Validate() override;
	virtual uint8_t* GetData();
	void Null();
	friend class EvoPacketAllocator;
	friend class EvoAttributeMemory;

	virtual std::string GetPacketTag();
	static std::string GetTag(int size);
private:
	EvoMemory();
	EvoMemory& operator= (const EvoMemory&);
	EvoMemory(const EvoMemory&);

private:
	uint8_t* PacketData = nullptr;
};

#endif//_EVO_MEMORY_H__