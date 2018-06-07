#include "MediaConvert.h"
#include "MemManager/EvoMemory.h"
#include "core/log.h"

unsigned int MediaConvert::GetSize(const EvoVideoInfo &info)
{
	unsigned int dstSize = av_image_get_buffer_size(info.Format,
		info.Width,
		info.Height,
		1);
	//unsigned int dstSize = avpicture_get_size(
	//	info.Format,
	//	info.Width,
	//	info.Height);
	return dstSize;
}

void MediaConvert::ConvertToYUV(const AVFrame* srcFrame, const EvoVideoInfo &info, uint8_t* dstBuf)
{
	ASSERT(info.Format == AV_PIX_FMT_YUV420P);

	int height_half = info.Height / 2, width_half = info.Width / 2;
	int y_wrap = srcFrame->linesize[0];
	int u_wrap = srcFrame->linesize[1];
	int v_wrap = srcFrame->linesize[2];

	uint8_t* y_buf = srcFrame->data[0];
	uint8_t* u_buf = srcFrame->data[1];
	uint8_t* v_buf = srcFrame->data[2];

	uint8_t* temp = dstBuf;
	memcpy(temp, y_buf, info.Width * info.Height);
	temp += info.Width * info.Height;
	memcpy(temp, u_buf, height_half * width_half);
	temp += height_half * width_half;
	memcpy(temp, v_buf, height_half * width_half);
}

void MediaConvert::ChangeEndianPic(unsigned char *image, int w, int h, int bpp)
{
	unsigned char *pixeldata = NULL;
	for (int i = 0; i<h; i++)
		for (int j = 0; j<w; j++) {
			pixeldata = image + (i*w + j)*bpp / 8;
			if (bpp == 32) {
				ChangeEndian32(pixeldata);
			}
			else if (bpp == 24) {
				ChangeEndian24(pixeldata);
			}
		}
}

//change endian of a pixel (32bit)  
void MediaConvert::ChangeEndian32(unsigned char *data)
{
	char temp3, temp2;
	temp3 = data[3];
	temp2 = data[2];
	data[3] = data[0];
	data[2] = data[1];
	data[0] = temp3;
	data[1] = temp2;
}

void MediaConvert::ChangeEndian24(unsigned char *data)
{
	char temp2 = data[2];
	data[2] = data[0];
	data[0] = temp2;
}

EvoVideoInfo MediaConvert::GetInfo(const AVFrame* frame)
{
	EvoVideoInfo info;
	info.Format = (AVPixelFormat)frame->format;
	info.Width = frame->width;
	info.Height = frame->height;
	return info;
}

MediaConvert::MediaConvert()
	:SwsCtx(NULL), SrcFrame(NULL), DstFrame(NULL)
{
	srcInfo.Width = 0;
	srcInfo.Height = 0;
	srcInfo.Format = AV_PIX_FMT_NONE;
	desInfo.Width = 0;
	desInfo.Height = 0;
	desInfo.Format = AV_PIX_FMT_NONE;

	swsInfo.Width = 0;
	swsInfo.Height = 0;
	swsInfo.Format = AV_PIX_FMT_NONE;
}

MediaConvert::~MediaConvert()
{
    if (NULL != this->SrcFrame){
        av_frame_free(&(this->SrcFrame));
    }
	this->SrcFrame = NULL;

    if (NULL != this->DstFrame){
        av_frame_free(&(this->DstFrame));
    }
	this->DstFrame = NULL;

    if (NULL != this->SwsCtx){
        sws_freeContext(this->SwsCtx);
    }
	this->SwsCtx = NULL;
}

bool MediaConvert::Initialize(const EvoVideoInfo &src, const EvoVideoInfo &des)
{

	swsInfo.Width = 0;
	swsInfo.Height = 0;
	swsInfo.Format = AV_PIX_FMT_NONE;

	srcInfo = src;
	desInfo = des;

	/*if (srcInfo.Format == AV_PIX_FMT_NONE
		|| srcInfo.Width <= 0
		|| srcInfo.Height <= 0)
	{
		return false;
	}*/

	this->SrcFrame = av_frame_alloc();
	if (NULL == this->SrcFrame) {
		return false;
	}

	this->DstFrame = av_frame_alloc();
	if (NULL == this->DstFrame) {
		return false;
	}

	return true;
}

bool MediaConvert::CheckSrc(const EvoVideoInfo &info)
{
	if (srcInfo.Format != info.Format) return false;
	if (srcInfo.Width != info.Width) return false;
	if (srcInfo.Height != info.Height) return false;
	return true;
}

bool MediaConvert::CheckDes(const EvoVideoInfo &info)
{
	if (desInfo.Format != AV_PIX_FMT_NONE && desInfo.Format != info.Format) return false;
	if (desInfo.Width != 0 && desInfo.Width != info.Width) return false;
	if (desInfo.Height != 0 && desInfo.Height != info.Height) return false;
	return true;
}

EvoVideoInfo MediaConvert::GetDesInfo()
{
	return this->desInfo;
}

EvoVideoInfo MediaConvert::GetSrcInfo()
{
	return this->srcInfo;
}

