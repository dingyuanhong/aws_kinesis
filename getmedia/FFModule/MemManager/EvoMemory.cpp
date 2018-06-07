#include "EvoMemory.h"

#ifndef WIN32
#define sprintf_s snprintf
#endif

EvoMemory::EvoMemory()
{

}

EvoMemory& EvoMemory::operator=(const EvoMemory&)
{
	return *this;
}

EvoMemory::EvoMemory(const EvoMemory&)
{

}

EvoMemory::~EvoMemory()
{
	if (PacketData)
		delete[] PacketData;
}

bool EvoMemory::Validate()
{
	if (PacketData && DataSize != 0)
		return true;
	else
	{
		if (DataSize == 0)
			return false;
		PacketData = new uint8_t[DataSize];
		memset(PacketData, 0, DataSize);
	}
	return true;
}

uint8_t* EvoMemory::GetData()
{
	return PacketData;
}

void EvoMemory::Null()
{
	if (PacketData != NULL)
	{
		delete []PacketData;
	}
	PacketData = NULL;
}

std::string EvoMemory::GetTag(int size)
{
	char str[1024] = { 0 };
	sprintf_s(str, 1024, "%d_%d", TYPE_MEMORY, size);
	return str;
}

std::string EvoMemory::GetPacketTag()
{
	return GetTag(DataSize);
}