#include "EvoTPacket.h"
#include <memory>

EvoTPacket::EvoTPacket(unsigned long size)
	:FPTS(0), FDTS(0), FTimeamp(0)
	, FTime(0), FSeq(0), FFrame(0)
	, Data(NULL), DataSize(size)
{
	Data = new uint8_t[DataSize];
	memset(Data, 0, DataSize);
}

EvoTPacket::~EvoTPacket()
{
	if(Data != NULL)
		delete[]Data;
	Data = NULL;
	DataSize = 0;
}

bool EvoTPacket::Validate()
{
	if (Data && DataSize != 0)
		return true;
	else
	{
		if (DataSize == 0)
			return false;
		Data = new uint8_t[DataSize];
		memset(Data, 0, DataSize);
	}
	return true;
}

uint32_t EvoTPacket::GetSize()
{
	return DataSize;
}

uint8_t *EvoTPacket::GetData()
{
	return Data;
}

void EvoTPacket::Null()
{
	Data = NULL;
}

void EvoTPacket::SetTimeamp(const int64_t value)
{
	FTimeamp = value;
}

void EvoTPacket::SetPTS(const int64_t value)
{
	FPTS = value;
}

void EvoTPacket::SetDTS(const int64_t value)
{
	FDTS = value;
}

void EvoTPacket::SetDuration(const int64_t value)
{
	FDuration = value;
}

void EvoTPacket::SetTime(const int64_t value)
{
	FTime = value;
}

void EvoTPacket::SetSeq(const int32_t value)
{
	FSeq = value;
}

void EvoTPacket::SetFrame(const int32_t value)
{
	FFrame = value;
}

int64_t EvoTPacket::GetTimeamp()
{
	return FTimeamp;
}

int64_t EvoTPacket::GetPTS()
{
	return FPTS;
}

int64_t EvoTPacket::GetDTS()
{
	return FDTS;
}

int64_t EvoTPacket::GetDuration()
{
	return FDuration;
}

int64_t EvoTPacket::GetTime()
{
	return FTime;
}

int32_t EvoTPacket::GetSeq()
{
	return FSeq;
}

int32_t EvoTPacket::GetFrame()
{
	return FFrame;
}