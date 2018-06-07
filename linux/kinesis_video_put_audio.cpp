#include <string.h>
#include <chrono>
#include <Logger.h>
#include "KinesisVideoProducer.h"
#include <vector>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"{
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
    #include "libswresample/swresample.h"
    #include "libavcodec/avcodec.h"
    #include "libavutil/avutil.h"
    #include "libavutil/imgutils.h"
    #include "libavutil/time.h"
}
#endif

#define LOGE printf
#define LOGD printf

// #define DUMP_FILE

using namespace std;
using namespace com::amazonaws::kinesis::video;
using namespace log4cplus;

LOGGER_TAG("com.amazonaws.kinesis.video.gstreamer");

#define ACCESS_KEY_ENV_VAR "AWS_ACCESS_KEY_ID"
#define SECRET_KEY_ENV_VAR "AWS_SECRET_ACCESS_KEY"
#define SESSION_TOKEN_ENV_VAR "AWS_SESSION_TOKEN"
#define DEFAULT_REGION_ENV_VAR "AWS_DEFAULT_REGION"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class SampleClientCallbackProvider : public ClientCallbackProvider {
public:

    StorageOverflowPressureFunc getStorageOverflowPressureCallback() override {
        return storageOverflowPressure;
    }

    static STATUS storageOverflowPressure(UINT64 custom_handle, UINT64 remaining_bytes);
};

class SampleStreamCallbackProvider : public StreamCallbackProvider {
public:

    StreamConnectionStaleFunc getStreamConnectionStaleCallback() override {
        return streamConnectionStaleHandler;
    };

    StreamErrorReportFunc getStreamErrorReportCallback() override {
        return streamErrorReportHandler;
    };

    DroppedFrameReportFunc getDroppedFrameReportCallback() override {
        return droppedFrameReportHandler;
    };

private:
    static STATUS
    streamConnectionStaleHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                                 UINT64 last_buffering_ack);

    static STATUS
    streamErrorReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle, UINT64 errored_timecode,
                             STATUS status_code);

    static STATUS
    droppedFrameReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                              UINT64 dropped_frame_timecode);
};

class SampleCredentialProvider : public StaticCredentialProvider {
    // Test rotation period is 40 second for the grace period.
    const std::chrono::duration<uint64_t> ROTATION_PERIOD = std::chrono::seconds(2400);
public:
    SampleCredentialProvider(const Credentials &credentials) :
            StaticCredentialProvider(credentials) {}

    void updateCredentials(Credentials &credentials) override {
        // Copy the stored creds forward
        credentials = credentials_;

        // Update only the expiration
        auto now_time = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch());
        auto expiration_seconds = now_time + ROTATION_PERIOD;
        credentials.setExpiration(std::chrono::seconds(expiration_seconds.count()));
        LOG_INFO("New credentials expiration is " << credentials.getExpiration().count());
    }
};

class SampleDeviceInfoProvider : public DefaultDeviceInfoProvider {
public:
    device_info_t getDeviceInfo() override {
        auto device_info = DefaultDeviceInfoProvider::getDeviceInfo();
        // Set the storage size to 256mb
        device_info.storageInfo.storageSize = 512 * 1024 * 1024;
        return device_info;
    }
};

STATUS
SampleClientCallbackProvider::storageOverflowPressure(UINT64 custom_handle, UINT64 remaining_bytes) {
    UNUSED_PARAM(custom_handle);
    LOG_WARN("Reporting storage overflow. Bytes remaining " << remaining_bytes);
    return STATUS_SUCCESS;
}

STATUS SampleStreamCallbackProvider::streamConnectionStaleHandler(UINT64 custom_data,
                                                                  STREAM_HANDLE stream_handle,
                                                                  UINT64 last_buffering_ack) {
    LOG_WARN("Reporting stream stale. Last ACK received " << last_buffering_ack);
    return STATUS_SUCCESS;
}

STATUS
SampleStreamCallbackProvider::streamErrorReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                                                       UINT64 errored_timecode, STATUS status_code) {
    LOG_ERROR("Reporting stream error. Errored timecode: " << errored_timecode << " Status: "
                                                           << status_code);
    return STATUS_SUCCESS;
}

