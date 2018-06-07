#include "EvoAVFrame.h"

#ifndef WIN32
#define sprintf_s snprintf
#endif

EvoAVFrame::EvoAVFrame()
	: EvoPacket()
{
	PacketData = NULL;
}

EvoAVFrame& EvoAVFrame::operator=(const EvoAVFrame&)
{
	return *this;
}

EvoAVFrame::EvoAVFrame(const EvoAVFrame&)
{

}

EvoAVFrame::~EvoAVFrame()
{
	if (PacketData && PacketData->data[0])
		av_freep(&PacketData->data[0]);
	if (PacketData)
		av_frame_free(&PacketData);
	PacketData = NULL;
}

bool EvoAVFrame::Validate()
{
	if (PacketData)
		return true;
	else
	{
		return AllocFrame();
	}
	return true;
}

AVFrame* EvoAVFrame::GetData()
{
	return PacketData;
}

void EvoAVFrame::Null()
{
	if(PacketData != NULL)
	{
		av_frame_free(&PacketData);
	}
}

bool EvoAVFrame::AllocFrame()
{
	if (Type == VIDEO_FRAME) {
		if (!Width || !Height || VFormat == AV_PIX_FMT_NONE)
			return false;

		PacketData = av_frame_alloc();
		if (!PacketData)
			goto _FAILED_;
		
		DataSize = av_image_alloc(PacketData->data, PacketData->linesize, Width, Height, VFormat, 1);
	}

	if (Type == AUDIO_FRAME) {
		if (!Channel || !SampleRate || AFormat == AV_SAMPLE_FMT_NONE)
			return false;

		PacketData = av_frame_alloc();
		if (!PacketData)
			goto _FAILED_;

		DataSize = av_samples_alloc(PacketData->data, PacketData->linesize, Channel, SampleRate, AFormat, 1);
		if (DataSize < 0)
			goto _FAILED_;
	}
	return true;

_FAILED_:
	if (PacketData && PacketData->data[0])
		av_freep(&PacketData->data[0]);
	if (PacketData)
		av_frame_free(&PacketData);

	return false;
}

int64_t EvoAVFrame::GetPTS()
{
	if (PacketData == NULL) return 0;
	return PacketData->pts;
}

std::string EvoAVFrame::GetTag(int width, int height, AVPixelFormat format)
{
	char str[1024] = { 0 };
	sprintf_s(str, 1024, "%d_V_W=%d_H=%d_F=%d", TYPE_AVFRAME, width, height, (int)format);
	return str;
}

std::string EvoAVFrame::GetTag(int channel, int sample_rate, AVSampleFormat format)
{
	char str[1024] = { 0 };
	sprintf_s(str, 1024, "%d_A_C=%d_S=%d_F=%d", TYPE_AVFRAME, channel, sample_rate, (int)format);
	return str;
}

std::string EvoAVFrame::GetPacketTag()
{
	return Type == VIDEO_FRAME ? GetTag(Width, Height, VFormat) : GetTag(Channel, SampleRate, AFormat);
}