#include "MediaSourceBase.h"

#ifdef USE_NEW_API
static AVCodecContext *CreateCodecContent(AVCodecParameters *codecpar)
{
	AVCodecContext *codecContext = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(codecContext, codecpar);
	return codecContext;
}
#endif

EvoMediaSourceBase::EvoMediaSourceBase()
	:iocontent_(NULL),
	context_(NULL),
	packet_(NULL),
	codecType_(AVMEDIA_TYPE_UNKNOWN),
	index_(-1),
	stream_(NULL),
	codecContext_(NULL)
{
	memset(codecContexts_,0,sizeof(AVCodecContext *)*MAX_CONTENT_COUNT);
}

EvoMediaSourceBase::~EvoMediaSourceBase()
{
	Close();
}

int EvoMediaSourceBase::Open(PIO_Read_Packet read_cb, void* opaque, int io_size, AVMediaType codecType, ConigureSourceBase configure)
{
	av_register_all();

	if (io_size <= 0)
	{
		io_size = IO_SIZE_DEFAULT;
	}

	unsigned char * io_buffer = (unsigned char *)av_malloc(io_size);
	iocontent_ = avio_alloc_context(io_buffer, io_size, 0, opaque, read_cb, NULL, NULL);

	if (NULL == iocontent_)
	{
		if(io_buffer != NULL) av_free(io_buffer);
		return -1;
	}

	context_ = avformat_alloc_context();

	context_->pb = iocontent_;
	context_->flags = AVFMT_FLAG_CUSTOM_IO;
	AVDictionary* options = NULL;

	//调整探测头避免过长的等待
	if (configure.Inline)
	{
		av_dict_set(&options, "max_analyze_duration", "100", 0);
		av_dict_set(&options, "probesize", "1024", 0);
		av_dict_set(&options, "stimeout", "3000000", 0);
		//av_dict_set(&options, "buffer_size", "10240000", 0);
	}

	if (avformat_open_input(&context_, NULL, NULL, &options) != 0) {
		Close();
		return -1;
	}

	return OpenStream(codecType);
}

int EvoMediaSourceBase::Open(const char * file, AVMediaType codecType, ConigureSourceBase configure)
{
	av_register_all();

	context_ = avformat_alloc_context();

	AVDictionary* options = NULL;

	//调整探测头避免过长的等待
	if (configure.Inline)
	{
		av_dict_set(&options, "max_analyze_duration", "5000", 0);
		av_dict_set(&options, "probesize", "5120", 0);
		av_dict_set(&options, "stimeout", "3000000", 0);
		//av_dict_set(&options, "buffer_size", "1024000", 0);
	}

	int ret = avformat_open_input(&context_,file,NULL, NULL);
	if (ret != 0)
	{
		if (context_ != NULL)
		{
			avformat_close_input(&context_);
		}
		context_ = NULL;
		return ret;
	}
	if (configure.Inline)
	{
		context_->probesize = 5 * 1024;
		context_->max_analyze_duration = 5 * AV_TIME_BASE;
	}
	return OpenStream(codecType);
}

