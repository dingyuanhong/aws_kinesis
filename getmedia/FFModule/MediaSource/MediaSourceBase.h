#ifndef EvoMediaSourceBase_H
#define EvoMediaSourceBase_H

#include "../FFheader.h"
#include <string>

#define IO_SIZE_DEFAULT (1024*5)
#define MAX_CONTENT_COUNT 10

typedef int(*PIO_Read_Packet)(void *opaque, uint8_t *buf, int buf_size);

class MediaReciever {
public:
	virtual void onMediaData(AVPacket * packet) = 0;
};

typedef struct ConigureSourceBase
{
	int Inline;
	ConigureSourceBase()
	{
		Inline = 0;
	}
}ConigureSourceBase;

class EvoMediaSourceBase
{
public:
	EvoMediaSourceBase();
	~EvoMediaSourceBase();

	int Open(PIO_Read_Packet read_cb, void* opaque, int io_size, AVMediaType codecType = AVMEDIA_TYPE_UNKNOWN, ConigureSourceBase configure = {});
	//���ļ� 0:�ɹ� !0:ʧ��
	int Open(const char * file, AVMediaType codecType = AVMEDIA_TYPE_UNKNOWN, ConigureSourceBase configure = {});
	//�ر�
	void Close();
	//��ת(��λ:����) 0:�ɹ� !0:ʧ��
	virtual int Seek(int64_t millisecond);
	//��ȡ֡ 0:�ɹ� AVERROR_EOF:�ļ����� !0:ʧ��
	virtual int ReadFrame();
	virtual int ReadFrame(AVPacket * packet);
	int ReadAllFrame();
	//ʱ�� ��λ:����
	int GetDuration();

	AVMediaType GetType();
	AVStream *GetStream();
	AVCodecContext *GetContext();

	//��������
	int GetIndex(AVMediaType type);
	AVCodecContext *GetContext(AVMediaType type);

	unsigned int GetStreamCount();
	AVMediaType GetType(unsigned int index);
	AVStream *GetStream(unsigned int index);
	AVCodecContext *GetContext(unsigned int index);

	//���ûص�
	void SetCallback(MediaReciever * cb) { callback_ = cb; }
private:
	int OpenStream(AVMediaType codecType = AVMEDIA_TYPE_VIDEO);
	virtual AVCodec *find_decoder(AVStream *stream) { return NULL; }
	virtual void onData(AVPacket * packet) { if (callback_ != NULL)callback_->onMediaData(packet); };
protected:
	AVIOContext     * iocontent_;
	AVFormatContext * context_;
	AVPacket * packet_;
	enum AVMediaType codecType_;

	int index_;
	AVStream *stream_;
	AVCodecContext *codecContext_;

	AVCodecContext *codecContexts_[MAX_CONTENT_COUNT];
	MediaReciever *callback_;
};

#endif