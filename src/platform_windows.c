#ifdef _WIN32
#include "platform.h"
#include <Shlobj.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <libavutil/error.h>
#include "log.h"
#include "util.h"

wchar_t w_av_errbuf[AV_ERROR_MAX_STRING_SIZE];

int mkdir_p(const pchar *path)
{
	pchar abp[MAX_PATH];

	if (PathIsRelativeW(path)) {
		/* SHCreateDirectoryExW only accepts an absolute path */
		path = _wfullpath(abp, path, ARRAY_COUNT(abp));
		if (path == NULL)
			return ERROR_FILENAME_EXCED_RANGE;
	}
	int r = SHCreateDirectoryExW(NULL, path, NULL);
	if (r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS)
		return 0;
	return r;
}

pchar *basename(pchar *path)
{
	pchar *c = wcsrchr(path, PSTR('\\'));
	if (c == NULL)
		return path;
	return c + 1;
}

pchar *get_current_dir_name()
{
	DWORD charCnt = GetCurrentDirectoryW(0, NULL);
	DWORD cwdWideByteLen = (charCnt + 1) * sizeof(pchar);
	pchar *cwdWide = malloc(cwdWideByteLen);
	GetCurrentDirectoryW(charCnt + 1, cwdWide);

	return cwdWide;
}


int platform_memory_map_file(const pchar *file, struct memory_file_map *out)
{
	HANDLE fileH = CreateFileW(file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fileH == INVALID_HANDLE_VALUE) {
		log_error("Failed to open file: %d\n", GetLastError());
		return -1;
	}
	LARGE_INTEGER fsizeli;
	if (!GetFileSizeEx(fileH, &fsizeli)) {
		log_error("Faled to get file size: %d\n", GetLastError());
		CloseHandle(fileH);
		return -1;
	}

	HANDLE mapH = CreateFileMappingW(fileH, NULL, PAGE_READONLY, 0, 0, NULL);
	if (mapH == NULL) {
		log_error("Failed to create file mapping: %d\n", GetLastError());
		CloseHandle(fileH);
		return -1;
	}

	void *addr = MapViewOfFile(mapH, FILE_MAP_READ, 0, 0, 0);
	if (addr == NULL) {
		log_error("Failed to map file: %d\n", GetLastError());
		CloseHandle(mapH);
		CloseHandle(fileH);
		return -1;
	}

	*out = (struct memory_file_map){
		.addr = addr,
		.size = fsizeli.QuadPart,
		.fileH = fileH,
		.mapH = mapH,
	};
	return 0;
}

void platform_memory_unmap_file(struct memory_file_map *map)
{
	if (map->addr)
		UnmapViewOfFile(map->addr);
	if (map->mapH)
		CloseHandle(map->mapH);
	if (map->fileH)
		CloseHandle(map->fileH);
	memset(map, 0, sizeof(*map));
}

#if 0
void utf8_to_pchar(char *in_ascii, int in_ascii_clen, pchar *out_pchar, int out_pchar_csize)
{
    assert(in_ascii_clen > 0);
    assert(out_pchar_csize > 0);
	int n = MultiByteToWideChar(CP_UTF8, 0, in_ascii, in_ascii_clen, out_pchar, out_pchar_csize);
	assert(n != 0);
	/* Only null terminated when cbMultiByte is -1 */
	assert(out_pchar_csize > n);
	out_pchar[n] = PSTR('\0');
}

void font_ucs_to_pchar(wchar_t *in_wchar, int in_wchar_clen, pchar *out_pchar, int out_pchar_csize)
{
	/* On windows, no need to do anything */
    assert(in_wchar_clen > 0);
    assert(out_pchar_csize > 0);

	assert(out_pchar_csize > in_wchar_clen);
	for (int i = 0; i < in_wchar_clen; i++) {
		out_pchar[i] = _byteswap_ushort(in_wchar[i]);
	}
	out_pchar[in_wchar_clen] = L'\0';
}
#endif

char PCu8_buffer[PCu8_BUFFER_SIZE];
char *platform_pchar_to_u8(const pchar *in, int in_ccount, char *out, int out_bsize)
{
	int n = WideCharToMultiByte(CP_UTF8, 0, in, in_ccount, out, out_bsize, NULL, NULL);
	assert(n != 0);
	if (in_ccount != -1)
		out[n] = '\0';
	return out;
}

pchar *platform_u8_to_pchar_mem(char *in)
{
	int n = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);
	assert(n != 0);
	pchar *ob = malloc(n * sizeof(*ob));
	assert(ob);
	n = MultiByteToWideChar(CP_UTF8, 0, in, -1, ob, n);
	assert(n != 0);
	free(in);
	return ob;
}

void unicode_to_pchar(char32_t cp, pchar outs[8])
{
	char u8[8];
	unicode_to_utf8(cp, u8);
	MultiByteToWideChar(CP_UTF8, 0, u8, -1, outs, 8);
}


pchar u8PC_buffer[PCu8_BUFFER_SIZE];
pchar *platform_u8_to_pchar(const char *in, int in_bcount, pchar *out, int out_csize)
{
	int n = MultiByteToWideChar(CP_UTF8, 0, in, in_bcount, out, out_csize);
	assert(n != 0);
	if (in_bcount != -1)
		out[n] = L'\0';
	return out;
}

#endif /* _WIN32 */
