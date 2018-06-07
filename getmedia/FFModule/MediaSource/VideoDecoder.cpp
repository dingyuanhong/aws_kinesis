#include "VideoDecoder.h"
#include "MemManager/EvoPacketAllocator.h"
#include "MemManager/EvoAVFrame.h"
#include "core/log.h"

VideoDecoder::VideoDecoder(AVCodecContext	*codec)
	:VideoCodecCtx(codec)
	, Convert(NULL)
	, KeepIFrame(false)
{
	this->VideoFrame = av_frame_alloc();
}

VideoDecoder::~VideoDecoder()
{
	if (this->VideoFrame != NULL) {
		av_frame_free(&this->VideoFrame);
		this->VideoFrame = NULL;
	}
}

void VideoDecoder::Attach(MediaConvert *convert)
{
	Convert = convert;
}

MediaConvert *VideoDecoder::Detach()
{
	MediaConvert *old = Convert;
	Convert = NULL;
	return old;
}

void VideoDecoder::Flush()
{
	if (this->VideoCodecCtx != NULL) {
		avcodec_flush_buffers(this->VideoCodecCtx);
	}
}

int  VideoDecoder::DecodePacket(AVPacket *packet, EvoPacket **evoResult)
{
	int gotFrame = 0;

	if (evoResult != NULL)
	{
		*evoResult = NULL;
	}
#ifndef USE_NEW_DECODER
	if (packet == NULL) {
		return 0;
	}

	int decoded = avcodec_decode_video2(this->VideoCodecCtx, VideoFrame, &gotFrame, packet);
	if (decoded < 0) {
		if (decoded == AVERROR(EAGAIN)) return 0;
		char errbuf[1024] = { 0 };
		av_strerror(decoded, errbuf, 1024);
		LOGD("VideoDecoder::DecodePacket:avcodec_decode_video2:%d(%s).\n", decoded, errbuf);
		if (decoded == AVERROR_INVALIDDATA) return 0;
		if (decoded == AVERROR_EOF) return -1;
		if (decoded == AVERROR(EINVAL)) return -1;
		if (AVERROR(ENOMEM)) return -1;
		return -1;
	}
#else
	int decoded = 0;
	if(packet != NULL)
	{
		decoded = avcodec_send_packet(this->VideoCodecCtx, packet);
		if (decoded < 0)
		{
			if (decoded == AVERROR(EAGAIN)) return 0;
			char errbuf[1024] = { 0 };
			av_strerror(decoded, errbuf,1024);
			LOGD("VideoDecoder::DecodePacket:avcodec_send_packet:%d(%s).\n", decoded, errbuf);
			if (decoded == AVERROR_INVALIDDATA) return 0;
			if (decoded == AVERROR_EOF) return -1;
			if (decoded == AVERROR(EINVAL)) return -1;
			if (AVERROR(ENOMEM)) return -1;
			return -1;
		}
	}

	decoded = avcodec_receive_frame(this->VideoCodecCtx, this->VideoFrame);
	if (decoded < 0)
	{
		if (decoded == AVERROR(EAGAIN)) return 0;
		char errbuf[1024] = { 0 };
		av_strerror(decoded, errbuf, 1024);
		LOGD("VideoDecoder::DecodePacket:avcodec_receive_frame:%d(%s).\n", decoded, errbuf);
		if (decoded == AVERROR_EOF) return -1;
		if (decoded == AVERROR(EINVAL)) return -1;
		return -1;
	}
	else
	{
		gotFrame = 1;
	}
#endif

	if (gotFrame) {

		if (KeepIFrame)
		{
			if (VideoFrame->pict_type == AV_PICTURE_TYPE_P)
			{
				av_frame_unref(this->VideoFrame);
				return 0;
			}
			else if (VideoFrame->pict_type == AV_PICTURE_TYPE_B)
			{
				av_frame_unref(this->VideoFrame);
				return 0;
			}else if (VideoFrame->pict_type == AV_PICTURE_TYPE_I)
			{
				//保留
				KeepIFrame = false;
			}
			else 
			{
				//直接处理掉
				KeepIFrame = false;
			}
		}

		if (Convert != NULL)
		{
			int ret = ConvertFrame(evoResult);
			av_frame_unref(this->VideoFrame);
			return ret;
		}

		int ret = CloneFrame(evoResult);
		av_frame_unref(this->VideoFrame);
		return ret;
	}

	return 0;
}

int VideoDecoder::ConvertFrame(EvoPacket **evoResult)
{
	struct EvoVideoInfo info = MediaConvert::GetInfo(VideoFrame);
	MediaConvert *convert = Convert;
	info = convert->GetDesInfo(info);

	EvoAVFrame * pkt = (EvoAVFrame*)EvoPacketAllocator::PoolNew(info.Width, info.Height, info.Format);

	if (pkt == NULL)
	{
		LOGD("VideoDecoder::DecodePacket:EvoPacketAllocator::CreateAVFrame(%d,%d,%d)==NULL.\n"
			, info.Width, info.Height, info.Format);

		return -1;
	}
	else
	{
		AVFrame* desFrame = pkt->GetData();
		//视频信息
		desFrame->width = info.Width;
		desFrame->height = info.Height;
		desFrame->format = info.Format;

		int ret = convert->Convert(VideoFrame, desFrame);

#ifndef USE_NEW_API
		desFrame->pkt_pts = VideoFrame->pkt_pts;
#endif
		desFrame->pkt_dts = VideoFrame->pkt_dts;
		desFrame->pts = VideoFrame->pts;

		desFrame->pts = av_frame_get_best_effort_timestamp(VideoFrame);

		if (evoResult != NULL)
		{
			*evoResult = pkt;
		}
		else
		{
			EvoPacketAllocator::PoolDelete((EvoPacket**)&pkt);
		}

		return 1;
	}
}

int VideoDecoder::CloneFrame(EvoPacket **evoResult)
{
	struct EvoVideoInfo info = MediaConvert::GetInfo(VideoFrame);
	EvoAVFrame * pkt = (EvoAVFrame*)EvoPacketAllocator::PoolNew(info.Width, info.Height, info.Format);
	if (pkt == NULL)
	{
		LOGD("VideoDecoder::DecodePacket:EvoPacketAllocator::CreateAVFrame(%d,%d,%d)==NULL.\n"
			, info.Width, info.Height, info.Format);

		av_frame_unref(this->VideoFrame);
		return -1;
	}
	else
	{
		AVFrame* desFrame = pkt->GetData();

		//存放视频信息
		desFrame->width = info.Width;
		desFrame->height = info.Height;
		desFrame->format = info.Format;

		//拷贝数据
		int ret = av_frame_copy(desFrame, VideoFrame);

		if (ret < 0)
		{
			EvoPacketAllocator::PoolDelete((EvoPacket**)&pkt);

			LOGD("VideoDecoder::DecodePacket:EvoPacketAllocator::av_frame_copy==(%d).\n"
				, ret);

			return -1;
		}

#ifndef USE_NEW_API
		desFrame->pkt_pts = VideoFrame->pkt_pts;
#endif
		desFrame->pkt_dts = VideoFrame->pkt_dts;
		desFrame->pts = VideoFrame->pts;
		desFrame->pts = av_frame_get_best_effort_timestamp(VideoFrame);

		if (evoResult != NULL)
		{
			*evoResult = pkt;
		}
		else
		{
			EvoPacketAllocator::PoolDelete((EvoPacket**)&pkt);
		}

		return 1;
	}
}