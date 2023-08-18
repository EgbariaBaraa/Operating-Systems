#include <iostream> 
#include <unistd.h>
#include <cstring>

#define LIMIT 1e8

    class MallocMetaData 
    {
      public:
        size_t size;
		bool is_free;
        MallocMetaData *next;
        MallocMetaData *prev;
    };

void FillMetaData(MallocMetaData* data, size_t size, MallocMetaData *ptr = nullptr, MallocMetaData *ptr2 = nullptr)
{
	data->size = size;
	data->is_free = false;
	data->prev = ptr;
	data->next = ptr2;
}


MallocMetaData* List = nullptr;
int blocks = 0;
int free_blocks = 0;
int free_bytes = 0;
int allocated_bytes = 0;
        

   void* insert(size_t size)
   {
		void* ret;
        if(List == NULL)
        {
			ret = sbrk(size+sizeof(MallocMetaData));
            if(ret == (void*) -1)
				return ret;
			List = (MallocMetaData*) ret;
			FillMetaData(List, size);
            blocks++;
            allocated_bytes += size;
            return (void*)(List+1);
        }
        
        MallocMetaData* prev = List;
        while( prev->next != nullptr )
        {
            prev = prev->next;
        }
        
        ret = sbrk(size+sizeof(MallocMetaData));
        if(ret == (void*) -1)
			return ret;
        prev->next = (MallocMetaData*) ret;
        FillMetaData(prev->next, size, prev);
        blocks++;      
        allocated_bytes += size;
        return (void*)(prev->next + 1);
    }

void* smalloc(size_t size)
{
	if(size <= 0 or size > LIMIT)
		return NULL;
		
	for(MallocMetaData* it = List; it != nullptr; it = it->next)
	{
		if(it->size >= size and it->is_free)
		{
			it->is_free = false;
			free_blocks--;
			free_bytes -= size;
			return (void*)(it+1);
		}
	}
	
	void* ret = insert(size);
	if(ret == (void*) -1)
		return NULL;
	return ret;
}


void* scalloc(size_t num, size_t size)
{
	if(size <= 0 or num <= 0 or num*size > LIMIT)
		return NULL;
		
	void* ret = smalloc(num*size);
	if(ret == (void*) -1 || ret == NULL)
		return NULL;
	memset(ret, 0, num*size);
	return ret;
}

void sfree(void* p)
{
	if(p == NULL)
		return;
	MallocMetaData* ptr = (MallocMetaData*)p;
	ptr -= 1;
	if(ptr->is_free == true)
		return;
	ptr->is_free = true;
	free_blocks++;
	free_bytes += ptr->size;
}

void* srealloc(void* oldp, size_t size)
{
	if(size == 0 || size > LIMIT)
		return NULL;
	if(oldp == NULL)
		return smalloc(size);
	MallocMetaData* ptr = (MallocMetaData*) oldp;
	ptr -= 1;
	if(ptr->size >= size)
		return oldp;
	void* ret = smalloc(size);
	if(ret == NULL || ret == (void*) -1)
		return NULL;
	std::memmove(ret, oldp, ptr->size);
	sfree(oldp);
	return ret;
	
}

size_t _num_free_blocks()
{
	return free_blocks;
}
	
size_t _num_free_bytes()
{
	return free_bytes;
}

size_t _num_allocated_blocks()
{
	return blocks;
}

size_t _num_allocated_bytes()
{
	return allocated_bytes;
}

size_t _num_meta_data_bytes()
{
	return sizeof(MallocMetaData)*(blocks);
}

size_t _size_meta_data()
{
	return sizeof(MallocMetaData);
}
