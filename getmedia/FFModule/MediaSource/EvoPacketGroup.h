#ifndef EVOPACKETGROUP_H
#define EVOPACKETGROUP_H

#include <array>
#include "MemManager/EvoPacketAllocator.h"

template<class T>
class EvoPacketGroup {
public:
	EvoPacketGroup(uint32_t count)
	{
		Arr = (T**)new char*[count];
		Count = count;
		Empty();
	}

	EvoPacketGroup(T ** data, uint32_t count)
	{
		Arr = (T**)new char*[count];
		Count = count;
		Empty();
	}

	~EvoPacketGroup()
	{
		Empty();
		Clear();
	}

	void Clear()
	{
		if (Arr != NULL)
		{
			delete[]Arr;
		}
		Arr = NULL;
	}

	void Empty()
	{
		if (Arr != NULL)
		{
			memset(Arr, 0, sizeof(T*)*Count);
		}
	}

	int GetCount()
	{
		return (int)Count;
	}

	int Set(uint32_t index, T* data)
	{
		if (index > Count) return -1;
		Arr[index] = data;
		return 0;
	}

	T* Get(uint32_t index)
	{
		if (index > Count) return NULL;
		return Arr[index];
	}

	T*& operator[](uint32_t _Pos)
	{
		_Analysis_assume_(_Pos < Count);

		return Arr[_Pos];
	}
private:
	T** Arr;
	uint32_t Count;
};

template<class T>
void DeletePacketGroup(EvoPacketGroup<T> *group, bool bCache = false)
{
	if (group == NULL) return;
	int nCount = group->GetCount();
	for (int i = 0; i < nCount; i++)
	{
		T * t = group->Get(i);
		if (t != NULL)
		{
			if (bCache) {
				EvoPacketAllocator::PoolDelete(&t);
			}
			else
			{
				delete t;
			}
			group->Set(i, NULL);
		}
	}
	delete group;
}

template<class T>
void EmptyPacketGroup(EvoPacketGroup<T> *group, bool bCache = false)
{
	if (group == NULL) return;
	int nCount = group->GetCount();
	for (int i = 0; i < nCount; i++)
	{
		T * t = group->Get(i);
		if (t != NULL)
		{
			if (bCache) {
				EvoPacketAllocator::PoolDelete((EvoPacket**)&t);
			}
			else
			{
				delete t;
			}
			group->Set(i, NULL);
		}
	}
}

#endif