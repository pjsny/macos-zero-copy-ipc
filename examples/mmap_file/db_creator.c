#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include "mmap_shared.h"

int main()
{
    printf("Creating memory-mapped database file...\n");

    // Create or truncate the file
    int fd = open(MMAP_FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("Failed to open file");
        return 1;
    }

    // Set the file size
    if (ftruncate(fd, MMAP_FILE_SIZE) == -1)
    {
        perror("Failed to set file size");
        close(fd);
        return 1;
    }

    // Map the file into memory
    void *addr = mmap(NULL, MMAP_FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("Failed to map file");
        close(fd);
        return 1;
    }

    // Initialize the database structure
    mmap_database_t *db = (mmap_database_t *)addr;

    // Initialize mutex with process-shared attribute
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&db->mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    // Initialize database metadata
    db->record_count = 0;
    db->max_records = MAX_RECORDS(MMAP_FILE_SIZE);

    printf("Database initialized with capacity for %u records\n", db->max_records);

    // Add some initial records
    pthread_mutex_lock(&db->mutex);

    for (uint32_t i = 0; i < 5; i++)
    {
        record_t *record = &db->records[i];
        record->id = i + 1;
        snprintf(record->name, sizeof(record->name), "Initial Record %u", i + 1);
        record->value = (i + 1) * 10.5;
        record->is_active = true;
        db->record_count++;
    }

    pthread_mutex_unlock(&db->mutex);

    printf("Added 5 initial records\n");

    // Sync changes to disk
    if (msync(addr, MMAP_FILE_SIZE, MS_SYNC) == -1)
    {
        perror("Failed to sync memory to disk");
    }

    // Unmap and close
    munmap(addr, MMAP_FILE_SIZE);
    close(fd);

    printf("Database file created at %s\n", MMAP_FILE_PATH);
    printf("Run the reader and writer programs to interact with the database\n");

    return 0;
}