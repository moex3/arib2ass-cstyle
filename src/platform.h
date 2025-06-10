#ifndef ARIB2ASS_PLATFORM_H
#define ARIB2ASS_PLATFORM_H
#include <uchar.h>

#ifdef _WIN32
#define _CRT_INTERNAL_NONSTDC_NAMES 1
#include <BaseTsd.h>
#include <Windows.h>
#include <libavutil/error.h>

typedef wchar_t pchar;

#define ssize_t SSIZE_T
#define strdup _strdup
/* Wide snprintf are counting characters, not bytes, but we pass a byte count to the linux functions
 * so make it into a char count for wndows by dividing by sizeof(pchar) */
#define psnprintf(b, s, fmt, ...) _snwprintf(b, s / sizeof(pchar), fmt, __VA_ARGS__)
#define PLATFORM_CURRENT_TIMESPEC(otsp) ((void)_timespec64_get(otsp, TIME_UTC))
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define fnmain wmain
#define pstat _stati64
#define pstatfn _wstat64
#define ptimespec _timespec64

#define PSTR(s) L ## s
#define PSTR2(s) PSTR(s)
#define pstrdup(s) _wcsdup(s)
#define pstrcmp(x, y) wcscmp(x, y)
#define pstrlen wcslen
#define pstrerror _wcserror
#define pprintf wprintf
#define pfprintf fwprintf
#define pfopen _wfopen
#define pperror _wperror
#define pstrrchr wcsrchr
#define pvfprintf vfwprintf

/* Convert a pchar into an utf8 text using a static buffer */
#define PCu8_BUFFER_SIZE 1024
extern char PCu8_buffer[PCu8_BUFFER_SIZE];
#define PCu8(pc) (platform_pchar_to_u8(pc, -1, PCu8_buffer, sizeof(PCu8_buffer)))

#define u8PCmem(u8) (platform_u8_to_pchar_mem(u8))

#define u8PC_BUFFER_SIZE 1024
extern pchar u8PC_buffer[PCu8_BUFFER_SIZE];
#define u8PC(u8) (platform_u8_to_pchar(u8, -1, u8PC_buffer, u8PC_BUFFER_SIZE))

pchar *basename(pchar *path);
pchar *get_current_dir_name();

struct memory_file_map {
	void  *addr;
	size_t size;

	HANDLE fileH, mapH;
};
int  platform_memory_map_file(const pchar *file, struct memory_file_map *out);
void platform_memory_unmap_file(struct memory_file_map *map);

char  *platform_pchar_to_u8(const pchar *in, int in_ccount, char *out, int out_bsize);
pchar *platform_u8_to_pchar(const char *in, int in_bcount, pchar *out, int out_csize);
pchar *platform_u8_to_pchar_mem(char *in);
#endif

#ifdef __linux__
#include <unistd.h>

#define PLATFORM_CURRENT_TIMESPEC(otsp) (clock_gettime(CLOCK_MONOTONIC_COARSE, otsp))
#define fnmain main

typedef char pchar;

#define PSTR(s) s
#define PSTR2(s) PSTR(s)
#define pstrdup(s) strdup(s)
#define pstrcmp(x, y) strcmp(x, y)
#define psnprintf snprintf

#define pstat stat
#define pstatfn stat
#define pstrlen strlen
#define pstrerror strerror
#define pprintf printf
#define pstrrchr strrchr
#define ptimespec timespec
#define pfopen fopen
#define pperror perror
#define pfprintf fprintf
#define pvfprintf vfprintf

#define PCu8(pc) (pc)
#define u8PCmem(u8) (u8)
#define u8PC(u8) (u8)

#endif

int mkdir_p(const pchar *path);
//void utf8_to_pchar(char *in_ascii, int in_ascii_clen, pchar *out_pchar, int out_pchar_csize);
//void font_ucs_to_pchar(wchar_t *in_wchar, int in_wchar_clen, pchar *out_pchar, int out_pchar_csize);
void unicode_to_pchar(char32_t cp, pchar outs[8]);

#endif /* ARIB2ASS_PLATFORM_H */
