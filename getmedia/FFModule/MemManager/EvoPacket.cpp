#include "EvoPacket.h"

#include "EvoAVFrame.h"

EvoPacket::EvoPacket()
{
	
}

EvoPacket::~EvoPacket()
{

}

EvoPacket& EvoPacket::operator=(const EvoPacket&)
{
	return *this;
}

EvoPacket::EvoPacket(const EvoPacket&)
{

}

std::string EvoPacket::GetPacketTag()
{
	return "error";
}

EvoPacket::TypeEnum EvoPacket::GetPacketType()
{
	return PacketType;
}

uint32_t EvoPacket::GetPacketSize()
{
	return DataSize;
}