int EvoMediaSourceBase::OpenStream(AVMediaType codecType)
{
#ifndef USE_NEW_API
	for (unsigned int i = 0; i < context_->nb_streams; i++)
	{
		if (codecType == AVMEDIA_TYPE_UNKNOWN)
		{
			AVStream *stream = context_->streams[i];
			AVCodec * codec = find_decoder(stream);
			if (codec == NULL) codec = avcodec_find_decoder(stream->codec->codec_id);
			if (codec != NULL)
			{
				stream->codec->codec = codec;
			}
		}
		else if (context_->streams[i]->codec->codec_type == codecType)
		{
			AVStream *stream = context_->streams[i];
			AVCodec * codec = find_decoder(stream);
			if (codec == NULL) codec = avcodec_find_decoder(stream->codec->codec_id);
			if (codec != NULL)
			{
				stream->codec->codec = codec;
			}
			stream_ = stream;
			index_ = i;
			break;
		}
	}
#endif

	if (avformat_find_stream_info(context_, NULL))
	{
		if (context_ != NULL)
		{
			avformat_close_input(&context_);
		}
		context_ = NULL;
		return -1;
	}

	if (codecType != AVMEDIA_TYPE_UNKNOWN)
	{
		for (unsigned int i = 0; i < context_->nb_streams; i++)
		{
#ifdef USE_NEW_API
			if (context_->streams[i]->codecpar->codec_type == codecType)
#else
			if (context_->streams[i]->codec->codec_type == codecType)
#endif
			{

				AVStream *stream = context_->streams[i];
#ifdef USE_NEW_API
				AVCodecContext * codecContext = CreateCodecContent(stream->codecpar);
#else
				AVCodecContext * codecContext = stream->codec;
#endif
				AVCodec * codec = find_decoder(stream);
				if (codec == NULL) codec = avcodec_find_decoder(codecContext->codec_id);
				if (codec != NULL)
				{
					codecContext->codec = codec;
				}

				index_ = i;
				stream_ = stream;
				codecContext_ = codecContext;

				if (stream != NULL && codecContext != NULL && codecContext->codec != NULL)
				{
					printf("EvoMediaSourceBase DECODE:%s\n", codecContext->codec->name);
				}
			}
		}
	}

	if (codecType != AVMEDIA_TYPE_UNKNOWN &&
		(index_ == -1 || stream_ == NULL || codecContext_ == NULL || codecContext_->codec == NULL))
	{
		Close();
		return -1;
	}

	packet_ = (AVPacket*)av_malloc(sizeof(AVPacket));
	memset(packet_, 0, sizeof(AVPacket));
	av_init_packet(packet_);

	codecType_ = codecType;

	return 0;
}

void EvoMediaSourceBase::Close()
{
	if (codecContext_ != NULL)
	{
		avcodec_close(this->codecContext_);
#ifdef USE_NEW_API
		avcodec_free_context(&codecContext_);
#endif
		this->codecContext_ = NULL;
	}

	if (context_ != NULL)
	{
		avformat_close_input(&context_);
		context_ = NULL;
	}

	for (int i = 0; i < MAX_CONTENT_COUNT; i++)
	{
		AVCodecContext * content = codecContexts_[i];
		if (content != NULL)
		{
			//avcodec_close(content);
#ifdef USE_NEW_API
			avcodec_free_context(&content);
#endif
			//content = NULL;
		}
	}
	memset(codecContexts_, 0, sizeof(AVCodecContext *)*MAX_CONTENT_COUNT);

	if (packet_ != NULL)
	{
#ifdef USE_NEW_API
		av_packet_unref(packet_);
#else
		av_free_packet(packet_);
#endif
		av_freep(packet_);
		packet_ = NULL;
	}
	
	index_ = -1;
	stream_ = NULL;
}

int EvoMediaSourceBase::Seek(int64_t millisecond)
{
	if (context_ == NULL) return -1;
	int duration = GetDuration();
	if (duration > 0 && millisecond > duration)
	{
		millisecond = duration;
	}
	int64_t timeStamp = millisecond * 1000;
	int ret = av_seek_frame(context_,-1, timeStamp, AVSEEK_FLAG_BACKWARD);
	if (ret >= 0)
	{
		avformat_flush(this->context_);
		return 0;
	}
	return ret;
}

int EvoMediaSourceBase::ReadFrame(AVPacket * packet)
{
	if (context_ == NULL) return -1;

	bool get_packet = false;
	do {
		int ret = av_read_frame(context_, packet_);
		if (ret == 0)
		{
			if (codecType_ == AVMEDIA_TYPE_UNKNOWN)
			{
				av_packet_ref(packet, packet_);
				get_packet = true;
			}
			else if(index_ == packet_->stream_index)
			{
				av_packet_ref(packet, packet_);
				get_packet = true;
			}
		}

		if (packet_ != NULL)
		{
			av_packet_unref(packet_);
		}

		if (get_packet == true)
		{
			break;
		}
		if (ret == AVERROR_EOF)
		{
			//文件结束
			return ret;
		}
	} while (true);

	return 0;
}

