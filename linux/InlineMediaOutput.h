#ifndef INLINEMEDIAOUTPUT_H
#define INLINEMEDIAOUTPUT_H
#include <string.h>
#include <chrono>
#include <vector>
#include <stdlib.h>
#include <memory>
#include <memory>

#include <Logger.h>
#include "KinesisVideoProducer.h"

LOGGER_TAG("com.amazonaws.kinesis.video.gstreamer");

class InlineMediaOutput
{
public:
	int init(char const *accessKey,char const *secretKey,char const *sessionToken,char const *defaultRegion,
			char *stream_name,uint32_t flags,unsigned char * privateData,int privateSize);
	int put_frame(void *data, size_t len, const nanoseconds &pts, const nanoseconds &dts, FRAME_FLAGS flags);
private:
	std::unique_ptr<com::amazonaws::kinesis::video::KinesisVideoProducer> kinesis_video_producer;
    std::shared_ptr<com::amazonaws::kinesis::video::KinesisVideoStream> kinesis_video_stream;
    bool stream_started;

	std::unique_ptr<com::amazonaws::kinesis::video::Credentials> credentials_;
};


#endif
