#pragma once

#include <stdlib.h>

namespace MemroryBuffer{

typedef struct MemroryBuffer
{
	char *data;
	int size;
	int r_index;
	int w_index;
	int full;
}MemroryBuffer;

inline void Init(MemroryBuffer * buffer, int size)
{
	buffer->data = (char*)malloc(size);
	buffer->size = size;
	buffer->r_index = 0;
	buffer->w_index = 0;
	buffer->full = 0;
}

inline void Free(MemroryBuffer * buffer)
{
	if (buffer->data != NULL) {
		free(buffer->data);
		buffer->data = NULL;
	}
}

inline void Clear(MemroryBuffer * buffer)
{
	buffer->r_index = 0;
	buffer->w_index = 0;
	buffer->full = 0;
}

inline int read_len(MemroryBuffer * buffer)
{
	if (buffer->w_index > buffer->r_index)
	{
		return buffer->w_index - buffer->r_index;
	}
	else if (buffer->w_index < buffer->r_index)
	{
		return buffer->size - buffer->r_index;
	}
	else if (buffer->full)
	{
		return buffer->size;
	}
	else {
		return 0;
	}
	return 0;
}

inline char * read_ptr(MemroryBuffer * buffer, int &nread)
{
	if (buffer->w_index > buffer->r_index)
	{
		nread = buffer->w_index - buffer->r_index;
	}
	else if (buffer->w_index < buffer->r_index)
	{
		nread = buffer->size - buffer->r_index;
	}
	else if (buffer->full)
	{
		nread = buffer->size - buffer->r_index;
	}
	else {
		nread = 0;
	}
	return buffer->data + buffer->r_index;
}

inline void read(MemroryBuffer * buffer, int size)
{
	buffer->r_index += size;
	if (buffer->r_index >= buffer->size)
	{
		buffer->r_index = 0;
	}
	if (buffer->w_index == buffer->r_index)
	{
		buffer->full = 0;
	}
	else
	{
		buffer->full = 1;
	}
}

inline int write_len(MemroryBuffer * buffer)
{
	if (buffer->w_index > buffer->r_index)
	{
		return buffer->size - buffer->w_index;
	}
	else if (buffer->w_index < buffer->r_index)
	{
		return buffer->r_index - buffer->w_index;
	}
	else if (buffer->full)
	{
		return 0;
	}
	else {
		return buffer->size;
	}
}

inline char * write_ptr(MemroryBuffer * buffer, int &nwrite)
{
	if (buffer->w_index > buffer->r_index)
	{
		nwrite = buffer->size - buffer->w_index;
	}
	else if (buffer->w_index < buffer->r_index)
	{
		nwrite = buffer->r_index - buffer->w_index;
	}
	else if (buffer->full)
	{
		nwrite = 0;
	}
	else {
		nwrite = buffer->size - buffer->w_index;
	}
	return buffer->data + buffer->w_index;
}

inline void write(MemroryBuffer * buffer, int size)
{
	buffer->w_index += size;
	if (buffer->w_index >= buffer->size)
	{
		buffer->w_index = 0;
	}
	if (buffer->w_index == buffer->r_index)
	{
		buffer->full = 1;
	}
	else
	{
		buffer->full = 0;
	}
}

}