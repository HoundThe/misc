#include "my_malloc.h"

typedef struct header {
	// this aligns first element and thus the whole structure
	// to the most restrictive alignment on the platform (C11 feature)
	_Alignas(max_align_t) struct header *next;
	struct header *prev;
	size_t size;
	size_t free;
} header_t;

typedef struct arena {
	_Alignas(max_align_t) struct arena *next;
	size_t size;
} arena_t;

arena_t *first_arena = NULL;

/* mmap returns page aligned memory, lets allocate in huge chunks and delegate parts of them */
#define PAGE_SIZE (128 * 4096)

/* smallest size of the block possible. (header + the most restrictive alignment on the platform) */
#define MIN_BLOCK_SIZE (sizeof(header_t) + alignof(max_align_t))

/* align arena to the nearest multiply of PAGE_SIZE */
#define ALIGN_ARENA(size) ((size) % (PAGE_SIZE) ? (size) + (PAGE_SIZE) - (size) % (PAGE_SIZE) : (size))

/* align block to the nearest multiply of most restrictive alignment on the platform */
#define ALIGN_BLOCK(size) ((size + (alignof(max_align_t)) - 1) / (alignof(max_align_t))) * (alignof(max_align_t))

/**
 * @brief Allocates new arena
 *
 * @param size the least amount of bytes necessary
 * @return arena_t* Pointer to the newly allocated arena
 */
static arena_t *arena_alloc(size_t size)
{
	// we need to allocate atleast enough memory to fit in the req size and header + arena metadata
	size_t minSize = size + sizeof(header_t) + sizeof(arena_t);
	// we need to page align it (to multiply of PAGE_SIZE)
	size_t alignedSize = ALIGN_ARENA(minSize);
	// allocate the memory, watchout anonymous flag is linux specific thing
	arena_t *arena = mmap(0, alignedSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (arena == MAP_FAILED)
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
static header_t *first_fit(size_t req_size)
{
	if (first_arena == NULL)
		return NULL;

	header_t *hdr = (header_t *)(first_arena + 1);
	for (; hdr; hdr = hdr->next) {
		if ((hdr->size >= req_size) && hdr->free) // if the header is large enough and free, return it
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
static header_t *hdr_split(header_t *hdr, size_t req_size)
{
	// get pointer to the start of the new block
	header_t *new = (header_t *)((char *)hdr + req_size + sizeof(header_t));
	new->free = 1;
	new->size = hdr->size - req_size - sizeof(header_t);
	// wire the new header in, don't forget it's double linked list
	new->next = hdr->next;
	new->prev = hdr;
	hdr->size = req_size;
	if(hdr->next != NULL)
		hdr->next->prev = new;
	hdr->next = new;
	return new;
}

static inline int hdr_can_split(header_t *hdr, size_t minSize)
{
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
static inline int hdr_can_merge(header_t *hdr, header_t *next)
{
	char *ptr = (char *)hdr;
	// next should follow after hdr, both should be free and they should be in the same arena
	return (hdr->next == next && hdr->free == 1 && next->free == 1 &&
	        ptr + sizeof(header_t) + hdr->size == (char *)next)
	           ? 1
	           : 0;
}

static void hdr_merge(header_t *left, header_t *right)
{
	left->size += right->size + sizeof(header_t);
	left->next = right->next;
	if(right->next != NULL)
		right->next->prev = left;
}

static header_t *header_ctor(header_t *hdr, size_t size, size_t free, header_t *prev, header_t *next)
{
	hdr->size = size;
	hdr->free = free;
	hdr->next = next;
	hdr->prev = prev;
	return hdr;
}
void *my_malloc(size_t size)
{
	if (size == 0) // Malloc of 0 is implementation defined
		return NULL;

	size_t alignedSize = ALIGN_BLOCK(size);

	header_t *freeHdr = first_fit(alignedSize);
	if (freeHdr != NULL) { // we found a block, try to split it,
		                   // reserve it and return beginning of the free space
		if (hdr_can_split(freeHdr, alignedSize)){
			hdr_split(freeHdr, alignedSize);
		}
		freeHdr->free = 0;
		return freeHdr + 1; // return start opf
	}

	arena_t *arena = arena_alloc(size); // we need new space, arena

	if (arena == NULL) // unsuccessful allocation
		return NULL;

	header_t *hdr = header_ctor((void *)(arena + 1), arena->size - sizeof(header_t), 0, NULL, NULL);

	// if the space is large enough to contain the necessary memory
	// and atleast minimum for new header
	if (hdr_can_split(hdr, alignedSize))
		hdr_split(hdr, alignedSize);
	// if this is first allocation
	if (first_arena == NULL) {
		first_arena = arena;
	} else {
		// Append the arena to the end of list;
		arena_t *prev = first_arena;
		while (prev->next != NULL) {
			prev = prev->next;
		}
		prev->next = arena;
		// connect the linked list of headers with the newly created one
		header_t *last = (header_t *)(prev + 1);
		for (; last->next; last = last->next)
			;
		last->next = hdr;
		hdr->prev = last;
	}
	return hdr + 1; // start of the free memory block
}

void my_free(void *ptr)
{
	if (ptr == NULL)	// free of NULL pointer should do nothing
		return;

	header_t *hdr = (header_t *)ptr - 1;	// get the header
	hdr->free = 1;

	if (hdr_can_merge(hdr->prev, hdr)) {	// check if you can merge with left
		hdr_merge(hdr->prev, hdr);
	}
	if (hdr_can_merge(hdr, hdr->next)) {	// check if you can merge with right
		hdr_merge(hdr, hdr->next);
	}
	char *p = (char *)hdr;
	// if the block is alone in the arena
	if (p + sizeof(header_t) + hdr->size != (char *)hdr->next && 
	(p - sizeof(header_t) - hdr->prev->size != (char *)hdr->prev)) {
		if(hdr->prev != NULL)
			hdr->prev->next = hdr->next;
		if(hdr->next != NULL)
			hdr->next->prev = hdr->prev;
		arena_t *arena = (void*)(p - sizeof(arena_t));
		if(arena == first_arena)
			first_arena = NULL;
		else{ // find the arena and remove it from linked list
			arena_t *a = first_arena;
			for ( ; a->next; a = a->next){
				if(a->next == arena){
					a->next = arena->next;
					break;
				}
			}
			
		}

		munmap(arena, arena->size);
	}
}
