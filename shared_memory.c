// shared_memory.c
#include "shared_memory.h"

int shared_memory_create(shared_memory_t *shm, const char *name, size_t size)
{
    // First, try to unlink any existing shared memory with this name
    shm_unlink(name);

    // Create the shared memory object
    int fd = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("shm_open");
        return -1;
    }

    // Set the size of the shared memory object
    if (ftruncate(fd, size) == -1)
    {
        perror("ftruncate");
        close(fd);
        shm_unlink(name);
        return -1;
    }

    // Map the shared memory object into this process's address space
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        shm_unlink(name);
        return -1;
    }

    // Initialize the shared memory struct
    shm->addr = addr;
    shm->size = size;
    shm->fd = fd;
    shm->name = name;

    return 0;
}

int shared_memory_open(shared_memory_t *shm, const char *name)
{
    // Open the shared memory object
    int fd = shm_open(name, O_RDWR, 0);
    if (fd == -1)
    {
        perror("shm_open");
        return -1;
    }

    // Get the size of the shared memory object
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("fstat");
        close(fd);
        return -1;
    }

    // Map the shared memory object into this process's address space
    void *addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return -1;
    }

    // Initialize the shared memory struct
    shm->addr = addr;
    shm->size = sb.st_size;
    shm->fd = fd;
    shm->name = name;

    return 0;
}

void shared_memory_destroy(shared_memory_t *shm, int unlink_shm)
{
    if (shm->addr != NULL && shm->addr != MAP_FAILED)
    {
        // Unmap the shared memory
        munmap(shm->addr, shm->size);
    }

    if (shm->fd != -1)
    {
        // Close the file descriptor
        close(shm->fd);
    }

    if (unlink_shm && shm->name != NULL)
    {
        // Remove the shared memory object
        shm_unlink(shm->name);
    }

    // Reset the struct
    shm->addr = NULL;
    shm->size = 0;
    shm->fd = -1;
    shm->name = NULL;
}