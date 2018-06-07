#include "KinesisMedia.h"

#include "aws/core/utils/ratelimiter/RateLimiterInterface.h"
#include "aws/core/utils/ratelimiter/DefaultRateLimiter.h"

#include "aws/kinesisvideo/KinesisVideoClient.h"
#include "aws/kinesisvideo/KinesisVideoEndpoint.h"
#include "aws/kinesisvideo/model/GetDataEndpointRequest.h"
#include "aws/kinesisvideo/model/GetDataEndpointResult.h"

#include "aws/kinesis-video-media/KinesisVideoMediaClient.h"
#include "aws/kinesis-video-media/KinesisVideoMediaEndpoint.h"
#include "aws/kinesis-video-media/KinesisVideoMediaErrors.h"
#include "aws/kinesis-video-media/model/GetMediaRequest.h"
#include "aws/kinesis-video-media/model/GetMediaResult.h"
#include "aws/kinesis-video-media/model/StartSelector.h"
#include "aws/kinesis-video-media/model/StartSelectorType.h"

#include "core/log.h"

using namespace Aws::KinesisVideoMedia;

Aws::String GetDataEndPoint(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& provider,
	const Aws::Client::ClientConfiguration& config, const Aws::String& stream)
{
	Aws::String  endpoint;
	auto client = Aws::MakeShared<Aws::KinesisVideo::KinesisVideoClient>(ALLOCATION_TAG, provider, config);
	Aws::KinesisVideo::Model::GetDataEndpointRequest request;
	request.SetStreamName(stream);
	request.SetAPIName(Aws::KinesisVideo::Model::APIName::GET_MEDIA);
	Aws::KinesisVideo::Model::GetDataEndpointOutcome out = client->GetDataEndpoint(request);
	if (out.IsSuccess())
	{
		endpoint = out.GetResult().GetDataEndpoint();
	}
	if (endpoint.size() == 0)
	{
		return "";
	}
	size_t index = endpoint.find("http://");
	if (index != -1)
	{
		endpoint = endpoint.substr(7);
	}
	index = endpoint.find("https://");
	if (index != -1)
	{
		endpoint = endpoint.substr(8);
	}
	return endpoint;
}


Aws::Client::ClientConfiguration GetDefaultConfigure(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& provider
	, const Aws::String& streamName)
{
	auto limiter = Aws::MakeShared<Aws::Utils::RateLimits::DefaultRateLimiter<>>(ALLOCATION_TAG, 10000000);
	Aws::Client::ClientConfiguration config = Aws::Client::ClientConfiguration();

	config.scheme = Aws::Http::Scheme::HTTPS;
	config.region = Aws::Region::AP_NORTHEAST_1;
	config.useDualStack = false;

	Aws::String  endpoint = GetDataEndPoint(provider, config, streamName);
	if (endpoint.size() == 0)
	{
		LOGE("GetDataEndPoint empty.\n");
		//config.endpointOverride = Aws::KinesisVideoMedia::KinesisVideoMediaEndpoint::ForRegion(config.region, config.useDualStack);
		return config;
	}
	else
	{
		std::cout << "endpoint:" << endpoint << std::endl;
		config.endpointOverride = endpoint;
	}
	

#ifndef USE_DEFAULT_HTTP
	config.httpLibOverride = Aws::Http::TransferLibType::DEFAULT_CLIENT;
#else
	config.httpLibOverride = Aws::Http::TransferLibType::CURL_CLIENT;
	config.verifySSL = true;
	config.caPath = "E:\\work\\amazon-kinesis\\";
	config.caFile = "cacert.pem";
#endif

	config.connectTimeoutMs = 3000;
	config.requestTimeoutMs = 3000;
	/*config.readRateLimiter = limiter;
	config.writeRateLimiter = limiter;*/

	return config;
}

int GetMediaData(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& provider,
	const Aws::Client::ClientConfiguration& config, const Aws::String& stream,
	GetMediaDataProcess * process)
{
	Model::StartSelector selector;
	selector.SetStartSelectorType(Model::StartSelectorType::NOW);
	Model::GetMediaRequest request;
	request.SetStreamName(stream);
	request.SetStartSelector(selector);

	Aws::Http::DataReceivedEventHandler hander = std::bind(&GetMediaDataProcess::DataReceivedEventHandler, process, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	request.SetDataReceivedEventHandler(hander);

	Aws::Http::ContinueRequestHandler continueHandler = std::bind(&GetMediaDataProcess::ContinueHandler, process, std::placeholders::_1);
	request.SetContinueRequestHandler(continueHandler);

	auto client = Aws::MakeShared<KinesisVideoMediaClient>(ALLOCATION_TAG, provider, config);

	Model::GetMediaOutcome out = client->GetMedia(request);
	if (out.IsSuccess())
	{
		return 0;
	}
	return -1;
}