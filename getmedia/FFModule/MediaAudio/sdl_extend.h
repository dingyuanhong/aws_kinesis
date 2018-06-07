#pragma once
#include "../FFheader.h"
#include <SDL/SDL.h>

typedef struct FSColorFormat
{
	AVPixelFormat	f;
	uint32_t		s;
}FSColorFormat;

static FSColorFormat arrColorFormat[] = {
	{ AV_PIX_FMT_NONE, SDL_PIXELFORMAT_UNKNOWN },
	{ AV_PIX_FMT_YUV420P ,SDL_PIXELFORMAT_IYUV },
	{ AV_PIX_FMT_YUYV422 ,SDL_PIXELFORMAT_YUY2 },
	{ AV_PIX_FMT_RGB24 ,SDL_PIXELFORMAT_RGB24 },
	{ AV_PIX_FMT_BGR24 ,SDL_PIXELFORMAT_BGR24 },
	{ AV_PIX_FMT_YUV422P , SDL_PIXELFORMAT_UNKNOWN },
	{ AV_PIX_FMT_YUV444P , SDL_PIXELFORMAT_UNKNOWN },
	{ AV_PIX_FMT_YUV410P , SDL_PIXELFORMAT_UNKNOWN },
	{ AV_PIX_FMT_YUV411P , SDL_PIXELFORMAT_UNKNOWN },

	{ AV_PIX_FMT_ARGB ,SDL_PIXELFORMAT_BGRA8888 },
	{ AV_PIX_FMT_RGBA ,SDL_PIXELFORMAT_ABGR8888 },
	{ AV_PIX_FMT_ABGR ,SDL_PIXELFORMAT_RGBA8888 },
	{ AV_PIX_FMT_BGRA ,SDL_PIXELFORMAT_ARGB8888 },

	{ AV_PIX_FMT_YUVJ420P , SDL_PIXELFORMAT_YV12 },
	{ AV_PIX_FMT_YUV440P , SDL_PIXELFORMAT_UNKNOWN },
	{ AV_PIX_FMT_YUVJ440P , SDL_PIXELFORMAT_UNKNOWN },
	{ AV_PIX_FMT_YUVA420P , SDL_PIXELFORMAT_UNKNOWN },
	{ AV_PIX_FMT_YUVJ440P , SDL_PIXELFORMAT_UNKNOWN }
};

static uint32_t GetSDLFormat(AVPixelFormat format)
{
	for (size_t i = 0; i < sizeof(arrColorFormat) / sizeof(arrColorFormat[0]); i++)
	{
		if (arrColorFormat[i].f == format) return arrColorFormat[i].s;
	}
	return SDL_PIXELFORMAT_UNKNOWN;
}