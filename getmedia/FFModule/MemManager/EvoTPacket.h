#ifndef EVOTPACKET_H
#define EVOTPACKET_H

#include <stdint.h>

class EvoTPacket {
public:
	EvoTPacket(unsigned long size);
	virtual ~EvoTPacket();

	virtual bool Validate();
	uint32_t GetSize();
	uint8_t *GetData();
	void Null();
public:
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
private:
	EvoTPacket(const EvoTPacket&) = delete;
	EvoTPacket& operator= (const EvoTPacket&) = delete;
private:
	int64_t FPTS;				//PTS
	int64_t FDTS;				//DTS
	int64_t FTimeamp;			//时间戳
	int64_t FDuration;			//帧持续时间
	int64_t FTime;				//时间
	int32_t FSeq;				//帧序列号
	int32_t FFrame;				//帧标识

	uint8_t* Data;
	uint32_t DataSize;
};


#endif