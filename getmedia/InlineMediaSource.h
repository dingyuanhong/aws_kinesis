#pragma once

#include "MediaSource\MediaSourceBase.h"
#include "KinesisMedia.h"
#include "GetMediaDataProcess.h"

#include "core/lock.h"
#include "core/log.h"
#include "core/thread.h"

#include "memoryBuffer.h"

class InlineEvent
{
public:
	virtual void onBeginGetMedia() = 0;
	virtual void onEndGetMedia(int success) = 0;
	virtual void onBeginMedia() = 0;
	virtual void onEndMedia(int success) = 0;
	virtual void onOpenMedia(EvoMediaSourceBase * source) = 0;
};

class InlineMediaSource
	:public EvoMediaSourceBase
	, public GetMediaDataProcess
{
public:
	InlineMediaSource();
	~InlineMediaSource();

	int Start(Aws::String streamName, std::shared_ptr<Aws::Auth::AWSCredentialsProvider> cre);
	int Stop();

	void SetEvent(InlineEvent * ev) { event_ = ev; }
protected:
	static void ThreadInline_(void* arg)
	{
		InlineMediaSource *thiz = (InlineMediaSource*)arg;
		thiz->ThreadInline();
	}

	static void ThreadPlay_(void* arg)
	{
		InlineMediaSource *thiz = (InlineMediaSource*)arg;
		thiz->ThreadOpen();
	}

	static int CallbackRead_(void *opaque, uint8_t *buf, int buf_size)
	{
		InlineMediaSource *thiz = (InlineMediaSource*)opaque;
		return thiz->Fill(buf, buf_size);
	}
private:
	void ThreadInline();
	void DataReceivedEventHandler(const Aws::Http::HttpRequest* request, Aws::Http::HttpResponse* response, long long read);
	bool ContinueHandler(const Aws::Http::HttpRequest* request);

	void ThreadOpen();
	int Fill(uint8_t *buf, int buf_size);
private:
	Aws::String stream;
	std::shared_ptr<Aws::Auth::AWSCredentialsProvider> provider;

	uv_sem_t sem_data;
	spin_mutex lock;
	uv_thread_t thread_inline;
	uv_thread_t thread_play;
	uv_sem_t sem_play;
	MemroryBuffer::MemroryBuffer buffer;
	int stop;

	InlineEvent * event_;
};