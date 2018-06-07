#ifndef MEDIADECODER_H
#define MEDIADECODER_H

#include "../FFheader.h"
#include "MemManager/EvoPacket.h"
#include "MemManager/EvoQueue.hpp"
#include "MediaConvert.h"
#include "AudioDecoder.h"
#include "VideoDecoder.h"
#include "VideoDecoder.h"
#include "AudioDecoder.h"
#include "core/thread.h"

typedef int(*PIO_Read_Packet)(void *opaque, uint8_t *buf, int buf_size);

class MediaDecoder
{
public:
	enum DECODER_TYPE { VIDEO, AUDIO, STREAN };

	MediaDecoder(enum DECODER_TYPE type);
	~MediaDecoder();
	void Clear();

	void Init();
	int Open(PIO_Read_Packet read_cb, void* opaque, int io_size);
	int Open(const char* url);
	
	//pos:֡����
	bool SetStartPos(int64_t pos);
	//timePos:ʱ���,��λ����
	bool SetTimePos(int64_t timePos);
	
	enum DECODER_TYPE GetType() { return DecoderType; }
	AVStream * GetVideoStream();
	AVStream * GetAudioStream();
	//���ý��ʱ�� ms
	int64_t GetDuration();
	struct EvoAudioInfo GetAudioInfo() const;
	struct EvoVideoInfo GetVideoInfo() const;
	struct EvoVideoInfo GetDesVideoInfo() const;

	bool IsReadEnd();
	void ResetIsEnd();
	bool CheckIsEnd();
	bool CheckAudioAvailable();
	bool CheckVideoAvailable();
	bool CheckIsGetAudioSource();
	//������Ƶת��ģ��
	int SetConvert(const EvoVideoInfo &info);
	//��ȡ��Ƶ������
	VideoDecoder * GetVideoDecoder() {return VDecoder;}
	//��ȡ��Ƶ������
	AudioDecoder * GetAudioDecoder() { return ADecoder;}

	bool Decode();

	//�������ݰ���������
	int DecodeAudioPacket(AVPacket *packet);
	int DecodeVideoPacket(AVPacket *packet);

	void SetGetAudioSource(bool getAudioSource);
	//��ȡ��������
	EvoPacket *VideoDeQueue();
	EvoPacket *AudioDeQueue();
	AVPacket  *AudioSourceDeQueue();
	//��ȡ�������ݻ���������
	int GetVideoDataSetCount();
	int GetAudioDataSetCount();
	//�������еĻ�����������������
	void ClearAllQueue();
	//���û���������
	void ResetAllQueue();
	int64_t GetLastVideoPTS();
	void SetVideoDecoderName(const char * name);
private:
	void Close();
	void CloseBuffer();
	void InitDecoder(bool videoret, bool audioret);

	bool FindVideo();
	bool FindAudio();
	int  DecodeVideo();
	int  DecodeAudio();

	int DecodeAudioQueueData(AVPacket *&packet);
	int DecodeVideoQueueData(AVPacket *&packet);
	AVCodec *GetBestVideoDecoder(AVCodecID id);
private:
	DECODER_TYPE	DecoderType;

	AVIOContext     *avioCtx;
	AVFormatContext *FormatCtx;
	AVCodecContext	*AudioCodecCtx;
	AVCodecContext	*VideoCodecCtx;
	AVCodec			*AudioCodec;
	AVCodec			*VideoCodec;
	AVStream        *VideoStream;
	AVStream        *AudioStream;

	AVPacket		*MediaPacket;

	int				VideoIndex;
	int				AudioIndex;
	int				DecoderIndex;

	int64_t			StartPos;
	bool			IsEnd;
	std::mutex		TimePosMutex;

	int64_t			VideoFrameCount;
	int64_t			AudioFrameCount;
	int64_t			LastVideoPTS;

	VideoDecoder	*VDecoder;
	AudioDecoder	*ADecoder;
	MediaConvert	*Convert;
	struct EvoVideoInfo DesInfo;
	uv_rwlock_t		ConvertMutex;
	
	EvoQueue<EvoPacket> VideoDataSet;					//��Ƶ���
	EvoQueue<EvoPacket> AudioDataSet;					//��Ƶ���
	EvoQueue<AVPacket>	AudioDataSourceSet;				//��ƵԴ���
	bool IsGetAudioSource;

	std::string		VideoDecoderName;
};


#endif