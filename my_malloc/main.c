#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdalign.h>
#include <string.h>
#include "my_malloc.h"
#include <assert.h>
#include <time.h>
typedef struct header {
	// this aligns the header to the most restrictive alignment on the platform
	// beware, this is C11
	alignas(max_align_t) 
	struct header *next;
	struct header *prev;
	size_t size;
	size_t free;
} header_t;


typedef struct arena {
	alignas(max_align_t) struct arena *next;
	size_t size;
} arena_t;

extern arena_t *first_arena;

void debug_hdr(header_t *h, int idx)
{
	char free[2][4] = {"no", "yes"};
	printf("+- Header %d @ %p, data @ %p\n", idx, (void*)h, (void*)&h[1]);
	printf("|    | next           | prev           | size     | isFree   |\n");
	printf("|    | %-14p | %-14p | %-8lu | %8s |\n", (void*)h->next, (void*)h->prev, h->size, free[h->free]);
}

void debug_arena(arena_t *a, int idx)
{
	printf("Arena %d @ %p\n", idx, (void*)a);
	printf("|\n");
	char *arena_stop = (char*)a + a->size;
	header_t *h = (header_t*)&a[1];
	int i = 1;

	while ((char*)h >= (char*)a && (char*)h < arena_stop)
	{
		debug_hdr(h, i);
		i++;
		h = h->next;
		if (h == (header_t*)&a[1])
			break;
	}
}

#define PAGE_SIZE (128*1024)

void debug_arenas()
{
	if(first_arena == NULL){
		printf("==========================================\n");
		printf("================= EMPTY ==================\n");
		printf("==========================================\n");
		printf("==========================================\n");
		return;
	}
	printf("\n*****************************************************\n");
	printf("***** START *******************************************\n");
	arena_t *a = first_arena;
	for (int i = 1; a; i++)
	{
		debug_arena(a, i);
		a = a->next;
	}
	printf("\n************* END\n");
	printf("******************************\n\n");
}



int main(void)
{
	assert(first_arena == NULL);
	printf("Size of header and arena %lu %lu\n", sizeof(header_t), sizeof(arena_t));
	/***********************************************************************/
	// Prvni alokace
	// Mela by alokovat novou arenu, pripravit hlavicku v ni a prave jeden
	// blok.
	void *p[100];
	debug_arenas();
	p[0] = my_malloc(123);
	memset(p[0],'K', 123);
	debug_arenas();
	p[1] = my_malloc(123123122);
	memset(p[1],'A', 130856);
	debug_arenas();
	p[2] = my_malloc(123);
	memset(p[2],'A', 123);
	debug_arenas();
	p[3] = my_malloc(123);
	memset(p[3],'A', 123);
	debug_arenas();
	p[4] = my_malloc(130705);
	memset(p[4],'A', 130705);
	debug_arenas();
	
	my_free(p[2]);
	debug_arenas();

	my_free(p[3]);
	debug_arenas();

	p[2] = my_malloc(288);
	debug_arenas();

	p[5] = my_malloc(393000);
	debug_arenas();
	p[6] = my_malloc(128);
	debug_arenas();

	my_free(p[6]);
	debug_arenas();

	my_free(p[1]);
	debug_arenas();

	return 0;
}
