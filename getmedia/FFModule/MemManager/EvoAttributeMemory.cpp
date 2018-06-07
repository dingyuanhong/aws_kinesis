#include "EvoAttributeMemory.h"

EvoAttributeMemory::EvoAttributeMemory()
	:FPTS(0), FDTS(0),FTimeamp(0)
	,FTime(0),FSeq(0),FFrame(0)
{
}

EvoAttributeMemory& EvoAttributeMemory::operator= (const EvoAttributeMemory&)
{
	return *this;
}

EvoAttributeMemory::EvoAttributeMemory(const EvoAttributeMemory&)
{
}

EvoAttributeMemory::~EvoAttributeMemory()
{
}

void EvoAttributeMemory::SetTimeamp(const int64_t value)
{
	FTimeamp = value;
}
void EvoAttributeMemory::SetPTS(const int64_t value)
{
	FPTS = value;
}

void EvoAttributeMemory::SetDTS(const int64_t value)
{
	FDTS = value;
}
void EvoAttributeMemory::SetDuration(const int64_t value)
{
	FDuration = value;
}

void EvoAttributeMemory::SetTime(const int64_t value)
{
	FTime = value;
}

void EvoAttributeMemory::SetSeq(const int32_t value)
{
	FSeq = value;
}
void EvoAttributeMemory::SetFrame(const int32_t value)
{
	FFrame = value;
}

int64_t EvoAttributeMemory::GetTimeamp()
{
	return FTimeamp;
}

int64_t EvoAttributeMemory::GetPTS()
{
	return FPTS;
}
int64_t EvoAttributeMemory::GetDTS()
{
	return FDTS;
}

int64_t EvoAttributeMemory::GetDuration()
{
	return FDuration;
}

int64_t EvoAttributeMemory::GetTime()
{
	return FTime;
}

int32_t EvoAttributeMemory::GetSeq()
{
	return FSeq;
}

int32_t EvoAttributeMemory::GetFrame()
{
	return FFrame;
}