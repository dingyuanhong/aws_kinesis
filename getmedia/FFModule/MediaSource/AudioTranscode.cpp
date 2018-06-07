#include "AudioTranscode.h"
#include "MediaConvert.h"

AudioTranscode::AudioTranscode()
	:ADTSHeader(NULL)
{
}

AudioTranscode::~AudioTranscode()
{
	Close();
}

int AudioTranscode::Init(const SourceParams &params)
{
	av_register_all();

	CodecCtx = avcodec_alloc_context3(NULL); 

	CodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	CodecCtx->codec_id = AV_CODEC_ID_AAC;

	CodecCtx->bit_rate = 128000;

	CodecCtx->time_base.num = 1;
	CodecCtx->time_base.den = params.sample_rate;
	//最重要的4个参数
	CodecCtx->sample_rate = params.sample_rate;	// 44100;
	CodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
	CodecCtx->channels = params.channels;
	CodecCtx->channel_layout = av_get_default_channel_layout(CodecCtx->channels);

	AVCodec* codec = avcodec_find_encoder(CodecCtx->codec_id);
	if (!codec) {
		printf("Can not find encoder!\n");
		return -1;
	}

	AVDictionary *param = NULL;
	int ret = avcodec_open2(CodecCtx, codec, &param);
	if (ret < 0) {
		printf("Failed to open encoder!\n");
		return -1;
	}

	int samples_size = CodecCtx->frame_size;//样本数

	//转换后的数据存储区域
	DesFrame = av_frame_alloc();
	DesFrame->nb_samples = samples_size;
	DesFrame->format = CodecCtx->sample_fmt;

	int32_t frameSize = av_samples_get_buffer_size(NULL, CodecCtx->channels, samples_size, CodecCtx->sample_fmt, 1);
	uint8_t* frame_buf = (uint8_t *)av_malloc(frameSize);

	avcodec_fill_audio_frame(DesFrame, CodecCtx->channels,
		CodecCtx->sample_fmt, (const uint8_t*)frame_buf, frameSize, 1);

	WriteADTSHeader(0, params.sample_rate, params.channels);

	return InitSWR(params);
}

int AudioTranscode::InitSWR(const SourceParams &srcParams)
{
	AVCodecContext*  codecCtx = CodecCtx;
	//转换器
	/* 由AV_SAMPLE_FMT_FLT转为AV_SAMPLE_FMT_FLTP */
	Swr_ctx = swr_alloc_set_opts(
		NULL,
		av_get_default_channel_layout(codecCtx->channels),
		codecCtx->sample_fmt,                   //在编码前，我希望的采样格式  
		codecCtx->sample_rate,
		av_get_default_channel_layout(srcParams.channels),
		srcParams.sample_fmt,                      //PCM源文件的采样格式  
		srcParams.sample_rate,
		0, NULL);

	swr_init(Swr_ctx);
	/* 分配空间 */
	uint8_t ** convert_data = (uint8_t**)calloc(codecCtx->channels,
		sizeof(*convert_data));

	av_samples_alloc(convert_data, NULL,
		codecCtx->channels, codecCtx->frame_size,
		codecCtx->sample_fmt, 0);

	return 0;
}

