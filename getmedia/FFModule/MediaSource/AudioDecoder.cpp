#include "AudioDecoder.h"
#include "MemManager/EvoPacketAllocator.h"
#include "MemManager/EvoMemory.h"
#include "MemManager/EvoAVFrame.h"
#include "MediaConvert.h"
#include "core/log.h"

AudioDecoder::AudioDecoder(AVCodecContext	*codec)
	:AudioCodecCtx(codec), AudioConvertCtx(NULL)
{
	this->AudioFrame = av_frame_alloc();
}

AudioDecoder::~AudioDecoder()
{
	if (this->AudioConvertCtx != NULL) {
		swr_free(&this->AudioConvertCtx);
		this->AudioConvertCtx = NULL;
	}

	if (this->AudioFrame != NULL) {
		av_frame_free(&this->AudioFrame);
		this->AudioFrame = NULL;
	}
}

void AudioDecoder::Flush() {
	if (this->AudioCodecCtx != NULL) {
		avcodec_flush_buffers(this->AudioCodecCtx);
	}
}

int AudioDecoder::AudioResampling(AVFrame * audioDecodeFrame, uint8_t * audioBuffer, int bufferLen)
{
	if (!this->AudioConvertCtx) {
		int64_t decChannelLayout, tagChannelLayout;
		decChannelLayout = av_get_default_channel_layout(audioDecodeFrame->channels);
		tagChannelLayout = av_get_default_channel_layout(this->AudioCodecCtx->channels);

		this->AudioConvertCtx = swr_alloc_set_opts(this->AudioConvertCtx,
			tagChannelLayout,
			AV_SAMPLE_FMT_S16,
			this->AudioCodecCtx->sample_rate,
			decChannelLayout,
			(AVSampleFormat)(audioDecodeFrame->format), audioDecodeFrame->sample_rate, 0, NULL);

		if (!this->AudioConvertCtx || swr_init(this->AudioConvertCtx) < 0) {

		}
	}

	swr_convert(this->AudioConvertCtx, &audioBuffer, bufferLen, (const uint8_t **)(audioDecodeFrame->data), audioDecodeFrame->nb_samples);
	return 1;
}

int AudioDecoder::DecodePacket(AVPacket *packet, EvoPacket **evoResult)
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

	int decoded = avcodec_decode_audio4(this->AudioCodecCtx, AudioFrame, &gotFrame, packet);
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
	if (packet != NULL)
	{
		decoded = avcodec_send_packet(this->AudioCodecCtx, packet);
		if (decoded < 0)
		{
			if (decoded == AVERROR(EAGAIN)) return 0;
			char errbuf[1024] = { 0 };
			av_strerror(decoded, errbuf, 1024);
			LOGD("AudioDecoder::DecodePacket:avcodec_send_packet:%d(%s).\n", decoded, errbuf);
			if (decoded == AVERROR_EOF) return -1;
			if (decoded == AVERROR(EINVAL)) return -1;
			if (AVERROR(ENOMEM)) return -1;
			return -1;
		}
	}

	decoded = avcodec_receive_frame(this->AudioCodecCtx, this->AudioFrame);
	if (decoded < 0)
	{
		if (decoded == AVERROR(EAGAIN)) return 0;
		char errbuf[1024] = { 0 };
		av_strerror(decoded, errbuf, 1024);
		LOGD("AudioDecoder::DecodePacket:avcodec_receive_frame:%d(%s).\n", decoded, errbuf);
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

		EvoAVFrame * audioFrame = (EvoAVFrame*)EvoPacketAllocator::PoolNew(this->AudioFrame->channels, this->AudioFrame->nb_samples, AV_SAMPLE_FMT_S16,1);
		if (audioFrame != NULL)
		{
			AVFrame * frame = audioFrame->GetData();
			
			int ret = this->AudioResampling(this->AudioFrame, frame->data[0], audioFrame->GetPacketSize());
			frame->pts = this->AudioFrame->pts;
			frame->pkt_dts = this->AudioFrame->pkt_dts;
			frame->nb_samples = this->AudioFrame->nb_samples;
			frame->channels = this->AudioFrame->channels;
			frame->channel_layout = this->AudioFrame->channel_layout;
			frame->format = AV_SAMPLE_FMT_S16;
			frame->sample_rate = this->AudioCodecCtx->sample_rate;
			
			if (evoResult != NULL)
			{
				*evoResult = audioFrame;
			}
			else
			{
				EvoPacketAllocator::PoolDelete((EvoPacket**)&audioFrame);
			}
		}
		else
		{
			av_frame_unref(this->AudioFrame);
			return -1;
		}
		av_frame_unref(this->AudioFrame);
		return 1;
	}
	return 0;
}

EvoPacket* AudioDecoder::Decode(AVPacket *packet)
{
	int gotFrame = 0;
#ifndef USE_NEW_DECODER
	if (packet == NULL) {
		return NULL;
	}

	int decoded = avcodec_decode_audio4(this->AudioCodecCtx, AudioFrame, &gotFrame, packet);
	if (decoded < 0) {
		if (decoded == AVERROR(EAGAIN)) return NULL;
		char errbuf[1024] = { 0 };
		av_strerror(decoded, errbuf, 1024);
		LOGD("VideoDecoder::DecodePacket:avcodec_decode_video2:%d(%s).\n", decoded, errbuf);
		if (decoded == AVERROR_INVALIDDATA) return NULL;
		if (decoded == AVERROR_EOF) return NULL;
		if (decoded == AVERROR(EINVAL)) return NULL;
		if (AVERROR(ENOMEM)) return NULL;
		return NULL;
	}
#else
	int decoded = 0;
	if (packet != NULL)
	{
		decoded = avcodec_send_packet(this->AudioCodecCtx, packet);
		if (decoded < 0)
		{
			return NULL;
		}
	}

	decoded = avcodec_receive_frame(this->AudioCodecCtx, this->AudioFrame);
	if (decoded < 0)
	{
		return NULL;
	}
	else
	{
		gotFrame = 1;
	}
#endif
	if (gotFrame) {

		EvoAVFrame * audioFrame = (EvoAVFrame*)EvoPacketAllocator::PoolNew(this->AudioFrame->channels, this->AudioFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);
		if (audioFrame != NULL)
		{
			AVFrame * frame = audioFrame->GetData();

			int ret = this->AudioResampling(this->AudioFrame, frame->data[0], audioFrame->GetPacketSize());

			frame->pts = this->AudioFrame->pts;
			frame->pkt_dts = this->AudioFrame->pkt_dts;
			frame->nb_samples = this->AudioFrame->nb_samples;
			frame->channels = this->AudioFrame->channels;
			frame->channel_layout = this->AudioFrame->channel_layout;
			frame->format = AV_SAMPLE_FMT_S16;
			frame->sample_rate = this->AudioCodecCtx->sample_rate;

			return audioFrame;
		}
		else
		{
			return NULL;
		}
	}
	return NULL;
}