EvoVideoInfo MediaConvert::GetDesInfo(const EvoVideoInfo &info)
{
	EvoVideoInfo ret = this->desInfo;
	if (ret.Format == AV_PIX_FMT_NONE)
	{
		ret.Format = info.Format;
	}
	if (ret.Width == 0)
	{
		ret.Width = info.Width;
	}
	if (ret.Height == 0)
	{
		ret.Height = info.Height;
	}
	return ret;
}

EvoVideoInfo MediaConvert::GetSrcInfo(const EvoVideoInfo &info)
{
	EvoVideoInfo ret = this->srcInfo;
	if (ret.Format == AV_PIX_FMT_NONE)
	{
		ret.Format = info.Format;
	}
	if (ret.Width == 0)
	{
		ret.Width = info.Width;
	}
	if (ret.Height == 0)
	{
		ret.Height = info.Height;
	}
	return ret;
}

bool MediaConvert::Check(const EvoVideoInfo &info)
{
	if (SwsCtx != NULL && swsInfo.Format == info.Format && swsInfo.Width == info.Width && swsInfo.Height == info.Height)
	{
		return true;
	}
	return false;
}

bool MediaConvert::Init(const EvoVideoInfo &info)
{
	if (this->SwsCtx != NULL)
	{
		sws_freeContext(this->SwsCtx);
	}

	EvoVideoInfo src = GetSrcInfo(info);
	EvoVideoInfo des = GetDesInfo(info);

	this->SwsCtx = sws_getContext(
		src.Width,
		src.Height,
		src.Format,
		des.Width,
		des.Height,
		des.Format,
		SWS_BICUBIC,
		NULL,
		NULL,
		NULL);

	if (NULL == this->SwsCtx) {
		return false;
	}
	swsInfo = info;
	return true;
}

int MediaConvert::Convert(const AVFrame* srcFrame, AVFrame*desFrame)
{
	EvoVideoInfo info = GetInfo(srcFrame);
	if (Check(info) != true)
	{
		bool ret = Init(info);
		if (ret == false)
		{
			return -1;
		}
	}
	return Convert(srcFrame,(uint8_t*)desFrame->data[0]);
}

int MediaConvert::Convert(const AVFrame* srcFrame, uint8_t* dstBuf)
{
	EvoVideoInfo src = GetSrcInfo(GetInfo(srcFrame));
	EvoVideoInfo des = GetDesInfo(GetInfo(srcFrame));

    if (des.Format == AV_PIX_FMT_YUV420P){
        if (CheckDes(src))
		{
            this->ConvertToYUV(srcFrame, des, dstBuf);
            return 1;
        }
    }

	if (this->SwsCtx == NULL) return -1;
	AVFrame* desframe = this->DstFrame;
    int dstDataLen = 0;
    do
    {
        unsigned int dstSize = GetSize(des);

		av_image_fill_arrays(
			desframe->data,
			desframe->linesize,
			dstBuf,
			des.Format,
			des.Width,
			des.Height,
			1
			);

        memset(dstBuf, 0, sizeof(char)* dstSize);

        int ret = sws_scale(
            this->SwsCtx,
            (const uint8_t* const*)srcFrame->data,
            srcFrame->linesize,
            0,
			src.Height,
			desframe->data,
			desframe->linesize
            );
        if (des.Height == ret){
            dstDataLen = dstSize;
        }

    } while (false);

    return dstDataLen;
}

int MediaConvert::Convert(unsigned char *srcBuf, unsigned char *dstBuf, bool doRotate)
{
	if (this->SwsCtx == NULL) return -1;

    int dstDataLen = 0;
    do
    {
		av_image_fill_arrays(
			this->SrcFrame->data,
			this->SrcFrame->linesize,
			srcBuf,
			this->srcInfo.Format,
			this->srcInfo.Width,
			this->srcInfo.Height,
			1
			);

        this->SrcFrame->width = this->srcInfo.Width;
        this->SrcFrame->height = this->srcInfo.Height;

		unsigned int dstSize = GetSize(desInfo);

		av_image_fill_arrays(
			this->DstFrame->data,
			this->DstFrame->linesize,
			dstBuf,
			this->desInfo.Format,
			this->desInfo.Width,
			this->desInfo.Height,
			1
			);

        memset(dstBuf, 0, sizeof(char)* dstSize);

        //·­×ªÍ¼Ïñ:×óÓÒ·­×ª
        if (doRotate){
            this->SrcFrame->data[0] += this->SrcFrame->linesize[0] * (this->srcInfo.Height - 1);
            this->SrcFrame->linesize[0] *= -1;
            this->SrcFrame->data[1] += this->SrcFrame->linesize[1] * (this->srcInfo.Height / 2 - 1);
            this->SrcFrame->linesize[1] *= -1;
            this->SrcFrame->data[2] += this->SrcFrame->linesize[2] * (this->srcInfo.Height / 2 - 1);
            this->SrcFrame->linesize[2] *= -1;
        }

        int ret = sws_scale(
            this->SwsCtx,
            (const uint8_t* const*)this->SrcFrame->data,
            this->SrcFrame->linesize,
            0,
            this->srcInfo.Height,
            this->DstFrame->data,
            this->DstFrame->linesize
            );
        if (this->desInfo.Height == ret){
            dstDataLen = dstSize;
        }

    } while (false);

    return dstDataLen;
}