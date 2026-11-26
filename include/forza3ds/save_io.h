/* Forza3DS - checksummed, versioned save files with a backup copy. */
#ifndef F3DS_SAVE_IO_H
#define F3DS_SAVE_IO_H

#include "common.h"
#include <stddef.h>

typedef bool (*F3DSSaveValidator)(const void *payload);

uint32_t save_crc32(const void *data, size_t size);
/* Writes a temporary file, verifies it, keeps the previous valid file as
 * "<path>.bak", then moves the new file into place. */
bool save_write(const char *path, uint32_t magic, uint32_t version, const void *payload, size_t size,
	F3DSSaveValidator valid);
/* Reads path, falling back to the backup. payload is untouched on failure. */
bool save_read(const char *path, uint32_t magic, uint32_t version, void *payload, size_t size,
	F3DSSaveValidator valid);

#endif
