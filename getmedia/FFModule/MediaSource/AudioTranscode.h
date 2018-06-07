#pragma once

#include "../FFheader.h"

//ԭʼ���ݸ�ʽ
typedef struct SourceParams {
	int channels;					//ͨ��
	int sample_rate;				//������
	enum AVSampleFormat sample_fmt;	//����λ��
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

