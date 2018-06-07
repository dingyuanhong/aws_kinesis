#ifndef MEDIACONVERT_H
#define MEDIACONVERT_H

#include "../FFheader.h"
#include "MemManager/EvoPacket.h"

typedef struct EvoVideoInfo
{
	int Width;
	int Height;
	AVPixelFormat Format;
}EvoVideoInfo;

//#define USE_NEW_DECODER
//#define USE_NEW_ALLOC

#ifndef USE_NEW_ALLOC

inline AVPacket *av_packet_alloc() {
	AVPacket * packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	if(packet != NULL) av_init_packet(packet);
	return packet;
}

inline void av_packet_free(AVPacket** p) {
	if (p == NULL) return;
	if(*p != NULL) av_free_packet(*p);
	*p = NULL;
}
#endif

inline void AVPacketDelete(void **p)
{
	if (p == NULL) return;
	av_packet_free((AVPacket**)p);
}

class MediaConvert
{
public:
    MediaConvert();
    ~MediaConvert();
	bool CheckSrc(const EvoVideoInfo &info);
	bool CheckDes(const EvoVideoInfo &info);
	EvoVideoInfo GetDesInfo();
	EvoVideoInfo GetSrcInfo();
	EvoVideoInfo GetDesInfo(const EvoVideoInfo &info);
	EvoVideoInfo GetSrcInfo(const EvoVideoInfo &info);

    bool Initialize(const EvoVideoInfo &src, const EvoVideoInfo &des);
	int Convert(const AVFrame* srcFrame, AVFrame* desFrame);
public:
	static unsigned int GetSize(const EvoVideoInfo &info);
	static EvoVideoInfo GetInfo(const AVFrame* frame);

	static void ConvertToYUV(const AVFrame* srcFrame, const EvoVideoInfo &info, uint8_t* dstBuf);
	//×ª»»×Ö½ÚÐò
	static void ChangeEndianPic(unsigned char *image, int w, int h, int bpp);
	static void ChangeEndian32(unsigned char *data);
	static void ChangeEndian24(unsigned char *data);
private:
	bool Check(const EvoVideoInfo &info);
	bool Init(const EvoVideoInfo &info);

	int Convert(const AVFrame* srcFrame, uint8_t* dstBuf);
	int Convert(unsigned char *srcBuf, unsigned char *dstBuf, bool doRotate);
private:
    AVFrame * SrcFrame;
    AVFrame * DstFrame;

	EvoVideoInfo swsInfo;
    struct SwsContext *SwsCtx;

	EvoVideoInfo srcInfo;
	EvoVideoInfo desInfo;
};

#endif