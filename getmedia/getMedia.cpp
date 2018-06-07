#include <stdint.h>

#include "aws/core/http/HttpResponse.h"

#pragma comment(lib,"aws-cpp-sdk-core.lib")
#pragma comment(lib,"aws-cpp-sdk-kinesis-video-media.lib")
#pragma comment(lib,"aws-cpp-sdk-kinesisvideo.lib")

#include <chrono>

#include "GetMediaDataProcess.h"
#include "KinesisMedia.h"
#include "AWSCredentialInfo.h"


class EvoGetMediaDataProcess 
	: public GetMediaDataProcess
{
public:
	EvoGetMediaDataProcess(const char * file)
	{
		fp = fopen(file,"wb");
	}
	~EvoGetMediaDataProcess()
	{
		fclose(fp);
	}
	void DataReceivedEventHandler(const Aws::Http::HttpRequest* request, Aws::Http::HttpResponse* response, long long read)
	{
		char buffer[65535];
		char * tmp = buffer;
		Aws::IOStream& io = response->GetResponseBody();
		std::streamsize size = read;
		io.read(tmp, read);
		if (size == read)
		{
			if (fp != NULL)
			{
				fwrite(tmp, size, 1, fp);
			}
		}
		std::cout << read << std::endl;
	}

	bool ContinueHandler(const Aws::Http::HttpRequest* request)
	{
		return true;
	}
private:
	FILE * fp;
};


void testFile(const char * name, const char * file)
{
	std::shared_ptr<Aws::Auth::SimpleAWSCredentialsProvider> provider(
		new Aws::Auth::SimpleAWSCredentialsProvider(
			AWS_ACCESS_KEY, 
			AWS_SECRET_KEY));
	
	Aws::String streamName = name;
			
	Aws::Client::ClientConfiguration config = GetDefaultConfigure(provider, streamName);

	int count = 10;
	//while (count > 0)
	{
		EvoGetMediaDataProcess process(file);
		int ret = GetMediaData(provider, config, streamName, &process);
		if (ret == -1)
		{
			count--;
		}
	}
}

#pragma comment(lib,"FFModuled.lib")

#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avdevice.lib")
#pragma comment(lib,"avfilter.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swresample.lib")
#pragma comment(lib,"swscale.lib")

#include "FFMPEGGetMediaDataProcess.h"

class FileFFMPEGGetMediaDataProcess :
	 public FFMPEGGetMediaDataProcess
{
public:
	FileFFMPEGGetMediaDataProcess(const char * file)
	{
		fp = fopen(file, "wb");
	}

	~FileFFMPEGGetMediaDataProcess()
	{
		fclose(fp);
	}

	virtual void onMediaData(AVPacket * packet)
	{
		LOGD("read : %lld %lld\n", packet->pts, packet->dts);
		if (packet != NULL && fp != NULL)
		{
			fwrite(packet->data, packet->size, 1, fp);
		}
		
	}
private:
	FILE * fp;
};

void testStream(const char * name,const char * file)
{
	std::shared_ptr<Aws::Auth::SimpleAWSCredentialsProvider> provider(
		new Aws::Auth::SimpleAWSCredentialsProvider(
			AWS_ACCESS_KEY, 
			AWS_SECRET_KEY));
			
	Aws::String streamName = name;
	Aws::Client::ClientConfiguration config = GetDefaultConfigure(provider, streamName);

	int count = 10;
	//while (count > 0)
	{
		FileFFMPEGGetMediaDataProcess process(file);
		process.Start();
		int ret = GetMediaData(provider, config, streamName, &process);
		process.Stop();
		if (ret == -1)
		{
			count--;
		}
	}
}

#include "MediaSource\VideoDecoder.h"
#include "MediaSource\AudioDecoder.h"
#include "InlineMediaSource.h"
#include "core/time_util.h"
#include "MemManager/EvoPacketAllocator.h"
#include "MemManager/EvoAVFrame.h"