STATUS
SampleStreamCallbackProvider::droppedFrameReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,
                                                        UINT64 dropped_frame_timecode) {
    LOG_WARN("Reporting dropped frame. Frame timecode " << dropped_frame_timecode);
    return STATUS_SUCCESS;
}

}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;

unique_ptr<Credentials> credentials_;

typedef struct _CustomData {
    unique_ptr<KinesisVideoProducer> kinesis_video_producer;
    shared_ptr<KinesisVideoStream> kinesis_video_stream;
    bool stream_started;
} CustomData;

int frame_index = 0;

void create_kinesis_video_frame(Frame *frame, const nanoseconds &pts, const nanoseconds &dts, FRAME_FLAGS flags,
                                void *data, size_t len) {
    frame->flags = flags;
	frame->index = frame_index++;
	
	auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count() / DEFAULT_TIME_UNIT_IN_NANOS;
    frame->decodingTs = timestamp;
    frame->presentationTs = timestamp;

    //frame->decodingTs = static_cast<UINT64>(dts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
    //frame->presentationTs = static_cast<UINT64>(pts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
	
    frame->duration = 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    frame->size = static_cast<UINT32>(len);
    frame->frameData = reinterpret_cast<PBYTE>(data);
}

bool put_frame(shared_ptr<KinesisVideoStream> kinesis_video_stream, void *data, size_t len, const nanoseconds &pts, const nanoseconds &dts, FRAME_FLAGS flags) {
    Frame frame;
    create_kinesis_video_frame(&frame, pts, dts, flags, data, len);
    LOG_INFO("pts:" << frame.presentationTs  << " dts:" << frame.decodingTs  );
    return kinesis_video_stream->putFrame(frame);
}

void kinesis_video_init(CustomData *data, char *stream_name,uint32_t flags,string codec_id) {
    unique_ptr<DeviceInfoProvider> device_info_provider = make_unique<SampleDeviceInfoProvider>();
    unique_ptr<ClientCallbackProvider> client_callback_provider = make_unique<SampleClientCallbackProvider>();
    unique_ptr<StreamCallbackProvider> stream_callback_provider = make_unique<SampleStreamCallbackProvider>();

    char const *accessKey;
    char const *secretKey;
    char const *sessionToken;
    char const *defaultRegion;
    string defaultRegionStr;
    string sessionTokenStr;
    if (nullptr == (accessKey = getenv(ACCESS_KEY_ENV_VAR))) {
        accessKey = "AccessKey";
    }

    if (nullptr == (secretKey = getenv(SECRET_KEY_ENV_VAR))) {
        secretKey = "SecretKey";
    }

    if (nullptr == (sessionToken = getenv(SESSION_TOKEN_ENV_VAR))) {
        sessionTokenStr = "";
    } else {
        sessionTokenStr = string(sessionToken);
    }

    if (nullptr == (defaultRegion = getenv(DEFAULT_REGION_ENV_VAR))) {
        defaultRegionStr = DEFAULT_AWS_REGION;
    } else {
        defaultRegionStr = string(defaultRegion);
    }

    credentials_ = make_unique<Credentials>(string(accessKey),
                                            string(secretKey),
                                            sessionTokenStr,
                                            std::chrono::seconds(180));
    unique_ptr<CredentialProvider> credential_provider = make_unique<SampleCredentialProvider>(*credentials_.get());

    data->kinesis_video_producer = KinesisVideoProducer::createSync(move(device_info_provider),
                                                                    move(client_callback_provider),
                                                                    move(stream_callback_provider),
                                                                    move(credential_provider),
                                                                    defaultRegionStr);

    LOG_DEBUG("Client is ready");
    /* create a test stream */
    map<string, string> tags;
    char tag_name[MAX_TAG_NAME_LEN];
    char tag_val[MAX_TAG_VALUE_LEN];
    sprintf(tag_name, "piTag");
    sprintf(tag_val, "piValue");
    auto stream_definition = make_unique<StreamDefinition>(stream_name,
                    hours(2),
                    &tags,
                    "",
                    STREAMING_TYPE_REALTIME,
                    "audio/aac",
                    milliseconds::zero(),
                    seconds(1),
                    milliseconds(1),
                    true,//Construct a fragment at each key frame
                    true,//Use provided frame timecode
                    true,//Relative timecode
                    true,//Ack on fragment is enabled
                    true,//SDK will restart when error happens
                    true,//recalculate_metrics
                    flags,
                    // NAL_ADAPTATION_ANNEXB_NALS | NAL_ADAPTATION_ANNEXB_CPD_NALS,
                    25,
                    8 * 1024 * 1024,
                    seconds(120),
                    seconds(40),
                    seconds(30),
                //    "V_MPEG4/ISO/AVC",
                    // "A_AAC/MPEG4/MAIN",
                    codec_id,
                    "kinesis_audio"
                    );

    
    //codec_id
    //https://matroska.org/technical/specs/codecid/index.html

    data->kinesis_video_stream = data->kinesis_video_producer->createStreamSync(move(stream_definition));

    LOG_DEBUG("Stream is ready");
}


