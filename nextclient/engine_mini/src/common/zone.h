#pragma once

#include <quakedef.h>

void* Mem_Malloc(size_t size);
void* Mem_ZeroMalloc(size_t size);
void Mem_Free(void *p);
char* Mem_Strdup(const char* strSource);

void *Z_Malloc(int size);

void* Cache_Alloc(cache_user_t* c, int size, char* name);
void* Cache_Check(cache_user_t* c);
void Cache_Free(cache_user_t* c);

void* Hunk_AllocName(int size, const char* name);
void* Hunk_Alloc(int size);
void* Hunk_TempAlloc(int size);
void Hunk_Check();
