#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include "mmap_shared.h"

// Flag for clean shutdown
volatile sig_atomic_t running = 1;

// Signal handler for Ctrl+C
void handle_sigint(int sig)
{
    (void)sig; // Suppress unused parameter warning
    running = 0;
}

int main()
{
    printf("Starting memory-mapped database reader\n");

    // Set up signal handler for clean shutdown
    signal(SIGINT, handle_sigint);

    // Open the file
    int fd = open(MMAP_FILE_PATH, O_RDONLY);
    if (fd == -1)
    {
        perror("Failed to open file");
        printf("Make sure to run db_creator first\n");
        return 1;
    }

    // Get file size
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("Failed to get file size");
        close(fd);
        return 1;
    }

    // Map the file into memory (read-only)
    void *addr = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("Failed to map file");
        close(fd);
        return 1;
    }

    // Get database pointer
    mmap_database_t *db = (mmap_database_t *)addr;

    printf("Connected to database with %u/%u records\n",
           db->record_count, db->max_records);
    printf("Press Ctrl+C to exit\n\n");

    // Monitor for changes
    uint32_t last_count = db->record_count;

    while (running)
    {
        // Check if record count has changed
        if (db->record_count != last_count)
        {
            printf("Database updated: %u records (added %u)\n",
                   db->record_count, db->record_count - last_count);

            // Print the new records
            for (uint32_t i = last_count; i < db->record_count; i++)
            {
                record_t *record = &db->records[i];
                printf("  Record #%u: ID=%u, Name='%s', Value=%.2f, Active=%s\n",
                       i, record->id, record->name, record->value,
                       record->is_active ? "true" : "false");
            }

            last_count = db->record_count;
            printf("\n");
        }

        // Sleep to avoid busy waiting
        usleep(500000); // 500ms
    }

    printf("\nShutting down...\n");

    // Clean up
    munmap(addr, sb.st_size);
    close(fd);

    printf("Reader process completed\n");

    return 0;
}