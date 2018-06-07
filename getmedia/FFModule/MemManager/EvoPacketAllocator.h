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

//ͨ���ڴ�������ڴ湦��
	//�����ڴ��
	static EvoPacket* PoolNew(uint32_t dataSize);
	//������Ƶ�ڴ��
	static EvoPacket* PoolNew(uint32_t width, uint32_t height, AVPixelFormat format);
	//������Ƶ�ڴ��
	static EvoPacket* PoolNew(int channels, int samples, enum AVSampleFormat format, int align);
	//�ͷ�
	static void PoolDelete(EvoPacket **packet);
};

#endif//_EVO_PACKET_ALLOCATOR_H__
