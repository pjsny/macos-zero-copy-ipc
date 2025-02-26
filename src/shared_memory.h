#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

#define SHARED_MEM_SIZE 4096
#define SHM_NAME "/my_shared_memory"
#define SEM_READY_NAME "/sem_ready"
#define SEM_DONE_NAME "/sem_done"

typedef struct
{
    void *addr;
    size_t size;
    int fd;
    const char *name;
} shared_memory_t;

// Create and map shared memory
int shared_memory_create(shared_memory_t *shm, const char *name, size_t size);

// Open and map existing shared memory
int shared_memory_open(shared_memory_t *shm, const char *name);

// Unmap and close/unlink shared memory
void shared_memory_destroy(shared_memory_t *shm, int unlink_shm);

#endif