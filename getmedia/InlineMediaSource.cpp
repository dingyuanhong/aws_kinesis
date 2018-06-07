#include "InlineMediaSource.h"


InlineMediaSource::InlineMediaSource()
{
	event_ = NULL;
	stop = 0;
	MemroryBuffer::Init(&buffer, 10 * 1024 * 1024);
	uv_sem_init(&sem_data, 0);
	uv_sem_init(&sem_play, 0);
}

InlineMediaSource::~InlineMediaSource()
{
	Stop();

	lock.lock();
	MemroryBuffer::Free(&buffer);
	lock.unlock();
}

int InlineMediaSource::Start(Aws::String name, std::shared_ptr<Aws::Auth::AWSCredentialsProvider> cre)
{

	Stop();

	stream = name;
	provider = cre;

	stop = 0;
	uv_thread_create(&thread_play, InlineMediaSource::ThreadPlay_, this);
	uv_thread_create(&thread_inline, InlineMediaSource::ThreadInline_, this);

	return 0;
}

int InlineMediaSource::Stop()
{
	stop = 1;
	uv_sem_post(&sem_data);
	uv_sem_post(&sem_play);
	uv_thread_join(&thread_play);
	uv_thread_join(&thread_inline);
	thread_play = NULL;
	thread_inline = NULL;

	lock.lock();
	MemroryBuffer::Clear(&buffer);
	lock.unlock();
	return 0;
}


void InlineMediaSource::ThreadInline()
{
	Aws::String streamName = stream;

	Aws::Client::ClientConfiguration config = GetDefaultConfigure(provider, streamName);

	while (!stop)
	{
		if (event_ != NULL) event_->onBeginGetMedia();
		int ret = GetMediaData(provider, config, streamName, this);
		if (event_ != NULL) event_->onEndGetMedia(ret);

		uv_sem_post(&sem_play);
	}
}

void InlineMediaSource::DataReceivedEventHandler(const Aws::Http::HttpRequest* request, Aws::Http::HttpResponse* response, long long read)
{
	if (stop)
	{
		return;
	}

	Aws::IOStream& io = response->GetResponseBody();
	std::streamsize size = read;

	while (size > 0)
	{
		lock.lock();
		{
			int nwrite = 0;
			char * ptr = MemroryBuffer::write_ptr(&buffer, nwrite);
			if (nwrite > 0)
			{
				int copy = min(nwrite, size);
				io.read(ptr, copy);
				size -= copy;
				MemroryBuffer::write(&buffer, copy);


			}
		}
		lock.unlock();
	}

	uv_sem_post(&sem_data);
}

bool InlineMediaSource::ContinueHandler(const Aws::Http::HttpRequest* request)
{
	return stop == 0 ? true : false;
}

void InlineMediaSource::ThreadOpen()
{
	while (!stop)
	{
		if (event_ != NULL) event_->onBeginMedia();

		int ret = Open(InlineMediaSource::CallbackRead_, this, 10 * 1024 * 1024);
		if (ret == 0)
		{
			if (event_ != NULL) event_->onOpenMedia(this);

			while (!stop)
			{
				ret = ReadFrame();
				if (ret == 0)
				{

				}
				else
				{
					break;
				}
			}

			if (event_ != NULL) event_->onEndMedia(0);
		}
		else
		{
			if (event_ != NULL) event_->onEndMedia(ret);
		}
		uv_sem_wait(&sem_play);
	}
}

int InlineMediaSource::Fill(uint8_t *buf, int buf_size)
{
	if (stop)
	{
		return -1;
	}

	int copy = 0;

	while (!stop)
	{
		lock.lock();
		{
			int nread = 0;
			char * ptr = MemroryBuffer::read_ptr(&buffer, nread);
			if (nread > 0)
			{
				copy = min(buf_size, nread);
				memcpy(buf, ptr, copy);
				MemroryBuffer::read(&buffer, copy);
			}
		}
		lock.unlock();

		if (copy == 0) uv_sem_wait(&sem_data);
		else break;
	}
	return copy;
}