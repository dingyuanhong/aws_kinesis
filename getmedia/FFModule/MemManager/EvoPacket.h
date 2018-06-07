#ifndef _EVO_PACKET_H__
#define _EVO_PACKET_H__

#include <stdint.h>
#include <string>

#include "../FFheader.h"

class EvoPacket {
public:
	virtual ~EvoPacket();
	typedef enum
	{
		TYPE_AVFRAME,
		TYPE_MEMORY,
		TYPE_ATTRIBUTE_MEMORY
	}TypeEnum;
	//判断数据是否为空，如果为空，从新申请
	virtual bool Validate() = 0;
	virtual void Null() = 0;
	virtual std::string GetPacketTag();
	TypeEnum GetPacketType();
	uint32_t GetPacketSize();

protected:
	EvoPacket();

	EvoPacket& operator= (const EvoPacket&);
	EvoPacket(const EvoPacket&);

	friend class EvoPacketAllocator;

protected:
	TypeEnum PacketType;
	uint32_t DataSize = 0;
};

#endif//_EVO_PACKET_H__
