#include "bmblock.h"
#include "error.h"
#include <stdio.h>
#include <inttypes.h>


struct bmblock_array *bm_alloc(uint64_t min, uint64_t max)
{
	struct bmblock_array* bmb_array = NULL;
	// test if min and max are valid
	if (min > max) {
		return NULL;
	}
	// calloc size of bmblock_array
	size_t length = (size_t) ((max-min) / BITS_PER_VECTOR);
	bmb_array = calloc(1, sizeof(struct bmblock_array)
			      + (length)*sizeof(uint64_t));
	// if calloc not successfull
	if (!bmb_array) {
		return NULL;
	}

	bmb_array->cursor = 0;
	bmb_array->min = min;
	bmb_array->max = max;
	bmb_array->length = length + 1;

	return bmb_array;
}

int bm_get(struct bmblock_array *bmblock_array, uint64_t x)
{
	// test argument
	M_REQUIRE_NON_NULL(bmblock_array);

	// check if out of boundaries
	if (x < bmblock_array->min || x > bmblock_array->max) {
		return ERR_BAD_PARAMETER;
	}

	// compute in which index it is
	size_t index = (size_t) (x - bmblock_array->min) / BITS_PER_VECTOR;
	size_t elem = (size_t) (x - bmblock_array->min) % BITS_PER_VECTOR;

	// get the value
	uint64_t bm_vect = bmblock_array->bm[index];
	int value = (bm_vect >> elem) & 1;

	return value;
}


void bm_set(struct bmblock_array *bmblock_array, uint64_t x)
{
	// test arguments
	if (!bmblock_array || x < bmblock_array->min || x > bmblock_array->max) {
		return;
	}

	// test if already set to 1
	if (bm_get(bmblock_array, x) == 1) {
		return;
	}

	// compute in which index it is
	size_t index = (size_t) (x - bmblock_array->min) / BITS_PER_VECTOR;
	size_t elem = (size_t) (x - bmblock_array->min) % BITS_PER_VECTOR;

	// set the value
	uint64_t value = (UINT64_C(1) << elem);
	bmblock_array->bm[index] |= value;
}


void bm_clear(struct bmblock_array *bmblock_array, uint64_t x)
{
	// test arguments
	if (!bmblock_array || x < bmblock_array->min || x > bmblock_array->max) {
		return;
	}

	// test if already set to 0
	if (bm_get(bmblock_array, x) == 0) {
		return;
	}

	// compute in which index it is
	size_t index = (size_t) (x - bmblock_array->min) / BITS_PER_VECTOR;
	size_t elem = (size_t) (x - bmblock_array->min) % BITS_PER_VECTOR;

	// set the value
	uint64_t value = (UINT64_C(1) << elem);
	bmblock_array->bm[index] &= ~value;
}


void bm_print_content(struct bmblock_array *bmblock_array)
{
	// test argument
	if (!bmblock_array) {
		return;
	}

	printf("content:\n");
	for(size_t i = 0; i < bmblock_array->length; ++i) {
		printf("%zu: ", i);
		// print the element of an array
		for (size_t j = 0; j < (size_t)BITS_PER_VECTOR; ++j) {
			uint64_t i_elem = j + i*BITS_PER_VECTOR + bmblock_array->min;
			if (i_elem <= bmblock_array->max) {
				printf("%d", bm_get(bmblock_array, i_elem));
			} else {
				printf("0");
			}

			if((j +1)% 8 == 0) {
				printf(" ");
			}
		}
		printf("\n");
	}

}


int bm_find_next(struct bmblock_array *bmblock_array)
{
	uint64_t index = bmblock_array->min;
	while (bm_get(bmblock_array, index) && index <= bmblock_array->max) {
		++index;
	}
	if (index > bmblock_array->max) {
		return -1;
	} else {
		return (int)index;
	}
}

void bm_print(struct bmblock_array *bmblock_array)
{
	// test argument
	if (!bmblock_array) {
		return;
	}


	printf("**********BitMap Block START**********\n");
	printf("length: %zu\n",bmblock_array->length);
	printf("min: %" PRId64"\n", bmblock_array->min);
	printf("max: %" PRId64"\n", bmblock_array->max);
	printf("cursor: %" PRId64"\n", bmblock_array->cursor);
	bm_print_content(bmblock_array);
	printf("**********BitMap Block END**********\n");

}

