#pragma once

#include "FFheader.h"
#include "MediaSource\MediaSourceBase.h"
#include "core/lock.h"
#include "core/log.h"
#include "core/thread.h"

#include "memoryBuffer.h"
#include "GetMediaDataProcess.h"

class FFMPEGGetMediaDataProcess
	: public GetMediaDataProcess
	, public MediaReciever
{
public:
	FFMPEGGetMediaDataProcess()
	{
		open_success = true;
		stop = 0;
		MemroryBuffer::Init(&buffer, 10 * 1024 * 1024);

		source = new EvoMediaSourceBase();
		source->SetCallback(this);

		uv_sem_init(&sem, 0);
	}

	~FFMPEGGetMediaDataProcess()
	{
		Stop();
		delete source;
		source = NULL;
		lock.lock();
		MemroryBuffer::Free(&buffer);
		lock.unlock();
	}

	void DataReceivedEventHandler(const Aws::Http::HttpRequest* request, Aws::Http::HttpResponse* response, long long read)
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

		uv_sem_post(&sem);

		//std::cout << read << std::endl;
	}

	bool ContinueHandler(const Aws::Http::HttpRequest* request)
	{
		if (open_success == false) return false;
		return stop == 0 ? true : false;
	}

	virtual void onMediaData(AVPacket * packet)
	{
		LOGD("read : %lld %lld\n", packet->pts, packet->dts);
	}

	int Start()
	{
		open_success = true;
		uv_thread_create(&thread, FFMPEGGetMediaDataProcess::ThreadPlay_, this);
		return 0;
	}

	int Stop()
	{
		stop = 1;
		uv_sem_post(&sem);
		uv_thread_join(&thread);
		return 0;
	}

	static void ThreadPlay_(void* arg)
	{
		FFMPEGGetMediaDataProcess *thiz = (FFMPEGGetMediaDataProcess*)arg;
		thiz->ThreadPlay();
	}

	void ThreadPlay()
	{
		int ret = source->Open(FFMPEGGetMediaDataProcess::Read, this, 10 * 1024 * 1024);
		if (ret == 0)
		{
			while (!stop)
			{
				ret = source->ReadFrame();
				if (ret == 0)
				{

				}
				else
				{
					break;
				}
			}
		}
		else
		{
			LOGE("open failed:%d\n", ret);
			open_success = false;
		}
	}

	static int Read(void *opaque, uint8_t *buf, int buf_size)
	{
		FFMPEGGetMediaDataProcess *thiz = (FFMPEGGetMediaDataProcess*)opaque;
		return thiz->Fill(buf, buf_size);
	}

	int Fill(uint8_t *buf, int buf_size)
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

			if (copy == 0) uv_sem_wait(&sem);
			else break;
		}
		return copy;
	}
private:
	EvoMediaSourceBase * source;
	uv_sem_t sem;
	spin_mutex lock;
	uv_thread_t thread;
	MemroryBuffer::MemroryBuffer buffer;
	int stop;
	int open_success;
};