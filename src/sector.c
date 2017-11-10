#include <stdio.h>
#include "sector.h"
#include "unixv6fs.h"
#include "error.h"


int sector_read(FILE *f, uint32_t sector, void *data)
{
	// Test arguments
	M_REQUIRE_NON_NULL(f);
	M_REQUIRE_NON_NULL(data);

	// compute the offset to read
	uint32_t position = sector * SECTOR_SIZE;

	// if fseek did not work return ERR_IO
	if(fseek(f, position, SEEK_SET) == 0) {
		// if fread did not work return ERR_IO
		if(fread(data, SECTOR_SIZE, 1, f) != 1) {
			return ERR_IO;
		}
	} else {
		return ERR_IO;
	}

	return 0;
}



int sector_write(FILE *f, uint32_t sector, void  *data)
{
	// Test arguments
	M_REQUIRE_NON_NULL(f);
	M_REQUIRE_NON_NULL(data);

	// compute the offset to read
	uint32_t position = sector * SECTOR_SIZE;

	// if fseek did not work return ERR_IO
	if(fseek(f, position, SEEK_SET) == 0) {
		// if fread did not work return ERR_IO
		int return_write = fwrite(data, SECTOR_SIZE, 1, f);
		if(return_write != 1) {
			return ERR_IO;
		}
	} else {
		return ERR_IO;
	}

	return 0;
}
