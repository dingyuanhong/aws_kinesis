#ifndef _EVO_AVFRAME_H__
#define _EVO_AVFRAME_H__

#include "EvoPacket.h"

class EvoAVFrame 
	: public EvoPacket
{
public:
	virtual ~EvoAVFrame();
	virtual bool Validate() override;
	virtual AVFrame* GetData();
	void Null();
	friend class EvoPacketAllocator;

	int64_t GetPTS();

	typedef enum
	{
		VIDEO_FRAME,
		AUDIO_FRAME,
	}FrameType;

	virtual std::string GetPacketTag();
	static std::string GetTag(int width, int height, AVPixelFormat format);
	static std::string GetTag(int channel, int sample_rate, AVSampleFormat);

private:
	EvoAVFrame();
	EvoAVFrame& operator=(const EvoAVFrame&);
	EvoAVFrame(const EvoAVFrame&);

	bool AllocFrame();

private:
	AVFrame* PacketData;
	
	uint32_t Width = 0;
	uint32_t Height = 0;
	AVPixelFormat VFormat;

	uint16_t Channel = 0;
	uint16_t SampleRate = 0;
	AVSampleFormat AFormat;

	FrameType Type;
};

#endif//_EVO_AVFRAME_H__