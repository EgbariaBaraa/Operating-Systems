
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

#define LIMIT 1e8
#define SBRK_LIMIT (128*1024)
#define MAX_ORDER 11
#define MIN_BLOCK_SIZE 128
#define INIT_BLOCK_NUM 32
#define INIT_BLOCK_SIZE (128*1024)
#define ALIGNMENT (32*128*1024)
#define FAILURE ((void*)(-1))
#define is ==
#define isnot !=
#define returned ==

/**
 * @class MallocMetaData
 * @abstract type that holds the MetaData of the allocated block in our program.
 * @field cookie : random value that insures the corectness of the MetaData;
 * @field size : the size of the allocated block - without the MetaData block;
 * @field isFree : is the block free or is it allocated.
 * @field next : pointer to the next block in increasing addresses order.
 * @field prev : pointer to the prev block in increasing addresses order.
 * @field nextFree : pointer to the next free block in OrdersTable.
 * @field prevFree : pointer to the prev free block in OrdersTable.
 */
class MallocMetaData
{
public:
    int cookie;
    size_t size;
    bool is_free;
    MallocMetaData *next;
    MallocMetaData *prev;
    MallocMetaData *next_free;
    MallocMetaData *prev_free;
    void updateSize(size_t new_size) {
        size = new_size;
    }
    void InitMetaData(size_t size, size_t used, int cookie, MallocMetaData *prev = nullptr, MallocMetaData *next = nullptr)
    {
        this->cookie = cookie;
        this->is_free = true;
        this->size = size;
        this->prev = this->prev_free = prev;
        this->next = this->next_free = next;
    }
};


MallocMetaData* List = nullptr;
MallocMetaData* OrdersTable[MAX_ORDER] = {nullptr};
MallocMetaData* MmapedList = nullptr;
int metaDataSize = sizeof(MallocMetaData);
int blocks = 0;
int free_blocks = 0;
int free_bytes = 0;
int allocated_bytes = 0;
int cookie = 0;
bool first_allocation = true;

/** @abstract helpers */
static int findOrder(size_t size);
static size_t calculateSizeAfterSplitting(size_t size);
static MallocMetaData* getBuddyAddress(MallocMetaData* block);
static void checkBrokenCookie(MallocMetaData* block);
static void searchBrokenCookies(MallocMetaData* List);
static void* InitAllocator();
static void* insertInMmapedList(size_t size);
static void removeFromMmapedList(MallocMetaData* block);
static void insertToOrdersTable(int order, MallocMetaData* to_add);
static int splitFreeBlockInHalf(MallocMetaData* block, int entry);
static void* removeHeadFromOrdersTable(int order);
static void* findSuitableBlock(size_t size);
static void removeBlockFromOrdersList(MallocMetaData* block);
static MallocMetaData* mergeBuddies(MallocMetaData* block);
static void addFreedBlock(MallocMetaData* block);
MallocMetaData* mergeBuddiesUntilBigEnough(MallocMetaData* block, size_t size);
static void* sreallocEdgeCaseB(void* oldp, size_t size);
static void* sreallocEdgeCaseC(void* oldp, size_t size);
static void* sreallocEdgeCases(void* oldp, size_t size);

/** @abstract API */
void* smalloc(size_t size);
void* scalloc(size_t num, size_t size);
void sfree(void* p);
void* srealloc(void *oldp, size_t size);
size_t _num_free_blocks();
size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _num_meta_data_bytes();
size_t _size_meta_data();


/**
 * @function findOrder
 * @abstract finds the order (entry) in OrdersTable for a given a block.
 * @param size : the size of the block.
 * @return : the order of the block.
*/
static int findOrder(size_t size)
{
    size_t number = size/MIN_BLOCK_SIZE;
    int counter = 0;
    while(number >= 2)
    {
        counter++;
        number /= 2;
    }
    return counter;
}

/**
 * @function calculateSizeAfterSplitting
 * @abstract calculate the size of the new blocks that are created when a block is splitted.
 * @param size : the size of the old block.
 * @return : the new size.
*/
static size_t calculateSizeAfterSplitting(size_t size)
{
    return (size + metaDataSize)/2 - metaDataSize;
}

/**
 * @function getBuddyAddress
 * @abstract ;
 * @param block :
 * @return :
*/
static MallocMetaData* getBuddyAddress(MallocMetaData* block)
{
    return (MallocMetaData*) ((intptr_t) block ^ (block->size+metaDataSize));
}


