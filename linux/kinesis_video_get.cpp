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
#define ASSERT_EQ(A,B) {if((A) == (B)) LOGD(#B);}
#define EXPECT_EQ(A,B) {if((A) == (B)) LOGE(#B);}
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

void create_kinesis_video_frame(Frame *frame, const nanoseconds &pts, const nanoseconds &dts, FRAME_FLAGS flags,
                                void *data, size_t len) {
    frame->flags = flags;
    frame->decodingTs = static_cast<UINT64>(dts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
    frame->presentationTs = static_cast<UINT64>(pts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
    frame->duration = 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    frame->size = static_cast<UINT32>(len);
    frame->frameData = reinterpret_cast<PBYTE>(data);
}

bool put_frame(shared_ptr<KinesisVideoStream> kinesis_video_stream, void *data, size_t len, const nanoseconds &pts, const nanoseconds &dts, FRAME_FLAGS flags) {
    Frame frame;
    create_kinesis_video_frame(&frame, pts, dts, flags, data, len);
    return kinesis_video_stream->putFrame(frame);
}

void kinesis_video_init(CustomData *data, char *stream_name,unsigned char * privateData,int privateSize) {
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
                                                           seconds(2),
                                                           milliseconds(1),
                                                           true,//Construct a fragment at each key frame
                                                           true,//Use provided frame timecode
                                                           false,//Relative timecode
                                                           true,//Ack on fragment is enabled
                                                           true,//SDK will restart when error happens
                                                           true,//recalculate_metrics
                                                           NAL_ADAPTATION_ANNEXB_NALS,
                                                           30,
                                                           4 * 1024 * 1024,
                                                           seconds(120),
                                                           seconds(40),
                                                           seconds(30),
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


#define TEST_DEVICE_NAME                "Test device name"
#define MAX_TEST_STREAM_COUNT           10
#define TEST_DEVICE_STORAGE_SIZE        MIN_STORAGE_ALLOCATION_SIZE

#define TEST_CLIENT_MAGIC_NUMBER        0x1234567890ULL

#define TEST_CERTIFICATE_BITS           "Test certificate bits"
#define TEST_TOKEN_BITS                 "Test token bits"
#define TEST_DEVICE_FINGERPRINT         "Test device fingerprint"

#define TEST_STREAM_NAME                "Test stream name"
#define TEST_CONTENT_TYPE               "TestType"
#define TEST_CODEC_ID                   "TestCodec"
#define TEST_TRACK_NAME                 "TestTrack"

#define TEST_DEVICE_ARN                 "TestDeviceARN"

#define TEST_STREAM_ARN                 "TestStreamARN"

#define TEST_STREAMING_ENDPOINT         "http://test.com/test_endpoint"

#define TEST_UPDATE_VERSION             "TestUpdateVer"

#define TEST_STREAMING_TOKEN            "TestStreamingToken"

#define TEST_STREAMING_HANDLE           12345

#define TEST_BUFFER_DURATION            (40 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define TEST_REPLAY_DURATION            (20 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define TEST_FRAME_DURATION             (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define TEST_LONG_FRAME_DURATION        (400 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define TEST_SLEEP_TIME_IN_SECONDS                  1
#define TEST_PRODUCER_SLEEP_TIME_IN_MICROS          20000
#define TEST_CONSUMER_SLEEP_TIME_IN_MICROS          50000

#define TEST_AUTH_EXPIRATION            (UINT64)(-1LL)


#include <com/amazonaws/kinesis/video/client/Include.h>
#include <com/amazonaws/kinesis/video/mkvgen/Include.h>
#include <com/amazonaws/kinesis/video/trace/Include.h>
#include <com/amazonaws/kinesis/video/view/Include.h>
#include <mkvgen/src/Include_i.h>

int gstreamer_init(int argc, char* argv[]) {
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
    
    /* init Kinesis Video */
    char stream_name[MAX_STREAM_NAME_LEN];
    SNPRINTF(stream_name, MAX_STREAM_NAME_LEN, argv[optind]);
    

    kinesis_video_init(&data, stream_name,NULL,0);

    UINT32 i, j, filledSize, offset, bufferSize;
    BOOL validPattern;
    BYTE tempBuffer[1000];
    BYTE getDataBuffer[2000];
    UINT64 timestamp, clientStreamHandle, availableStorage, curAvailableStorage;
    Frame frame;
    ClientMetrics memMetrics;

    for (i = 0, timestamp = 0; timestamp < TEST_BUFFER_DURATION; timestamp += TEST_LONG_FRAME_DURATION, i++) {
        clientStreamHandle = 0;

        // The first frame will have the cluster and MKV overhead
        if (i == 0) {
            offset = MKV_HEADER_OVERHEAD;
        } else if (i % 10 == 0) {
            // Cluster start will have cluster overhead
            offset = MKV_CLUSTER_OVERHEAD;
        } else {
            // Simple block overhead
            offset = MKV_SIMPLE_BLOCK_OVERHEAD;
        }

        // Set the buffer size to be the offset + frame bits size
        bufferSize = SIZEOF(tempBuffer) + offset;

        ASSERT_EQ(STATUS_SUCCESS,
                  getKinesisVideoStreamData(*data.kinesis_video_stream.get()->getStreamHandle(), 
                    &clientStreamHandle, 
                    getDataBuffer, bufferSize, &filledSize));
        LOGD("%lld %d\n",clientStreamHandle,filledSize);

        EXPECT_EQ(bufferSize, filledSize);
        EXPECT_EQ(TEST_STREAMING_HANDLE, clientStreamHandle);

        // Validate the fill pattern
        validPattern = TRUE;
        for (j = 0; j < SIZEOF(tempBuffer); j++) {
            if (getDataBuffer[offset + j] != i) {
                validPattern = FALSE;
                break;
            }
        }

        ASSERT_EQ(TRUE,validPattern);
    }

    
    LOGD("处理结束!\n");
    return 0;
}

int main(int argc, char* argv[]) {
    return gstreamer_init(argc, argv);
}