int EvoMediaSourceBase::ReadFrame()
{
	if (context_ == NULL) return -1;

	int ret = av_read_frame(context_, packet_);
	if (ret == 0)
	{
		if (codecType_ == AVMEDIA_TYPE_UNKNOWN)
		{
			onData(packet_);
		}
		else if (packet_->stream_index == index_)
		{
			onData(packet_);
		}
	}

	if (packet_ != NULL)
	{
		av_packet_unref(packet_);
	}
	if (ret == AVERROR_EOF)
	{
		//文件结束
		return ret;
	}
	return ret;
}

int EvoMediaSourceBase::ReadAllFrame()
{
	if (context_ == NULL) return -1;
	do {
		int ret = av_read_frame(context_, packet_);
		if (ret == 0)
		{
			if (codecType_ == AVMEDIA_TYPE_UNKNOWN)
			{
				onData(packet_);
			}
			else if (packet_->stream_index == index_)
			{
				onData( packet_);
			}
		}

		if (packet_ != NULL) 
		{
			av_packet_unref(packet_);
		}
		if (ret == AVERROR_EOF)
		{
			//文件结束
			return ret;
		}
	} while (true);

	return 0;
}

int EvoMediaSourceBase::GetDuration()
{
	if (context_ == NULL) return 0;
	return (int)(context_->duration / 1000);
}

AVMediaType EvoMediaSourceBase::GetType()
{
	return codecType_;
}

AVStream *EvoMediaSourceBase::GetStream()
{
	return stream_;
}

AVCodecContext *EvoMediaSourceBase::GetContext()
{
	return codecContext_;
}

int EvoMediaSourceBase::GetIndex(AVMediaType type)
{
	if (context_ == NULL) return AVMEDIA_TYPE_UNKNOWN;
	for (unsigned int i = 0; i < context_->nb_streams; i++)
	{
#ifdef USE_NEW_API
		if (context_->streams[i]->codecpar->codec_type == type)
#else
		if (context_->streams[i]->codec->codec_type == type)
#endif
		{
			return i;
		}
	}
	return -1;
}

AVCodecContext *EvoMediaSourceBase::GetContext(AVMediaType type)
{
	if (context_ == NULL) return NULL;
	for (unsigned int i = 0; i < context_->nb_streams; i++)
	{
#ifdef USE_NEW_API
		if (context_->streams[i]->codecpar->codec_type == type)
#else
		if (context_->streams[i]->codec->codec_type == type)
#endif
		{
			return GetContext(i);
		}
	}
	return NULL;
}

unsigned int EvoMediaSourceBase::GetStreamCount()
{
	if (context_ == NULL) return 0;
	return context_->nb_streams;
}

AVMediaType EvoMediaSourceBase::GetType(unsigned int index)
{
	if (context_ == NULL || index >= context_->nb_streams) return AVMEDIA_TYPE_UNKNOWN;
	AVStream * stream = context_->streams[index];
#ifdef USE_NEW_API
	return stream->codecpar->codec_type;
#else
	return stream->codec->codec_type;
#endif
}

AVStream *EvoMediaSourceBase::GetStream(unsigned int index)
{
	if (context_ == NULL || index >= context_->nb_streams) return NULL;
	return context_->streams[index];
}

AVCodecContext *EvoMediaSourceBase::GetContext(unsigned int index)
{
	if (context_ == NULL || index >= context_->nb_streams) return NULL;
	if (index >= MAX_CONTENT_COUNT) return NULL;

	if (codecContexts_[index] != NULL) return codecContexts_[index];
	AVStream * stream = context_->streams[index];
#ifdef USE_NEW_API
	AVCodecContext * codecContext = CreateCodecContent(stream->codecpar);
#else
	AVCodecContext * codecContext = stream->codec;
#endif
	AVCodec * codec = find_decoder(stream);
	if (codec == NULL) codec = avcodec_find_decoder(codecContext->codec_id);
	if (codec != NULL)
	{
		codecContext->codec = codec;
	}
	codecContexts_[index] = codecContext;
	return codecContext;
}