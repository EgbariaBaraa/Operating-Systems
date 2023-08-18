
#include <iostream> 
#include <unistd.h>

#define LIMIT 1e8

void* smalloc(size_t size)
{
	if(size <= 0 or size > LIMIT)
		return NULL;
	void* ret = (int*) sbrk(size);
	if(ret == (void*) -1)
		return NULL;
	return ret;
}