int AudioTranscode::Transcode(AVFrame *frame, AVPacket *packet)
{
	if (packet == NULL)
	{
		return -1;
	}
#ifndef USE_NEW_DECODER
	if (packet == NULL) {
		return 0;
	}
	int got_frame = 0;
	int ret = 0;
	if (frame != NULL)
	{
		swr_convert(Swr_ctx, DesFrame->data, DesFrame->nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
		DesFrame->pts = frame->pts;
		DesFrame->pkt_dts = frame->pkt_dts;
	}

	int decoded = avcodec_decode_audio4(this->CodecCtx, DesFrame, &got_frame, packet);
	if (decoded < 0) {
		if (decoded == AVERROR(EAGAIN)) return 0;
		char errbuf[1024] = { 0 };
		av_strerror(decoded, errbuf, 1024);
		//LOGA("VideoDecoder::DecodePacket:avcodec_decode_video2:%d(%s).\n", decoded, errbuf);
		if (decoded == AVERROR_INVALIDDATA) return 0;
		if (decoded == AVERROR_EOF) return -1;
		if (decoded == AVERROR(EINVAL)) return -1;
		if (AVERROR(ENOMEM)) return -1;
		return -1;
	}
#else	
	int got_frame = 0;
	int ret = 0;
	if (frame != NULL)
	{
		swr_convert(Swr_ctx, DesFrame->data, DesFrame->nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
		DesFrame->pts = frame->pts;
		DesFrame->pkt_dts = frame->pkt_dts;

		ret = avcodec_send_frame(CodecCtx, DesFrame);
	}
	if (ret < 0)
	{
		if (ret == AVERROR(EAGAIN)) return 0;
		char errbuf[1024] = { 0 };
		av_strerror(ret, errbuf, 1024);
		//LOGA("VideoDecoder::DecodePacket:avcodec_send_packet:%d(%s).\n", ret, errbuf);
		if (ret == AVERROR_INVALIDDATA) return 0;
		if (ret == AVERROR_EOF) return -1;
		if (ret == AVERROR(EINVAL)) return -1;
		if (AVERROR(ENOMEM)) return -1;
		return -1;
	}

	ret = avcodec_receive_packet(this->CodecCtx, packet);
	if (ret < 0)
	{
		if (ret == AVERROR(EAGAIN)) return 0;
		char errbuf[1024] = { 0 };
		av_strerror(ret, errbuf, 1024);
		//LOGA("VideoDecoder::DecodePacket:avcodec_receive_frame:%d(%s).\n", ret, errbuf);
		if (ret == AVERROR_EOF) return -1;
		if (ret == AVERROR(EINVAL)) return -1;
		return -1;
	}
	else
	{
		got_frame = 1;
	}
#endif

	if (got_frame == 1) {

		ADTS(packet,&packet);

		//LOGA("Succeed to encode 1 frame! \tsize:%5d pts:%Id dts:%Id\n", packet->size, packet->pts, packet->dts);
		return 1;
	}

	return 0;
}

int AudioTranscode::GetSampleSize()
{
	return CodecCtx->frame_size;
}

void AudioTranscode::Close()
{
	if (DesFrame != NULL)
	{
		av_free(DesFrame->data[0]);
		av_frame_free(&DesFrame);
	}

	if (Swr_ctx != NULL)
	{
		swr_free(&Swr_ctx);
	}

	if (CodecCtx = NULL)
	{
		avcodec_close(CodecCtx);
		avcodec_free_context(&CodecCtx);
	}

	if (ADTSHeader != NULL)
	{
		av_free(ADTSHeader);
		ADTSHeader = NULL;
	}
}

#define ADTS_HEADER_SIZE 7

const int avpriv_mpeg4audio_sample_rates[16] = {
	96000, 88200, 64000, 48000, 44100, 32000,
	24000, 22050, 16000, 12000, 11025, 8000, 7350
};

static int GetSampleIndex(int sample_rate)
{
	for (int i = 0; i < 16; i++)
	{
		if (sample_rate == avpriv_mpeg4audio_sample_rates[i])
		{
			return i;
		}
	}
	return -1;
}

const uint8_t ff_mpeg4audio_channels[8] = {
	0, 1, 2, 3, 4, 5, 6, 8
};

void AudioTranscode::WriteADTSHeader(int Size, int sample_rate,int channels)
{
	if (ADTSHeader == NULL)
	{
		ADTSHeader = (char*)av_malloc(ADTS_HEADER_SIZE);
	}
	memset(ADTSHeader,0, ADTS_HEADER_SIZE);

	int length = ADTS_HEADER_SIZE + Size;
	length &= 0x1FFF;

	int sample_index = GetSampleIndex(sample_rate);
	int channel = 0;

	if (channels < FF_ARRAY_ELEMS(ff_mpeg4audio_channels))
		channel = ff_mpeg4audio_channels[channels];

	ADTSHeader[0] = (char)0xff;
	ADTSHeader[1] = (char)0xf1;
	ADTSHeader[2] = (char)(0x40 | (sample_index << 2) | (channel >> 2));
	ADTSHeader[3] = (char)((channel & 0x3) << 6 | (length >> 11));
	ADTSHeader[4] = (char)(length >> 3) & 0xff;
	ADTSHeader[5] = (char)(((length & 0x7) << 5) & 0xff) | 0x1f;
	ADTSHeader[6] = (char)0xfc;
}

int AudioTranscode::ADTS(AVPacket *src, AVPacket **des)
{
	if (src == NULL) return -1;
	if (des == NULL) return -1;

	AVPacket *adtsPacket = av_packet_alloc();
	av_init_packet(adtsPacket);
	av_new_packet(adtsPacket, src->size + ADTS_HEADER_SIZE);
	WriteADTSHeader(src->size, CodecCtx->sample_rate, CodecCtx->channels);
	memcpy(adtsPacket->data, ADTSHeader, ADTS_HEADER_SIZE);
	memcpy(adtsPacket->data + ADTS_HEADER_SIZE, src->data, src->size);

	adtsPacket->pts = src->pts;
	adtsPacket->dts = src->dts;
	adtsPacket->duration = src->duration;
	adtsPacket->flags = src->flags;
	adtsPacket->stream_index = src->stream_index;
	adtsPacket->pos = src->pos;

	if (*des == src)
	{
		av_packet_unref(src);
		av_packet_move_ref(*des, adtsPacket);
	}
	else if (*des != NULL)
	{
		av_packet_move_ref(*des, adtsPacket);
	}
	else
	{
		*des = adtsPacket;
	}

	return 0;
}