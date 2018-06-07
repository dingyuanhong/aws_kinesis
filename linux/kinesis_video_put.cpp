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
    //frame->decodingTs = timestamp;
    //frame->presentationTs = timestamp;

    frame->decodingTs = static_cast<UINT64>(dts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
    frame->presentationTs = static_cast<UINT64>(pts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
	
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

void kinesis_video_init(CustomData *data, char *stream_name,uint32_t flags,unsigned char * privateData,int privateSize) {
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
                                                           "video/h264",
                                                           milliseconds::zero(),
                                                           seconds(1),
                                                           milliseconds(1),
                                                           true,//Construct a fragment at each key frame
                                                           false,//Use provided frame timecode
                                                           false,//Relative timecode
                                                           true,//Ack on fragment is enabled
                                                           true,//SDK will restart when error happens
                                                           true,//recalculate_metrics
                                                           flags,
                                                           30,
                                                           8 * 1024 * 1024,
                                                           seconds(10),
                                                           seconds(4),
                                                           seconds(3),
                                                           "V_MPEG4/ISO/AVC",
                                                           "kinesis_video",
                                                           privateData,
                                                           privateSize);
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

static int time_index = 0;
static int use_time_index = false;

static int64_t last_pts = 0;
static int64_t last_time = 0;
static int64_t time_begin = 0;

static int64_t current_flow = 0;


// static int64_t current_limiting = 5*1024*1024;

static void on_new_sample(AVPacket * packet,AVRational time_base, CustomData *data) {
    FRAME_FLAGS kinesis_video_flags = FRAME_FLAG_NONE;
    if(packet->flags == AV_PKT_FLAG_KEY)
    {
        kinesis_video_flags = FRAME_FLAG_KEY_FRAME;
    }
    //毫秒
    int64_t pts = (packet->pts != AV_NOPTS_VALUE) ? (packet->pts * av_q2d(time_base) * 1000) : NAN;
    int64_t dts = (packet->dts != AV_NOPTS_VALUE) ? (packet->dts * av_q2d(time_base) * 1000) : NAN;

    // int64_t pts = packet->pts / 1000;
    // int64_t dts = packet->dts / 1000;

    // int64_t pts = packet->pts ;
    // int64_t dts = packet->dts ;

    //LOGD("PTS:%lld DTS:%lld time_base:AVRational(%d/%d)\n",pts,dts,time_base.num,time_base.den);

    if(time_index == 0 && (pts == NAN || dts == NAN))
    {
        use_time_index = true;
    }
    if(use_time_index)
    {
        pts = dts = time_index*1000/30;
    }
    time_index++;

    if(pts != dts)
    {
        pts = dts;
    }

    int64_t max_sleep = (pts - last_pts) - (time_millisecond() - last_time);
    if( max_sleep > 0 )
    {
        if(time_begin == 0)
        {
            time_begin = time_millisecond();
        }else{
            int64_t alive_time = (time_millisecond() - time_begin);
            if(alive_time > 0)
            {
                int64_t flow = (current_flow + packet->size ) * 1000 / alive_time;

                // StreamMetrics metrics;
                // data->kinesis_video_stream->getStreamMetrics(metrics);
                // LOGD("StreamMetrics:%d %d\n",metrics.currentFrameRate,metrics.currentTransferRate);

                //LOGD("flow:%d\n",flow);

                // if(flow > 300*1024)
                {
                    //LOGD("flow:%d max_sleep:%d\n",flow,max_sleep);
                    usleep((max_sleep-10) * 1000);
                }
            }
        }
    }

    last_pts = pts;
    last_time = time_millisecond();
    current_flow += packet->size;

    //纳秒
    pts *= HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_TIME_UNIT_IN_NANOS;
    dts *= HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_TIME_UNIT_IN_NANOS;

    pts *= 0.5;
    dts *= 0.5;

    //pts /= av_q2d({1001,48000});
    //dts /= av_q2d({1001,48000});

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

    //LOGD("PTS:%lld DTS:%lld NALU:%d %d %d %d %d\n",pts,dts,packet->data[0],packet->data[1],packet->data[2],packet->data[3],packet->data[4]);

    int count = 5;
	LOGD("put frame begin:%lld\n",time_millisecond());
retry:
    if (false == put_frame(data->kinesis_video_stream, packet->data, packet->size, std::chrono::nanoseconds(pts),
                           std::chrono::nanoseconds(dts), kinesis_video_flags)) {
        //LOGE("Dropped frame!\n");
        usleep(10*1000);
        count--;
        if(count > 0){
            goto retry;
        }
    }

	LOGD("put frame end:%lld\n",time_millisecond());
}

//检查是否为标准H264
uint32_t is_annex_b(uint8_t * packet, uint32_t size)
{
    unsigned char ANNEXB_CODE_LOW[] = { 0x00,0x00,0x01 };
    unsigned char ANNEXB_CODE[] = { 0x00,0x00,0x00,0x01 };

    unsigned char *data = packet;
    if (data == NULL) return 0;
    uint32_t isAnnexb = 0;
    if ((size > 3 && memcmp(data, ANNEXB_CODE_LOW, 3) == 0) ||
        (size > 4 && memcmp(data, ANNEXB_CODE, 4) == 0)
        )
    {
        isAnnexb = 1;
    }
    return isAnnexb;
}

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

    for (size_t i = 0; i < context_->nb_streams; i++)
    {
#ifdef USE_NEW_API
        if (context_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            stream_ = context_->streams[i];
            codecContext_ = CreateCodecContent(stream_->codecpar);
#else
        if (context_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
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

    if(stream_ == NULL)
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
	
    uint32_t flags = 0;

    bool use_filter = false;
    bool use_extra = false;
    if(codecContext_->extradata != NULL)
    {
        use_extra = true;
    }
    if(use_extra)
    {
        LOGD("-----------------------------------------------------------------\n");
        LOGD("private %x %d\n",codecContext_->extradata,codecContext_->extradata_size);
        LOGD("extra:%d %d %d %d %d\n",
            codecContext_->extradata[0],
            codecContext_->extradata[1],
            codecContext_->extradata[2],
            codecContext_->extradata[3],
            codecContext_->extradata[4]
            );
        if(is_annex_b(codecContext_->extradata,codecContext_->extradata_size))
        {
            flags |= NAL_ADAPTATION_ANNEXB_CPD_NALS;
        }
        LOGD("packet:%d %d %d %d %d\n",
            packet_->data[0],
            packet_->data[1],
            packet_->data[2],
            packet_->data[3],
            packet_->data[4]
            );
        if(use_filter || is_annex_b(packet_->data,packet_->size))
        {
            flags |= NAL_ADAPTATION_ANNEXB_NALS;
        }
        LOGD("-----------------------------------------------------------------\n");
        kinesis_video_init(&data, stream_name, flags ,
            codecContext_->extradata,codecContext_->extradata_size);
        LOGD("-----------------------------------------------------------------\n");
    }
    else
    {
        LOGD("-----------------------------------------------------------------\n");
        LOGD("packet:%d %d %d %d %d\n",
            packet_->data[0],
            packet_->data[1],
            packet_->data[2],
            packet_->data[3],
            packet_->data[4]
            );
        use_filter = true;
        if(use_filter || is_annex_b(packet_->data,packet_->size))
        {
            flags |= NAL_ADAPTATION_ANNEXB_NALS;
        }
        LOGD("-----------------------------------------------------------------\n");
        kinesis_video_init(&data, stream_name,flags,NULL,0);
        LOGD("-----------------------------------------------------------------\n");
    }

	LOGD("process data begin\n");
	
    AVBitStreamFilterContext *avcbsfc = av_bitstream_filter_init("h264_mp4toannexb");
    do {
		
        if(ret == 0)
        {
            if(packet_->stream_index == stream_index_)
            {
                if(use_filter)
                {
                    int ret = av_bitstream_filter_filter(avcbsfc,codecContext_,NULL,
                    &packet_->data,&packet_->size,
                    packet_->data,packet_->size,0);
                    LOGD("av_bitstream_filter_filter:%d\n",ret);
                    if(ret < 0)
                    {
                        LOGE("处理失败\n");
                    }
                    else
                    {
                        on_new_sample(packet_, stream_->time_base, &data);
                    }
                }else
                {
                    on_new_sample(packet_, stream_->time_base, &data);
                }
            }
            av_packet_unref(packet_);
        }
        if (ret == AVERROR_EOF)
        {
            break;
        }
		if ( ret != 0)
		{
			LOGD("read frame error:%d\n",ret);
			break;
		}
		ret = av_read_frame(context_, packet_);
    }while(true);
	
	LOGD("process data end\n");
	
    av_packet_unref(packet_);
    av_free(packet_);
    av_bitstream_filter_close(avcbsfc);

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
