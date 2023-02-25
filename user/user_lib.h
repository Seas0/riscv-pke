/*
 * header file to be used by applications.
 */
#include "util/types.h"

int printu(const char *s, ...);
int exit(int code);
void* naive_malloc();
void naive_free(void* va);
int fork();
void yield();
uint64 sem_new(uint64 init_value);
void sem_P(uint64 id);
void sem_V(uint64 id);