/**
 * @function checkBrokenCookie
 * @abstract checks if the MetaData of the a givrn block has been manipulated;
 * @param block ;
*/
static void checkBrokenCookie(MallocMetaData* block)
{
    if(block->cookie != cookie)
        exit(0xdeadbeef);
}

/**
 * @function searchBrokenCookies
 * @abstract looks for a MetaData that has been manipulated in the List.
 * @param list : List of Blocks.
*/
static void searchBrokenCookies(MallocMetaData* list)
{
    for(MallocMetaData* it = list; it != nullptr; it = it->next)
        checkBrokenCookie(it);
}

/**
 * @function InitAllocator
 * @abstract Initialize our buddy Allocator by creating 32 Initial blocks, each of size 128kb.
 * and then adding them to  the last entry of the OrdersTable.
 * plus, here we first allign the break pointer to 32 x 128kb so we can use the trick for calculating the buddy block.
 * @return FAILURE upon failing to increase the break pointer;
 * @return NULLPTR upon successful initialization.
*/
static void* InitAllocator()
{
    cookie = rand();
    first_allocation = false;
    size_t address = ((intptr_t)sbrk(0)) % ALIGNMENT;
    address = ALIGNMENT - address;
    if(sbrk(address) is FAILURE)
        return FAILURE;
    void* ret = sbrk(INIT_BLOCK_NUM*INIT_BLOCK_SIZE);
    if(ret is FAILURE)
        return FAILURE;
    List = (MallocMetaData*) ret;
    MallocMetaData* it = List;
    for(int i=0; i<INIT_BLOCK_NUM; i++)
    {
		MallocMetaData* next = nullptr, *prev = nullptr;
		if(i != 0)
			prev = (MallocMetaData*) ((char*)it-INIT_BLOCK_SIZE);
        if(i != INIT_BLOCK_NUM-1)
			next = (MallocMetaData*) ((char*)it+INIT_BLOCK_SIZE);
        int size = INIT_BLOCK_SIZE - metaDataSize;
        it->InitMetaData(size, 0, cookie, prev, next);
        it = it->next;
    }
 
    OrdersTable[MAX_ORDER-1] = List;
    blocks = free_blocks = INIT_BLOCK_NUM;
    allocated_bytes = free_bytes = INIT_BLOCK_NUM*INIT_BLOCK_SIZE - INIT_BLOCK_NUM*metaDataSize;
    return nullptr;
}

