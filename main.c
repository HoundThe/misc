#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdalign.h>
#include "my_malloc.h"
#include <assert.h>

typedef struct header {
	// this aligns the header to the most restrictive alignment on the platform
	// beware, this is C11
	alignas(max_align_t) struct header *next;
	size_t size;
    char free;
} header_t;


typedef struct arena {
	alignas(max_align_t) struct arena *next;
	size_t size;
} arena_t;

extern arena_t *first_arena;

void debug_hdr(header_t *h, int idx)
{
    printf("+- Header %d @ %p, data @ %p\n", idx, h, &h[1]);
    printf("|    | next           | size     | isFree   |\n");
    printf("|    | %-14p | %-8lu | %-8lu |\n", h->next, h->size, h->free);
}

void debug_arena(arena_t *a, int idx)
{
    printf("Arena %d @ %p\n", idx, a);
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



int main(int argc, char **argv)
{
    assert(first_arena == NULL);
    printf("Size of header and arena %lu %lu", sizeof(header_t), sizeof(arena_t));
    /***********************************************************************/
    // Prvni alokace
    // Mela by alokovat novou arenu, pripravit hlavicku v ni a prave jeden
    // blok.
    void *p1 = my_malloc(131025);

    /**
     *   v----- first_arena
     *   +-----+------+----+------+----------------------------+
     *   |Arena|Header|XXXX|Header|............................|
     *   +-----+------+----+------+----------------------------+
     *       p1-------^
     */
    // assert(first_arena != NULL);
    // assert(first_arena->next == NULL);
    // assert(first_arena->size > 0);
    // assert(first_arena->size <= PAGE_SIZE);
    // header_t *h1 = (header_t*)(&first_arena[1]);
    // header_t *h2 = h1->next;
    // assert(h1->free == 0);
    // assert((char*)h2 > (char*)h1);
    // // assert(h2->next == h1);
    // assert(h2->free == 1);

    // my_malloc(131072);
    // my_malloc(1);
    // my_malloc(13000);

    debug_arenas();
    return;
}