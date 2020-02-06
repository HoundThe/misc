#include "my_malloc.h"

typedef struct header {
	// this aligns the header to the most restrictive alignment on the platform
	// beware, this is C11
	_Alignas(max_align_t) 
	struct header *next;
	struct header *prev;
	size_t size;
	char free;
} header_t;


typedef struct arena {
	_Alignas(max_align_t) struct arena *next;
	size_t size;
} arena_t;

arena_t *first_arena = NULL;
// mmap returns page aligned memory, lets allocate in huge chunks and delegate parts of them
#define PAGE_SIZE (128*1024)
// smallest size of the block possible. (header + the most restrictive alignment on the platform)
#define MIN_BLOCK_SIZE (sizeof(header_t) + alignof(max_align_t))
// align arena to the nearest multiply of PAGE_SIZE
#define ALIGN_ARENA(size)	\
	((size) % (PAGE_SIZE) ? (size) + (PAGE_SIZE) - (size) % (PAGE_SIZE) : (size))
// align block to the nearest multiply of most restrictive alignment on the platform
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
	// allocate the memory, watchout anonymous flag is linux specific thing
	arena_t *arena = mmap(0, alignedSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);
	if(arena == MAP_FAILED)
		return NULL;
	// init the arena metadata
	arena->next = NULL;
	// size with metadata excluded
	arena->size = alignedSize - sizeof(arena_t);
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
	for (; hdr; hdr = hdr->next) {
		// if the header is large enough and free, return it
		if((hdr->size >= req_size) && hdr->free)
			return hdr;
	}
	// not found
	return NULL;
}
/**
 * @brief splits header in two
 * 
 * @param hdr header to be split
 * @param req_size Block aligned size
 * @return header_t* newly created header
 */
static header_t *hdr_split(header_t *hdr, size_t req_size){
	// get pointer to the start of the new block
	header_t *new = (header_t*)((char*)hdr + req_size + sizeof(header_t));
	new->free = 1;
	new->size = hdr->size - req_size - sizeof(header_t);
	new->next = hdr->next;
	new->prev = hdr;
	hdr->size = req_size;
	hdr->next = new;
	return new;
}

static inline int hdr_can_split(header_t *hdr, size_t minSize){
	// presumption is that hdr->size is already aligned
	return (hdr->size >= MIN_BLOCK_SIZE + minSize) ? 1 : 0;
}
/**
 * @brief Checks if we can merge hdr with next
 * 
 * @param hdr
 * @param next next following header after hdr int the list!
 * @return int 1 if can else 0
 */
static inline int hdr_can_merge(header_t *hdr, header_t *next){
	char *ptr = (char*)hdr;
	// next should follow after hdr, both should be free and they should be in the same arena
	return (hdr->next == next && hdr->free == 1 && next->free == 1 && ptr + sizeof(header_t) + hdr->size != (char*)next) ? 1 : 0;
}

static void hdr_merge(header_t *left, header_t *right){
	left->size += right->size + sizeof(header_t);
	left->next = right->next;
	right->next->prev = left;	
}
void *my_malloc(size_t size){
	size_t alignedSize = ALIGN_BLOCK(size);
	header_t *freeHdr = firstfit(alignedSize);
	// we found a block, try to split it, reserve it and return start of the free space
	if(freeHdr != NULL){
		if(hdr_can_split(freeHdr, alignedSize))
			hdr_split(freeHdr, alignedSize);
		freeHdr->free = 0;
		return freeHdr+1;
	} 
	// allocate new arena
	arena_t *arena = arena_alloc(size);
	if(arena == NULL)
		return NULL; // unsuccessful allocation
	// get pointer to the first header
	header_t *hdr = (void*)(arena+1);
	hdr->size = arena->size - sizeof(header_t);
	hdr->free = 0;
	hdr->next = NULL;
	hdr->prev = NULL;
	// if the spaceLeft is large enough to contain the necessary memory
	// and atleast minimum for new header
	if(hdr_can_split(hdr, alignedSize))
		hdr_split(hdr, alignedSize);
	// if this is first allocation
	if(first_arena == NULL) {
		first_arena = arena;
	} else {
		// Append the arena to the end of list;
		arena_t *prev = first_arena;
		while(prev->next != NULL){
			prev = prev->next;
		}
		prev->next = arena;
		// connect the linked list of headers with the newly created one
		header_t *last = (header_t*)(prev+1);
		for(;last->next;last = last->next);
		last->next = hdr;
		hdr->prev = last;
	}
	return hdr+1;
}

void free(void *ptr){
	// free of NULL pointer should do nothing
	if(ptr == NULL)
		return;
	// get the header
	header_t *hdr = (header_t*)ptr-1;
	hdr->free = 1;
	// check if you can merge with left
	if(hdr_can_merge(hdr->prev, hdr)){
		hdr_merge(hdr->prev, hdr);
	}
	// check if you can merge with right
	if(hdr_can_merge(hdr, hdr->next)){
		hdr_merge(hdr, hdr->next);
	}
	char *ptr = (char*)hdr;
	// if the block is alone in the arena
	if(ptr + sizeof(header_t) + hdr->size != (char*)hdr->next && (hdr->prev == NULL || ptr - 2 * sizeof(header_t) + hdr->prev->size != (char*)hdr->prev)){
		hdr->prev = hdr->next;
		hdr->next->prev = hdr->prev;
		
		arena_t *p, *pprev;
	}
}
