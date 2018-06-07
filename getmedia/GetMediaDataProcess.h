#pragma once

#include "aws/core/http/HttpRequest.h"
#include "aws/core/http/HttpResponse.h"

class GetMediaDataProcess {
public:
	virtual void DataReceivedEventHandler(const Aws::Http::HttpRequest* request, Aws::Http::HttpResponse* response, long long read) = 0;
	virtual bool ContinueHandler(const Aws::Http::HttpRequest* request) = 0;
};