#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "../FFheader.h"
#include "MemManager/EvoPacket.h"
#include "MemManager/EvoQueue.hpp"
#include "MediaConvert.h"

class VideoDecoder
{
public:
	VideoDecoder(AVCodecContext	*codec);
	~VideoDecoder();

	void Attach(MediaConvert *convert);
	MediaConvert *Detach();

	int DecodePacket(AVPacket *packet, EvoPacket **evoResult);
	void Flush();
	void SetKeepIFrame(bool keep)
	{
		KeepIFrame = keep;
	}
private:
	int ConvertFrame(EvoPacket **evoResult);
	int CloneFrame(EvoPacket **evoResult);
private:
	AVCodecContext	*VideoCodecCtx;
	AVFrame         *VideoFrame;
	bool			KeepIFrame;

	MediaConvert	*Convert;
};


#endif