/**
 * @function insertInMmapedList
 * @abstract inserts a new block to the systems using mmap (size  >= 128kb)
 * @param size :
 * @return FAILURE if mmap failed;
 * @return a Pointer to the new allocated (mmaped) block upon success;
*/
static void* insertInMmapedList(size_t size)
{
    void* ptr = nullptr;
    if(MmapedList == NULL)
    {
        ptr = mmap(NULL, size+sizeof(MallocMetaData), PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(ptr == FAILURE)
            return FAILURE;
        MmapedList = (MallocMetaData*) ptr;
        MmapedList->InitMetaData(size, size, cookie);
        MmapedList->is_free = false;
        blocks++;
        allocated_bytes += size;
        return (void*)(MmapedList+1);
    }

    MallocMetaData* prev = MmapedList;
    while(prev->next != nullptr)
        prev = prev->next;

    ptr = mmap(NULL, size+sizeof(MallocMetaData), PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(ptr == FAILURE)
        return FAILURE;
    MallocMetaData* tmp = (MallocMetaData*) ptr;
    tmp->InitMetaData(size, size, cookie, prev);
    tmp->is_free = false;
    prev->next = tmp;
    blocks++;
    allocated_bytes += size;
    return (void*)(tmp + 1);
}

/**
 * @function removeFromMmapedList
 * @abstract : removes a block that has been mmaped from MmapedList;
 * @param block : the block to remove;
*/
static void removeFromMmapedList(MallocMetaData* block)
{
    if(block == MmapedList)
    {
        MmapedList = MmapedList->next;
        if(MmapedList)
            MmapedList->prev = nullptr;
        size_t size =  block->size;
        if(munmap(block, block->size + sizeof(MallocMetaData)) != 0)
            return;
        blocks--;
        allocated_bytes-= size;
        return;
    }

    if(block->next == nullptr)
    {
        block->prev->next = nullptr;
        size_t size =  block->size;
        if(munmap(block, block->size + sizeof(MallocMetaData)) != 0)
            return;
        blocks--;
        allocated_bytes -= size;
        return;
    }
    block->next->prev = block->prev;
    block->prev->next = block->next;
    size_t size =  block->size;
    if(munmap(block, block->size + sizeof(MallocMetaData)) != 0)
        return;
    blocks--;
    allocated_bytes-= size;
    return;
}

/**
 * @function insertToOrdersTable
 * @abstract : inserts a given block to the OrdersTable in entry "order";
 * @param order : the entry in OrdersTable;
 * @param to_add : the block to be inserted;
*/
static void insertToOrdersTable(int order, MallocMetaData* to_add)
{
    MallocMetaData* it = nullptr, *prev = nullptr;
    MallocMetaData*& head = OrdersTable[order];
    if(head is nullptr)
    {
        head = to_add;
        return;
    }
    for(it = head; it != nullptr; it = it->next_free)
    {
        if( (void*) it > (void*) to_add)
            break;
        prev = it;
    }
    if(prev is nullptr)
    {
        to_add->next_free = head;
        to_add->prev_free = nullptr;
        head->prev_free = to_add;
        head = to_add;
    }
    else
    {
        to_add->next_free = prev->next_free;
        to_add->prev_free = prev;
        prev->next_free = to_add;
    }
}

/**
 * @function splitFreeBlockInHalf
 * @abstract : splits a given a free block to two free block with the same size;
 * @param block : the block that needs be splitted.
 * @param entry : the suitable entry in OrdersTable for the block.
 * @return : the new order of the new blocks;
*/
static int splitFreeBlockInHalf(MallocMetaData* block, int entry)
{
    OrdersTable[entry] = block->next_free; // update the head of the list
    if(OrdersTable[entry] != nullptr)
        OrdersTable[entry]->prev_free = nullptr;
    size_t new_size = calculateSizeAfterSplitting(block->size);
    block->updateSize(new_size); // block->size = new_size;
    block->next_free = nullptr;
    block->prev_free = nullptr;
    // create a new block by adding the offset to the current pointer;
    MallocMetaData* new_block = (MallocMetaData*)((char*) block + new_size + metaDataSize);
    new_block->InitMetaData(new_size, 0, cookie, block, block->next);
    new_block->next_free = nullptr;
    new_block->prev_free = nullptr;
    if(block->next != nullptr) // is not the tail of the list;
		block->next->prev = new_block;
    block->next = new_block;


    int order = findOrder(new_size + metaDataSize); // new order in Orders Table
    insertToOrdersTable(order, new_block);
    insertToOrdersTable(order, block);

    // update the global variables;
    free_blocks++;
    blocks++;
    free_bytes -= metaDataSize;
    allocated_bytes -= metaDataSize;
    return order;
}

/**
 * @function removeHeadFromOrdersTable
 * @abstract removes the head of the List in OrdersTable[order]
 * @param order : the entry in OrdersTable
 * @return : pointer to the head removed;
*/
static void* removeHeadFromOrdersTable(int order)
{
    MallocMetaData*& head = OrdersTable[order];
    MallocMetaData* block = head;
    head = head->next_free;
    if(head != nullptr)
        head->prev_free = nullptr;
    block->next_free = block->prev_free = nullptr;
    block->is_free = false;
    free_blocks--;
    free_bytes -= block->size;
    return (void*) (block+1);
}

/**
 * @function findSuitableBlock
 * @abstract ;
 * @param size :
 * @return :
*/
static void* findSuitableBlock(size_t size)
{
    int order = findOrder((int) size+metaDataSize);
    for(; order<MAX_ORDER; order++) // find the minimal block that need to split
        if(OrdersTable[order] != nullptr)
            break;
    if(order is MAX_ORDER)
		return FAILURE;
    MallocMetaData* head = OrdersTable[order];
    size_t new_size = calculateSizeAfterSplitting(head->size);
    while(order>0 && new_size >= size)
    {
        order = splitFreeBlockInHalf(head, order);
        head = OrdersTable[order];
        new_size = calculateSizeAfterSplitting(head->size);
    }
    return removeHeadFromOrdersTable(order);
}

/**
 * @function removeBlockFromOrdersList
 * @abstract ;
 * @param block :
*/
static void removeBlockFromOrdersList(MallocMetaData* block)
{
    int order = findOrder((int) block->size + metaDataSize);
    if(block->prev_free is nullptr) // head of list
    {
        OrdersTable[order] = block->next_free;
        if(OrdersTable[order] != nullptr)
            OrdersTable[order]->prev_free = nullptr;
    }
    else if(block->next_free is nullptr) // tail of list (can't be head of list)
        block->prev_free->next_free = nullptr;
    else
    {
        block->prev_free->next_free = block->next_free;
        block->next_free->prev_free = block->prev_free;
    }
    block->next_free = nullptr;
    block->prev_free = nullptr;
}

/**
 * @function isLegalToMerge
 * @abstract ;
 * @param block :
 * @param buddy :
 * @return :
*/
static bool isLegalToMerge(MallocMetaData* block, MallocMetaData* buddy)
{
    return not ((block->size+metaDataSize) is INIT_BLOCK_SIZE || buddy->is_free is false || block->size != buddy->size);
}

/**
 * @function mergeBlocksHelper
 * @abstract : update the fields of the block we wanna merge into
 * @return the address ot the new merged block
*/
static MallocMetaData* mergeBlocksHelper(MallocMetaData* block, MallocMetaData* buddy)
{
    if(block < buddy) // merge buddy into block;
    {
        removeBlockFromOrdersList(buddy);
        block->updateSize(block->size+buddy->size+metaDataSize);
        block->next = buddy->next;
        if(block->next isnot nullptr)
            block->next->prev = block;
    }
    else
    {
        removeBlockFromOrdersList(buddy); // merge block into buddy
        buddy->updateSize(buddy->size + block->size+metaDataSize);
        buddy->next = block->next;
        if(buddy->next isnot nullptr)
            buddy->next->prev = buddy;
        block = buddy;
    }
    return block;
}


/**
 * @function mergeBuddies
 * @abstract : merges two free blocks into one block recursivley until no possible merges left.
 * @param block : the first block
*/
static MallocMetaData* mergeBuddies(MallocMetaData* block)
{
    MallocMetaData* buddy = getBuddyAddress(block);
    while(isLegalToMerge(block, buddy) returned true)
    {
        block = mergeBlocksHelper(block, buddy);
        blocks--;
        free_blocks--;
        free_bytes += metaDataSize;
        allocated_bytes += metaDataSize;
        buddy = getBuddyAddress(block);
    }
    return block;
}

/**
 * @function addFreedBlock
 * @abstract frees a block and adds it to the system
 * @param block : the block we wanna add
*/
static void addFreedBlock(MallocMetaData* block)
{
    block->is_free = true;
    free_blocks++;
    free_bytes += block->size;
    block = mergeBuddies(block);
    int order = findOrder((int) block->size + metaDataSize);
    insertToOrdersTable(order, block);
}


/**
 * @function mergeBuddiesUntilBigEnough
 * @abstract helper for Case B, merges up to the block we wanna allocate if found.
 * @param block : the block we wanna start merging from
*/
MallocMetaData* mergeBuddiesUntilBigEnough(MallocMetaData* block, size_t size)
{
    MallocMetaData* buddy = getBuddyAddress(block);
    while(block->size < size)
    {
        if(isLegalToMerge(block, buddy) returned false)
            break;
        block = mergeBlocksHelper(block, buddy);
        buddy = getBuddyAddress(block);
        blocks--;
        free_blocks--;
        free_bytes += metaDataSize;
        allocated_bytes += metaDataSize;
    }
    return block;
}

/**
 * @function sreallocEdgeCaseB
 * @abstract searches for a block in the hierarchy that can handle the request. if exist, we free the old one, and merge up to that block.
 * @param oldp : pointer to the old block
 * @param size : the size of the new block
*/
static void* sreallocEdgeCaseB(void* oldp, size_t size)
{
    MallocMetaData* block = (MallocMetaData*) oldp - 1;
    MallocMetaData* tmp = block;
    MallocMetaData* buddy = getBuddyAddress(block);
    size_t block_size = tmp->size;
    while(block_size < size)
    {
        if(isLegalToMerge(tmp, buddy) returned false)
            break;

        if(tmp < buddy) // merge buddy into block;
            block_size += buddy->size+metaDataSize;
        else
        {
            block_size += tmp->size + metaDataSize;
            tmp = buddy;
        }
        buddy = getBuddyAddress(tmp);
    }
    if(block_size < size)
        return FAILURE;
    block->is_free = true;
    free_blocks++;
    free_bytes += block->size;
    block = mergeBuddiesUntilBigEnough(block, size);
    free_bytes -= block->size;
    free_blocks--;
    block->is_free = false;
    memmove((void*) (block+1), oldp, size);
    return (void*) (block+1);
}

/**
 * @function sreallocEdgeCaseC
 * @abstract allocate a new block and copies the old data into it;
 * @param oldp : pointer to the old block
 * @param size : the size of the new block
*/
static void* sreallocEdgeCaseC(void* oldp, size_t size)
{
    sfree(oldp);
    void* new_block = smalloc(size);
    memmove(new_block, oldp, size);
    return new_block;
}

/**
 * @function sreallocEdgeCases
 * @abstract handle the edge cases of srealloc, if a new block of the current block hierarchy can be used, or we should allocate a new one
 * @param oldp : pointer to the old block
 * @param size : the size of the new block
*/
static void* sreallocEdgeCases(void* oldp, size_t size)
{
    void* new_block = sreallocEdgeCaseB(oldp, size);
    if(new_block != FAILURE)
        return new_block;
    new_block = sreallocEdgeCaseC(oldp, size);
    return new_block;
}


/**
 * @function smalloc
 * @abstract allocates the smallest block possible with the required size (best-fit);
 *  if a pre-allocated block is reused and is "large enough", smalloc will cut the block into two blocks,
 *  one will serve the current allocation, and the other will remain unused for later.
 *  this process is done iteratively until the block cannot be cut into two.
 * @param size : the size of the desired block
 * @return a pointer to the first byte in the allocated block upon success.
 * @return NULL upon failure.
*/
void* smalloc(size_t size)
{
    if(first_allocation)
        if(InitAllocator() returned FAILURE)
            return NULL;
    if(size <= 0 or size > LIMIT)
        return NULL;

    searchBrokenCookies(MmapedList);
    searchBrokenCookies(List);

    if(size >= SBRK_LIMIT)
        return insertInMmapedList(size);;

    void* block = findSuitableBlock(size);
    if(block is FAILURE)
        return NULL;
    return block;
}

/**
 * @function scalloc
 * @abstract searches for a free block of at least ‘num’ elements, each ‘size’ bytes that are all set to 0,
 *  or allocates if none are found. In other words, find/allocate size num bytes and set all bytes to 0.
 * @param num : the number of elements.
 * @param size : the size of each element
 * @return a pointer to the first byte in the allocated block upon success.
 * @return NULL upon failure.
*/
void* scalloc(size_t num, size_t size)
{
    if(size <= 0 or num <= 0 or num*size > LIMIT)
        return NULL;
    void* ret = smalloc(num*size);
    if(ret is NULL)
        return NULL;
    memset(ret, 0, num*size);
    return ret;
}

/**
 * @function sfree
 * @abstract frees the allocated memory. if the buddy of the free'd block is also free, we merge them into one free block.
 *  this is done recursivly until we can't merge blovks anymore.
 * @param p : pointer to the previously allocated block.
*/
void sfree(void* p)
{
    if(p is nullptr)
        return;

    searchBrokenCookies(MmapedList);
    searchBrokenCookies(List);

    MallocMetaData* ptr = ((MallocMetaData*)p - 1);
    if(ptr->cookie != cookie)
        exit(0xdeadbeef);
    else if(ptr->is_free == true)
        return;
    if(ptr->size >= SBRK_LIMIT)
        removeFromMmapedList(ptr);
    else
        addFreedBlock(ptr);
}

/**
 * @function srealloc
 * @abstract If ‘size’ is smaller than or equal to the current block’s size, reuses the same block.
 *  Otherwise, finds/allocates ‘size’ bytes for a new space, copies content of oldp into the new allocated space and frees the oldp.
 * @param oldp : pointer to the block we want to realocate
 * @param size : new size of the block.
*/
void* srealloc(void *oldp, size_t size)
{
    if (size == 0 || size > LIMIT)
        return NULL;

    if (oldp is NULL)
        return smalloc(size);

    MallocMetaData* ptr = (MallocMetaData*) oldp-1;
    if (ptr->cookie != cookie)
        exit(0xdeadbeef);

    if (size >= SBRK_LIMIT) // mmaped area
    {
        if (ptr->size is size)
           return oldp;
        else if(ptr->size != size)
        {
            removeFromMmapedList(ptr);
            return insertInMmapedList(size);
        }
    }
    else
    {
        if (ptr->size >= size)
            return oldp;
        return sreallocEdgeCases(oldp, size);
    }
    return nullptr;
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

