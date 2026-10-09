#ifndef BL_PLATFORM_H
#define BL_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#ifdef _WIN32
#include "winmini.h"
__declspec(dllimport) void * __cdecl malloc(size_t);
__declspec(dllimport) void * __cdecl calloc(size_t,size_t);
__declspec(dllimport) void * __cdecl realloc(void *,size_t);
__declspec(dllimport) void __cdecl free(void *);
__declspec(dllimport) void * __cdecl memcpy(void *,const void *,size_t);
__declspec(dllimport) void * __cdecl memmove(void *,const void *,size_t);
__declspec(dllimport) void * __cdecl memset(void *,int,size_t);
__declspec(dllimport) int __cdecl memcmp(const void *,const void *,size_t);
__declspec(dllimport) size_t __cdecl strlen(const char *);
__declspec(dllimport) int __cdecl strcmp(const char *,const char *);
__declspec(dllimport) int __cdecl strncmp(const char *,const char *,size_t);
__declspec(dllimport) char * __cdecl strstr(const char *,const char *);
__declspec(dllimport) char * __cdecl strchr(const char *,int);
__declspec(dllimport) int __cdecl _vsnprintf(char *,size_t,const char *,va_list);
#define vsnprintf _vsnprintf
typedef HANDLE BLFile;
#define BL_BAD_FILE INVALID_HANDLE_VALUE
#else
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
typedef FILE *BLFile;
#define BL_BAD_FILE NULL
#endif
int bl_format(char *out,size_t cap,const char *fmt,...);
char *bl_dup(const char *s);
char *bl_ndup(const char *s,size_t len);
BLFile bl_open(const char *path,int write); /* write: 0 read; 1 create/truncate; 2 append */
void bl_close(BLFile f);
size_t bl_read(BLFile f,void *buf,size_t n);
int bl_write(BLFile f,const void *buf,size_t n);
int bl_seek(BLFile f,uint64_t off);
uint64_t bl_size(BLFile f);
uint64_t bl_mtime(BLFile f);
int bl_mkdir(const char *path);
int bl_remove(const char *path);
int bl_move(const char *from,const char *to);
uint64_t bl_millis(void);
uint32_t bl_pid(void);
int bl_flush(BLFile f);
#ifdef _WIN32
WCHAR *bl_wide(const char *s);
char *bl_utf8(const WCHAR *s);
WCHAR *bl_winpath(const char *s);
#endif
#endif
