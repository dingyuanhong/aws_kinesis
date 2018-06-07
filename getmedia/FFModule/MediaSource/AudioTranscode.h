#pragma once

#include "../FFheader.h"

//原始数据格式
typedef struct SourceParams {
	int channels;					//通道
	int sample_rate;				//采样率
	enum AVSampleFormat sample_fmt;	//数据位数
}SourceParams;

class AudioTranscode
{
public:
	AudioTranscode();
	~AudioTranscode();
	int Init(const SourceParams &srcParams);
	
	int Transcode(AVFrame *frame, AVPacket *packet);

	int ADTS(AVPacket *src, AVPacket **des);

	int GetSampleSize();
	void Close();
private:
	int InitSWR(const SourceParams &srcParams);
	void WriteADTSHeader(int Size, int sample_rate, int channels);
private:
	AVCodecContext*  CodecCtx;
	SwrContext * Swr_ctx;
	AVFrame* DesFrame;
	char * ADTSHeader;
};