class DataReciver 
	: public InlineEvent
	, public MediaReciever
{
public:
	DataReciver()
	{
		playCount = 1;
		index_ = -1;
		codecContext_ = NULL;
		decoder_ = NULL;
		decoder_a = NULL;

		fp = NULL;
	}

	void onBeginGetMedia()
	{

	}

	void onEndGetMedia(int success)
	{
		if (success != -1)
		{
			playCount--;
		}
	}

	void onBeginMedia()
	{

	}

	void onEndMedia(int success)
	{
		CloseMedia();
	}

	void onOpenMedia(EvoMediaSourceBase * source)
	{
		CloseMedia();
		OpenVideo(source);
		OpenAudio(source);
	}

	void OpenVideo(EvoMediaSourceBase * source)
	{
		if (source == NULL) return;
		AVCodecContext * codecContext = source->GetContext(AVMEDIA_TYPE_VIDEO);
		if (codecContext == NULL) return;
		AVCodec *codec = (AVCodec*)codecContext->codec;
		if (codec == NULL) codec = avcodec_find_decoder(codecContext->codec_id);
		if (avcodec_open2(codecContext, codec, NULL) < 0)
		{
			return;
		}
		index_ = source->GetIndex(AVMEDIA_TYPE_VIDEO);
		codecContext_ = codecContext;
		decoder_ = new VideoDecoder(codecContext);

		char name[MAX_PATH];
		sprintf(name, "./temp/%dx%d.yuv", codecContext->width, codecContext->height);
		fp = fopen(name, "wb+");
	}

	void OpenAudio(EvoMediaSourceBase * source)
	{
		if (codecContext_ != NULL) return;
		if (source == NULL) return;
		AVCodecContext * codecContext = source->GetContext(AVMEDIA_TYPE_AUDIO);
		if (codecContext == NULL) return;
		AVCodec *codec = (AVCodec*)codecContext->codec;
		if (codec == NULL) codec = avcodec_find_decoder(codecContext->codec_id);
		if (avcodec_open2(codecContext, codec, NULL) < 0)
		{
			return;
		}
		index_ = source->GetIndex(AVMEDIA_TYPE_AUDIO);
		codecContext_ = codecContext;
		decoder_a = new AudioDecoder(codecContext);

		char name[MAX_PATH];
		sprintf(name, "./temp/%dx%d.pcm", codecContext->sample_rate, codecContext->channels);
		fp = fopen(name, "wb+");
	}

	void CloseMedia()
	{
		if (codecContext_ != NULL)
		{
			avcodec_close(codecContext_);
			codecContext_ = NULL;
		}
		if (decoder_ != NULL)
		{
			delete decoder_;
			decoder_ = NULL;
		}

		if (decoder_a != NULL)
		{
			delete decoder_a;
			decoder_a = NULL;
		}

		if (fp != NULL)
		{
			fclose(fp);
			fp = NULL;
		}
	}

	void onMediaData(AVPacket * packet)
	{
		if (index_ == packet->stream_index)
		{
			if (decoder_ != NULL)
			{
				EvoPacket *result = NULL;
				decoder_->DecodePacket(packet, &result);
				if (result != NULL)
				{
					if (fp != NULL)
					{
						EvoAVFrame *frame = (EvoAVFrame*)result;
						AVFrame * avframe = frame->GetData();

						if (avframe->format == AV_PIX_FMT_YUV420P)
						{
							fwrite(avframe->data[0], avframe->width * avframe->height * 3 / 2, 1, fp);
						}
					}

					EvoPacketAllocator::Delete((EvoPacket**)&result);
				}
			}
			if (decoder_a != NULL)
			{
				EvoPacket *result = NULL;
				decoder_a->DecodePacket(packet, &result);
				if (result != NULL)
				{
					if (fp != NULL)
					{
						EvoAVFrame *frame = (EvoAVFrame*)result;
						AVFrame * avframe = frame->GetData();

						int size = av_samples_get_buffer_size(avframe->linesize, avframe->channels,avframe->nb_samples,(AVSampleFormat)avframe->format,1);

						fwrite(avframe->data[0], size, 1, fp);
					}

					EvoPacketAllocator::Delete((EvoPacket**)&result);
				}
			}

			LOGD("video pts:%lld dts:%lld timebase:(%d/%d)\n", packet->pts, packet->dts, codecContext_->time_base.num, codecContext_->time_base.den);
			/*int64_t pts = packet->pts * av_q2d(codecContext_->time_base) * 1000;
			int64_t dts = packet->dts * av_q2d(codecContext_->time_base) * 1000;
			LOGD("video pts:%lld dts:%lld\n", pts, dts);*/
		}
		else
		{
			LOGD("获取到其他流.\n");
		}
	}
	
	int getPlayCount()
	{
		return playCount;
	}
private:
	int playCount;
	VideoDecoder * decoder_;
	AudioDecoder * decoder_a;
	AVCodecContext * codecContext_;
	int index_;
	FILE * fp;
};

void testInline(const char * name, const char * file)
{
	std::shared_ptr<Aws::Auth::SimpleAWSCredentialsProvider> provider(
		new Aws::Auth::SimpleAWSCredentialsProvider(
			AWS_ACCESS_KEY, 
			AWS_SECRET_KEY));
			
	Aws::String streamName = name;

	InlineMediaSource source;

	DataReciver reciver;
	source.SetCallback(&reciver);
	source.SetEvent(&reciver);

	source.Start(streamName,provider);

	while (true)
	{
		if (reciver.getPlayCount() <= 0)
		{
			break;
		}

		usleep(1000);
	}

	source.Stop();
}


int main()
{
	Aws::SDKOptions options;
	Aws::InitAPI(options);

	//testFile("test", "./temp/1.mkv");
	testFile("test_audio", "./temp/2.mkv");
	//testStream("test_audio","./temp/1.aac");
	//testInline("test_audio","");

	Aws::ShutdownAPI(options);
	return -1;
}