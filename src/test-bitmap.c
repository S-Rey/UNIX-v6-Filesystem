#include "bmblock.h"
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>


int main () {
	// fill the pointer with the struct
	struct bmblock_array* bmblock;
	uint64_t min = 4;
	uint64_t max = 131;
	bmblock = bm_alloc(min,max);

	bm_print(bmblock);
	printf("find_next() = %d\n", bm_find_next(bmblock));

	bm_set(bmblock, 4);
	bm_set(bmblock, 5);
	bm_set(bmblock, 6);

	bm_print(bmblock);
	printf("find_next() = %d\n", bm_find_next(bmblock));

	for(size_t i = 4; i < bmblock->max; i+=3) {
		bm_set(bmblock, i);
	}

	bm_print(bmblock);
	printf("find_next() = %d\n", bm_find_next(bmblock));

	for(size_t i = 5; i < bmblock->max; i+=5) {
		bm_clear(bmblock, i);
	}


	bm_print(bmblock);
	printf("find_next() = %d\n", bm_find_next(bmblock));




	// free the pointer
	free(bmblock);
	bmblock = NULL;

	return 0;
}