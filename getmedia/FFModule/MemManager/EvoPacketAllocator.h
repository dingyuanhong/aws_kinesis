#ifndef _EVO_PACKET_ALLOCATOR_H__
#define _EVO_PACKET_ALLOCATOR_H__

#include "EvoPacket.h"

class EvoPacketAllocator {
public:
	static EvoPacket* CreateNew(uint32_t dataSize, EvoPacket::TypeEnum type);
	static EvoPacket* CreateNew(uint32_t width, uint32_t height, AVPixelFormat format, EvoPacket::TypeEnum type);
	static EvoPacket* CreateNew(int nb_channels,int nb_samples, enum AVSampleFormat sample_fmt, int align, EvoPacket::TypeEnum type);

	static EvoPacket* CreateMemory(uint32_t dataSize) 
	{
		return CreateNew(dataSize, EvoPacket::TYPE_MEMORY);
	}

	static EvoPacket* CreateAVFrame(uint32_t width, uint32_t height, AVPixelFormat format)
	{
		{
			return CreateNew(width,height,format, EvoPacket::TYPE_AVFRAME);
		}
	}

	static EvoPacket* CreateAVFrame(int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align)
	{
		{
			return CreateNew(nb_channels, nb_samples, sample_fmt, align, EvoPacket::TYPE_AVFRAME);
		}
	}
	
	static void Delete(EvoPacket **packet);

	static void DeleteMemory(void *mem, bool bCache);

//通过内存池申请内存功能
	//创建内存块
	static EvoPacket* PoolNew(uint32_t dataSize);
	//创建视频内存块
	static EvoPacket* PoolNew(uint32_t width, uint32_t height, AVPixelFormat format);
	//创建音频内存块
	static EvoPacket* PoolNew(int channels, int samples, enum AVSampleFormat format, int align);
	//释放
	static void PoolDelete(EvoPacket **packet);
};

#endif//_EVO_PACKET_ALLOCATOR_H__
