#pragma once

#include "aws/core/Aws.h"
#include "aws/core/auth/AWSCredentialsProvider.h"
#include "aws/core/client/ClientConfiguration.h"
#include "aws/core/utils/Outcome.h"
#include "aws/core/http/HttpRequest.h"

#include "GetMediaDataProcess.h"

#define ALLOCATION_TAG "ALLOCATION_TAG"

Aws::String GetDataEndPoint(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& provider,
	const Aws::Client::ClientConfiguration& config, const Aws::String& stream);

Aws::Client::ClientConfiguration GetDefaultConfigure(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& provider
	, const Aws::String& streamName);

int GetMediaData(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& provider,
	const Aws::Client::ClientConfiguration& config, const Aws::String& stream,
	GetMediaDataProcess * process);