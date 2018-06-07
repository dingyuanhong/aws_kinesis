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
	//打开文件 0:成功 !0:失败
	int Open(const char * file, AVMediaType codecType = AVMEDIA_TYPE_UNKNOWN, ConigureSourceBase configure = {});
	//关闭
	void Close();
	//跳转(单位:毫秒) 0:成功 !0:失败
	virtual int Seek(int64_t millisecond);
	//读取帧 0:成功 AVERROR_EOF:文件结束 !0:失败
	virtual int ReadFrame();
	virtual int ReadFrame(AVPacket * packet);
	int ReadAllFrame();
	//时长 单位:毫秒
	int GetDuration();

	AVMediaType GetType();
	AVStream *GetStream();
	AVCodecContext *GetContext();

	//数据类型
	int GetIndex(AVMediaType type);
	AVCodecContext *GetContext(AVMediaType type);

	unsigned int GetStreamCount();
	AVMediaType GetType(unsigned int index);
	AVStream *GetStream(unsigned int index);
	AVCodecContext *GetContext(unsigned int index);

	//设置回调
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