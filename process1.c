// process1.c
#include "shared_memory.h"

int main()
{
    printf("Starting shared memory writer (process1)\n");

    // Create semaphores
    sem_unlink(SEM_READY_NAME);
    sem_unlink(SEM_DONE_NAME);

    sem_t *sem_ready = sem_open(SEM_READY_NAME, O_CREAT, S_IRUSR | S_IWUSR, 0);
    sem_t *sem_done = sem_open(SEM_DONE_NAME, O_CREAT, S_IRUSR | S_IWUSR, 0);

    if (sem_ready == SEM_FAILED || sem_done == SEM_FAILED)
    {
        perror("sem_open");
        return 1;
    }

    // Create shared memory
    shared_memory_t shm = {0};
    if (shared_memory_create(&shm, SHM_NAME, SHARED_MEM_SIZE) != 0)
    {
        printf("Failed to create shared memory\n");
        return 1;
    }

    printf("Shared memory created, waiting for reader...\n");

    // Wait for reader to connect
    sem_wait(sem_ready);

    // Write data to shared memory
    const char *message = "Hello from process1 - this is zero-copy shared memory!";
    strcpy(shm.addr, message);

    printf("Data written to shared memory: %s\n", (char *)shm.addr);

    // Signal that data is ready
    sem_post(sem_done);

    // Wait for a moment to ensure reader has time to read
    sleep(1);

    // Clean up
    shared_memory_destroy(&shm, 1);
    sem_close(sem_ready);
    sem_close(sem_done);
    sem_unlink(SEM_READY_NAME);
    sem_unlink(SEM_DONE_NAME);

    printf("Writer process completed\n");

    return 0;
}