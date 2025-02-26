// process2.c
#include "shared_memory.h"

int main()
{
    printf("Starting shared memory reader (process2)\n");

    // Open semaphores
    sem_t *sem_ready = sem_open(SEM_READY_NAME, 0);
    sem_t *sem_done = sem_open(SEM_DONE_NAME, 0);

    if (sem_ready == SEM_FAILED || sem_done == SEM_FAILED)
    {
        perror("sem_open");
        printf("Make sure process1 is running first\n");
        return 1;
    }

    // Open shared memory
    shared_memory_t shm = {0};
    if (shared_memory_open(&shm, SHM_NAME) != 0)
    {
        printf("Failed to open shared memory\n");
        return 1;
    }

    printf("Connected to shared memory, signaling writer...\n");

    // Signal that reader is ready
    sem_post(sem_ready);

    // Wait for data to be ready
    sem_wait(sem_done);

    // Read data from shared memory
    printf("Read from shared memory: %s\n", (char *)shm.addr);

    // Clean up
    shared_memory_destroy(&shm, 0); // Don't unlink, let process1 do it
    sem_close(sem_ready);
    sem_close(sem_done);

    printf("Reader process completed\n");

    return 0;
}