#include "save_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SAVE_MAX_PAYLOAD (64u * 1024u)

typedef struct {
	uint32_t magic, version, size, crc;
} Header;

uint32_t save_crc32(const void *data, size_t size)
{
	const uint8_t *p = data;
	uint32_t crc = 0xFFFFFFFFu;
	while (size--) {
		crc ^= *p++;
		for (int i = 0; i < 8; ++i)
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

/* Create every parent directory of path. */
static void make_parents(const char *path)
{
	char buf[256];
	const size_t len = strlen(path);
	if (len >= sizeof(buf))
		return;
	memcpy(buf, path, len + 1);
	for (size_t i = 1; i < len; ++i) {
		if (buf[i] == '/' && buf[i - 1] != ':') {
			buf[i] = '\0';
			mkdir(buf, 0777);
			buf[i] = '/';
		}
	}
}

static bool read_one(const char *path, uint32_t magic, uint32_t version, void *out, size_t size,
	F3DSSaveValidator valid)
{
	if (size == 0 || size > SAVE_MAX_PAYLOAD)
		return false;
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	Header h;
	void *buf = malloc(size);
	bool ok = buf && fread(&h, sizeof(h), 1, f) == 1 && h.magic == magic && h.version == version &&
		h.size == size && fread(buf, size, 1, f) == 1 && fgetc(f) == EOF &&
		save_crc32(buf, size) == h.crc && (!valid || valid(buf));
	fclose(f);
	if (ok)
		memcpy(out, buf, size);
	free(buf);
	return ok;
}

bool save_write(const char *path, uint32_t magic, uint32_t version, const void *payload, size_t size,
	F3DSSaveValidator valid)
{
	if (size == 0 || size > SAVE_MAX_PAYLOAD || (valid && !valid(payload)))
		return false;
	char tmp[272], bak[272];
	if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp) ||
	    snprintf(bak, sizeof(bak), "%s.bak", path) >= (int)sizeof(bak))
		return false;
	make_parents(path);

	FILE *f = fopen(tmp, "wb");
	if (!f)
		return false;
	const Header h = { magic, version, (uint32_t)size, save_crc32(payload, size) };
	bool ok = fwrite(&h, sizeof(h), 1, f) == 1 && fwrite(payload, size, 1, f) == 1 && fflush(f) == 0;
	if (fclose(f) != 0)
		ok = false;
	void *check = malloc(size);
	ok = ok && check && read_one(tmp, magic, version, check, size, valid);
	if (ok) {
		/* Only a valid current file may replace the backup. */
		if (read_one(path, magic, version, check, size, valid)) {
			remove(bak);
			ok = rename(path, bak) == 0;
		} else {
			remove(path);
		}
	}
	free(check);
	if (ok)
		ok = rename(tmp, path) == 0;
	if (!ok)
		remove(tmp);
	return ok;
}

bool save_read(const char *path, uint32_t magic, uint32_t version, void *payload, size_t size,
	F3DSSaveValidator valid)
{
	if (read_one(path, magic, version, payload, size, valid))
		return true;
	char bak[272];
	if (snprintf(bak, sizeof(bak), "%s.bak", path) >= (int)sizeof(bak))
		return false;
	return read_one(bak, magic, version, payload, size, valid);
}
