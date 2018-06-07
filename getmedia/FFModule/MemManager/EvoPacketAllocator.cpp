#include "EvoPacketAllocator.h"

#include "EvoAVFrame.h"
#include "EvoMemory.h"
#include "EvoAttributeMemory.h"
#include "PacketPool.h"

EvoPacket* EvoPacketAllocator::CreateNew(uint32_t dataSize, EvoPacket::TypeEnum type)
{
	if (dataSize == 0)
		return nullptr;
	if (type != EvoPacket::TypeEnum::TYPE_MEMORY)
		return nullptr;
	EvoMemory* evoMemory = NULL;
	if (type == EvoPacket::TypeEnum::TYPE_MEMORY)
	{
		evoMemory = new EvoMemory;
	}
	if (!evoMemory)
		goto _FAILED_;

	evoMemory->PacketData = new uint8_t[dataSize];
	if (!evoMemory->PacketData)
		goto _FAILED_;
	memset(evoMemory->PacketData, 0, dataSize);

	evoMemory->DataSize = dataSize;
	evoMemory->PacketType = type;

	return evoMemory;

_FAILED_:
	if (evoMemory)
		delete evoMemory;
	return nullptr;
}

EvoPacket* EvoPacketAllocator::CreateNew(uint32_t width, uint32_t height, AVPixelFormat format, EvoPacket::TypeEnum type)
{
	if (type != EvoPacket::TypeEnum::TYPE_AVFRAME)
		return nullptr;

	EvoAVFrame* evoFrame = new EvoAVFrame;
	if (!evoFrame)
		goto _FAILED_;

	evoFrame->PacketData = av_frame_alloc();
	if (!evoFrame->PacketData)
		goto _FAILED_;

	evoFrame->DataSize = av_image_alloc(evoFrame->PacketData->data, evoFrame->PacketData->linesize, width, height, format, 1);
	if (evoFrame->DataSize < 0)
		goto _FAILED_;

	evoFrame->PacketType = type;
	evoFrame->Type = EvoAVFrame::VIDEO_FRAME;
	evoFrame->Width = width;
	evoFrame->Height = height;
	evoFrame->VFormat = format;

	return evoFrame;

_FAILED_:
	if (evoFrame)
		delete evoFrame;
	return nullptr;
}

EvoPacket* EvoPacketAllocator::CreateNew(int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align, EvoPacket::TypeEnum type)
{
	if (type != EvoPacket::TypeEnum::TYPE_AVFRAME)
		return nullptr;

	EvoAVFrame* evoFrame = new EvoAVFrame;
	if (!evoFrame)
		goto _FAILED_;

	evoFrame->PacketData = av_frame_alloc();
	if (!evoFrame->PacketData)
		goto _FAILED_;

	evoFrame->DataSize = av_samples_alloc(evoFrame->PacketData->data, evoFrame->PacketData->linesize, 
		nb_channels, nb_samples, sample_fmt, align);
	if (evoFrame->DataSize < 0)
		goto _FAILED_;

	evoFrame->PacketType = type;
	evoFrame->Type = EvoAVFrame::AUDIO_FRAME;
	evoFrame->Channel = nb_channels;
	evoFrame->SampleRate = nb_samples;
	evoFrame->AFormat = sample_fmt;

	return evoFrame;

_FAILED_:
	if (evoFrame)
		delete evoFrame;
	return nullptr;
}

void EvoPacketAllocator::DeleteMemory(void *mem, bool bCache)
{
	if (mem != NULL)
	{
// 			if (bCache)
// 				av_free(mem);
// 			else
		av_free(mem);
	}
}

void EvoPacketAllocator::Delete(EvoPacket **packet)
{
	if (packet == NULL) return;
	if (*packet == NULL) return;
	delete *packet;
	*packet = NULL;
}

//创建内存块
EvoPacket* EvoPacketAllocator::PoolNew(uint32_t dataSize)
{
	PacketPool * pool = PacketPool::GetInstance(PACKET_LIST_MAX_SIZE);
	EvoPacket * packet = NULL;
	if (pool != NULL)
	{
		packet = pool->GetPacket(EvoMemory::GetTag(dataSize), EvoPacket::TypeEnum::TYPE_MEMORY, dataSize);
		if (packet != NULL)
		{
			packet->Validate();
		}
	}
	if (packet == NULL)
	{
		return CreateMemory(dataSize);
	}
	return packet;
}

//创建视频内存块
EvoPacket* EvoPacketAllocator::PoolNew(uint32_t width, uint32_t height, AVPixelFormat format)
{
	PacketPool * pool = PacketPool::GetInstance(PACKET_LIST_MAX_SIZE);
	EvoPacket * packet = NULL;
	if (pool != NULL)
	{
		int DataSize = av_image_get_buffer_size(format, width, height , 1);
		packet = pool->GetPacket(EvoAVFrame::GetTag(width,height,format), EvoPacket::TypeEnum::TYPE_AVFRAME, DataSize);
		if (packet != NULL)
		{
			packet->Validate();
		}
	}
	if (packet == NULL)
	{
		return CreateAVFrame(width,height,format);
	}
	return packet;
}

//创建音频内存块
EvoPacket* EvoPacketAllocator::PoolNew(int channels, int samples, enum AVSampleFormat format, int align)
{
	PacketPool * pool = PacketPool::GetInstance(PACKET_LIST_MAX_SIZE);
	EvoPacket * packet = NULL;
	if (pool != NULL)
	{
		int DataSize = av_samples_get_buffer_size(NULL, channels, samples, format, align);
		packet = pool->GetPacket(EvoAVFrame::GetTag(channels, samples, format), EvoPacket::TypeEnum::TYPE_AVFRAME, DataSize);
		if (packet != NULL)
		{
			packet->Validate();
		}
	}
	if (packet == NULL)
	{
		return CreateAVFrame(channels, samples, format, align);
	}
	return packet;
}

//释放包
void EvoPacketAllocator::PoolDelete(EvoPacket **packet)
{
	PacketPool * pool = PacketPool::GetInstance(PACKET_LIST_MAX_SIZE);
	if(packet != NULL){
		if (*packet == NULL) return;
		if (pool != NULL)
		{
			pool->PutPacket(packet);
		}
		else
		{
			Delete(packet);
		}
	}
}