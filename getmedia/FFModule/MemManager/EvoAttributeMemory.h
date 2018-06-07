#pragma once
#include "EvoMemory.h"

class EvoAttributeMemory :
	public EvoMemory
{
public:
	virtual ~EvoAttributeMemory();

	void SetTimeamp(const int64_t value);
	void SetPTS(const int64_t value);
	void SetDTS(const int64_t value);
	void SetDuration(const int64_t value);
	void SetTime(const int64_t value);
	void SetSeq(const int32_t value);
	void SetFrame(const int32_t value);

	int64_t GetTimeamp();
	int64_t GetPTS();
	int64_t GetDTS();
	int64_t GetDuration();
	int64_t GetTime();
	int32_t GetSeq();
	int32_t GetFrame();

	friend class EvoPacketAllocator;
private:
	EvoAttributeMemory();
	EvoAttributeMemory& operator= (const EvoAttributeMemory&);
	EvoAttributeMemory(const EvoAttributeMemory&);

private:
	int64_t FPTS;				//PTS
	int64_t FDTS;				//DTS
	int64_t FTimeamp;			//ʱ���
	int64_t FDuration;			//֡����ʱ��
	int64_t FTime;				//ʱ��
	int32_t FSeq;				//֡���к�
	int32_t FFrame;				//֡��ʶ
};

