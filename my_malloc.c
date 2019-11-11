#include "my_malloc.h"

typedef struct header {
	// this aligns the header to the most restrictive alignment on the platform
	// beware, this is C11
	struct header *next;
	size_t size;
	alignas(max_align_t)char free;
} header_t;


typedef struct arena {
	alignas(max_align_t) struct arena *next;
	size_t size;
} arena_t;

arena_t *first_arena = NULL;
// mmap returns page aligned memory, lets allocate in huge chunks and delegate parts of them
#define PAGE_SIZE (128*1024)
// smallest size of the block possible. (header + free memory that is atleast as big as the header size)
#define MIN_BLOCK_SIZE (sizeof(header_t) + alignof(max_align_t))
// align arena to the nearest multiply of PAGE_SIZE
#define ALIGN_ARENA(size)	\
	((size) % (PAGE_SIZE) ? (size) + (PAGE_SIZE) - (size) % (PAGE_SIZE) : (size))
// align block to the nearest multiply of header_t
#define ALIGN_BLOCK(size)	\
	((size) % (alignof(max_align_t)) ? (size) + (alignof(max_align_t)) - (size) % (alignof(max_align_t)) : (size))

/**
 * @brief Allocates new arena
 * 
 * @param size the least amount of bytes necessary 
 * @return arena_t* Pointer to the newly allocated arena
 */
static arena_t *arena_alloc(size_t size){
	// we need to allocate atleast enough memory to fit in the req size and header + arena metadata
	size_t minSize = size + sizeof(header_t) + sizeof(arena_t);
	// we need to page align it (to multiply of PAGE_SIZE)
	size_t alignedSize = ALIGN_ARENA(minSize);

	arena_t *arena = mmap(0, alignedSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);
	if(arena == MAP_FAILED)
		return NULL;
	
	arena->next = NULL;
	arena->size = alignedSize;
	return arena;
}
/**
 * @brief Finds the first big enough block
 * 
 * @return header_t* Big enough block or NULL
 */
static header_t *firstfit(size_t req_size){
	if(first_arena == NULL)
			return NULL;

	header_t *hdr = (header_t*)(first_arena+1);
	for ( ; hdr; hdr = hdr->next) {
		if((hdr->size >= req_size) && hdr->free)
			return hdr;
	}
	return NULL;
}
/**
 * @brief splits header in two
 * 
 * @param hdr header to be split
 * @param req_size Block aligned size
 * @return header_t* newly created header
 */
static header_t *hdr_split(header_t *hdr, size_t req_size)
{
	char *ptr = (char*)hdr;
	ptr += req_size + sizeof(header_t);
	header_t *hdr2 = (header_t*)ptr;
	hdr2->free = 1;
	hdr2->size = hdr->size - req_size - sizeof(header_t);
	hdr->size = req_size;
	hdr2->next = hdr->next;
	hdr->next = hdr2;
	return hdr2;
}

void *my_malloc(size_t size){
	header_t *freeHeader = firstfit(ALIGN_BLOCK(size));
	if(freeHeader != NULL){
// we found a block, try to split it, reserve it and return start of the free space
		if((freeHeader->size - size) >= MIN_BLOCK_SIZE)
		{
			freeHeader->next = hdr_split(freeHeader, ALIGN_BLOCK(size));
		}
		freeHeader->free = 0;
		return freeHeader+1;
	}
	else // allocate new
	{
		arena_t *arena = arena_alloc(size);
		if(arena == NULL)
			return NULL; // unsuccessful allocation
		header_t *hdr = (void*)(arena+1);
		size_t spaceLeft = arena->size - sizeof(arena_t) - sizeof(header_t);

		hdr->size = spaceLeft;
		hdr->free = 0;
		hdr->next = NULL;;

		if(spaceLeft - ALIGN_BLOCK(size) >= MIN_BLOCK_SIZE)
		{
			hdr->next = hdr_split(hdr, ALIGN_BLOCK(size));
		}

		if(first_arena == NULL){
			first_arena = arena;
		}
		else { // Append the arena to the end of list;
			arena_t *prev = first_arena;
			while(prev->next != NULL){
				prev = prev->next;
			}
			prev->next = arena;
			header_t *last = (header_t*)(prev+1);
			for(;last->next;last = last->next);
			last->next = hdr;
		}
		return hdr+1;
	}	
	// if we didn't find one
}