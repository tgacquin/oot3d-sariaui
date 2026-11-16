#pragma once
#include <stddef.h>
void* memcpy(void*,const void*,size_t);
void* memset(void*,int,size_t);
int memcmp(const void*,const void*,size_t);
size_t strlen(const char*);
size_t strnlen(const char*,size_t);
char* strncpy(char*,const char*,size_t);
int strcmp(const char*,const char*);
