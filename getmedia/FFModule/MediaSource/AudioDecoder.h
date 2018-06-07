#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include "../FFheader.h"
#include "MediaConvert.h"
#include "MemManager/EvoPacket.h"
#include "MemManager/EvoQueue.hpp"

typedef struct EvoAudioInfo {
	int SampleRate;			//������
	int Channels;			//������
	int SampleSize;			//������С
	int64_t BitSize;			//�ֽڴ�С
}EvoAudioInfo;

class AudioDecoder
{
public:
	AudioDecoder(AVCodecContext	*codec);
	~AudioDecoder();
	void Flush();
	int DecodePacket(AVPacket *packet, EvoPacket **evoResult);
private:
	EvoPacket* Decode(AVPacket *packet);

	int AudioResampling(AVFrame * audioDecodeFrame,uint8_t * audioBuffer, int bufferLen);
private:
	AVCodecContext	*AudioCodecCtx;
	struct SwrContext *AudioConvertCtx;

	AVFrame         *AudioFrame;

};


#endif