#ifdef USE_NEW_API
static AVCodecContext *CreateCodecContent(AVCodecParameters *codecpar)
{
    AVCodecContext *codecContext = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(codecContext, codecpar);
    return codecContext;
}
#endif

//毫秒
inline uint64_t  time_millisecond(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

static int64_t last_pts = AV_NOPTS_VALUE;
static int64_t last_time = AV_NOPTS_VALUE;

void asyncTimestamp(AVPacket * packet,AVRational time_base)
{
    //毫秒
    int64_t pts = (packet->pts != AV_NOPTS_VALUE) ? (packet->pts * av_q2d(time_base) * 1000) : NAN;
    int64_t dts = (packet->dts != AV_NOPTS_VALUE) ? (packet->dts * av_q2d(time_base) * 1000) : NAN;

    if(last_time == AV_NOPTS_VALUE)
    {
        last_pts = pts;
        last_time = time_millisecond();
        return;
    }

    int64_t max_sleep = (pts - last_pts) - (time_millisecond() - last_time);
    if( max_sleep > 0 )
    {
        LOGD("max_sleep:%d\n",max_sleep);
        usleep((max_sleep-1) * 1000);
    }

    last_pts = pts;
    last_time = time_millisecond();

    //纳秒
    pts *= HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_TIME_UNIT_IN_NANOS;
    dts *= HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_TIME_UNIT_IN_NANOS;
}


void getTimestamp(AVPacket * packet,AVRational time_base,int64_t &pts,int64_t &dts)
{
    //毫秒
    pts = (packet->pts != AV_NOPTS_VALUE) ? (packet->pts * av_q2d(time_base) * 1000) : NAN;
    dts = (packet->dts != AV_NOPTS_VALUE) ? (packet->dts * av_q2d(time_base) * 1000) : NAN;

    //纳秒
    pts *= HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_TIME_UNIT_IN_NANOS;
    dts *= HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_TIME_UNIT_IN_NANOS;

    if(pts != dts)
    {
        pts = dts;
    }
}

static void on_new_sample(AVPacket * packet,AVRational time_base, CustomData *data) {
    FRAME_FLAGS kinesis_video_flags = FRAME_FLAG_NONE;
    if(packet->flags == AV_PKT_FLAG_KEY)
    {
        kinesis_video_flags = FRAME_FLAG_KEY_FRAME;
    }

    asyncTimestamp(packet,time_base);

    int64_t pts,dts;
    getTimestamp(packet,time_base,pts,dts);

#ifdef DUMP_FILE
    static int g_index = 0;
    char path[64];
    sprintf(path,"/data/wwwroot/data/tmp/%d.bin",g_index++);
    FILE * fp = fopen(path,"wb");
    if(fp != NULL)
    {
        fwrite(packet->data, packet->size,1,fp);
        fclose(fp);
    }
#endif

    LOGD("PTS:%lld DTS:%lld NALU:%d %d %d %d %d\n",pts,dts,packet->data[0],packet->data[1],packet->data[2],packet->data[3],packet->data[4]);

    int count = 5;
	LOGD("put frame begin:%lld\n",time_millisecond());
retry:
    if (false == put_frame(data->kinesis_video_stream, packet->data, packet->size, std::chrono::nanoseconds(pts),
                           std::chrono::nanoseconds(dts), kinesis_video_flags)) {
        usleep(1*1000);
        count--;
        if(count > 0){

            LOGE("put_frame false, retry!\n");

            goto retry;
        }
        LOGE("put_frame false, drop!\n");
    }

	LOGD("put frame end:%lld\n",time_millisecond());
}

#include "TransformADTS.h"

int gstreamer_init(int argc, char* argv[])
{
    BasicConfigurator config;
    config.configure();

    if (argc < 2) {
        LOG_ERROR(
                "Usage: AWS_ACCESS_KEY_ID=SAMPLEKEY AWS_SECRET_ACCESS_KEY=SAMPLESECRET ./kinesis_video_gstreamer_sample_app my-stream-name -w width -h height -f framerate -b bitrateInKBPS");
        return 1;
    }

    CustomData data;

    /* init data struct */
    memset(&data, 0, sizeof(data));

    /* init stream format */
    int opt;
    char *endptr;
    char * path = NULL;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
        case 'p':
            path = optarg;
            break;
        default: /* '?' */
            LOGE("Invalid arguments\n");
            return 1;
        }
    }

    if(path != NULL)
    {
        printf("%s\n",path);
    }

    /* init Kinesis Video */
    char stream_name[MAX_STREAM_NAME_LEN];
    SNPRINTF(stream_name, MAX_STREAM_NAME_LEN, argv[optind]);



    AVFormatContext * context_ = NULL;
    AVCodec * codec_ = NULL;
    AVStream *stream_ = NULL;
    int  stream_index_ = -1;
    AVCodecContext *codecContext_ = NULL;

    av_register_all();

    context_ = avformat_alloc_context();

    AVDictionary* options = NULL;
    av_dict_set(&options, "max_analyze_duration", "100", 0);
    av_dict_set(&options, "probesize", "1024", 0);
    int ret = avformat_open_input(&context_,path,NULL, &options);
    if (ret != 0)
    {
        if (context_ != NULL)
        {
            avformat_close_input(&context_);
        }
        context_ = NULL;
        return ret;
    }
    if (avformat_find_stream_info(context_, NULL))
    {
        if (context_ != NULL)
        {
            avformat_close_input(&context_);
        }
        context_ = NULL;
        return -1;
    }
	
	auto codec_type = AVMEDIA_TYPE_AUDIO;
	
    for (size_t i = 0; i < context_->nb_streams; i++)
    {
#ifdef USE_NEW_API
        if (context_->streams[i]->codecpar->codec_type == codec_type)
        {
            stream_ = context_->streams[i];
            codecContext_ = CreateCodecContent(stream_->codecpar);
#else
        if (context_->streams[i]->codec->codec_type == codec_type)
        {
            stream_ = context_->streams[i];
            codecContext_ = stream_->codec;
#endif
            stream_index_ = (int)i;
            if (codec_ == NULL) codec_ = avcodec_find_decoder(codecContext_->codec_id);
            if (codec_ != NULL)
            {
                codecContext_->codec = codec_;
            }
            if (stream_ != NULL && codecContext_ != NULL && codecContext_->codec != NULL)
            {
                LOGD("DECODE:%s\n", codecContext_->codec->name);
            }
        }
    }

    if(stream_ == NULL || !(
        codecContext_ != NULL && 
        codecContext_->codec_type == AVMEDIA_TYPE_AUDIO &&
        codecContext_->codec_id == AV_CODEC_ID_AAC
    ))
    {
        if (codecContext_ != NULL)
        {
            avcodec_close(codecContext_);
#ifdef USE_NEW_API
            avcodec_free_context(&codecContext_);
#endif
            codecContext_ = NULL;
        }
        if (context_ != NULL)
        {
            avformat_close_input(&context_);
        }
        context_ = NULL;
        return -1;
    }

    AVPacket * packet_ = NULL;
    packet_ = (AVPacket*)av_malloc(sizeof(AVPacket));
    av_init_packet(packet_);
	
	LOGD("get one video data begin\n");
	
    while(true){
        ret = av_read_frame(context_, packet_);
        if(ret == 0)
        {
			if(packet_->stream_index == stream_index_)
			{
				break;
			}
        }
        av_packet_unref(packet_);
    }
	LOGD("get one video data end\n");
	
    const char *codec_id_table[] = {
        "A_AAC/MPEG2/MAIN",
        "A_AAC/MPEG2/LC",
        "A_AAC/MPEG2/LC/SBR",
        "A_AAC/MPEG2/SSR",
        "A_AAC/MPEG4/MAIN",
        "A_AAC/MPEG4/LC",
        "A_AAC/MPEG4/LC/SBR",
        "A_AAC/MPEG4/SSR",
        "A_AAC/MPEG4/LTP"
    };

    uint32_t flags = NAL_ADAPTATION_FLAG_NONE;
	
    const char * profile_ptr = NULL;
    int profile = codecContext_->profile;
    switch (profile)
	{
	case FF_PROFILE_AAC_MAIN:
        profile_ptr = codec_id_table[4];
        break;
	case FF_PROFILE_AAC_LOW:
        profile_ptr = codec_id_table[5];
        break;
	case FF_PROFILE_AAC_SSR:
        profile_ptr = codec_id_table[7];
        break;
	case FF_PROFILE_AAC_LTP:
        profile_ptr = codec_id_table[8];
        break;
	case FF_PROFILE_MPEG2_AAC_LOW:
		profile_ptr = codec_id_table[1];
        break;
    default:
        profile_ptr = codec_id_table[5];
	}

    if(profile_ptr == NULL)
    {
        LOGE("profile error:%d\n",profile);
        return -1;
    }

    LOGE("profile:%s\n",profile_ptr);

	kinesis_video_init(&data, stream_name,flags,profile_ptr);
	
	LOGD("process data begin\n");
	

    ADTS adts = {NULL};
    
    AVPacket *des = av_packet_alloc();
	av_init_packet(des);

#ifdef DUMP_AAC
    FILE * fp = fopen("./1.aac","wb");
#endif

    do {
		
        if(ret == 0)
        {
            if(packet_->stream_index == stream_index_)
            {
				LOGD("send begin\n");
                
                TransformADTS(&adts,codecContext_,packet_,&des);
                if(des != NULL)
                {
                    LOGD("ADTS:%x %x %x %x %x %x %x  -  %x %x %x %x\n",des->data[0],des->data[1],des->data[2],des->data[3],
                        des->data[4],des->data[5],des->data[6],
                        des->data[7],des->data[8],des->data[9],des->data[10]);
                    on_new_sample(des, stream_->time_base, &data);
#ifdef DUMP_AAC
                    fwrite(des->data,des->size,1,fp);
#endif
                    av_packet_unref(des);
                }
				LOGD("send over\n");
            }
			av_packet_unref(packet_);
        }
        else if (ret == AVERROR_EOF)
        {
            break;
        }
		else
		{
			LOGD("read frame error:%d\n",ret);
			break;
		}
		ret = av_read_frame(context_, packet_);
		LOGD("read running\n");
    }while(true);

#ifdef DUMP_AAC
    fclose(fp);
#endif

	LOGD("process data end\n");
	
    av_packet_unref(packet_);
    av_free(packet_);

    if (codecContext_ != NULL)
    {
        avcodec_close(codecContext_);
#ifdef USE_NEW_API
        avcodec_free_context(&codecContext_);
#endif
        codecContext_ = NULL;
    }
    if (context_ != NULL)
    {
        avformat_close_input(&context_);
    }
    context_ = NULL;
    LOGD("处理结束!\n");
    return 0;
}

int main(int argc, char* argv[]) {
    return gstreamer_init(argc, argv);
}
