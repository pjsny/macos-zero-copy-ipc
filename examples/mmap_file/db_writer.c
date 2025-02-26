#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "mmap_shared.h"

// Function to add a record to the database
int add_record(mmap_database_t *db, const char *name, double value)
{
    // Check if database is full
    if (db->record_count >= db->max_records)
    {
        printf("Error: Database is full\n");
        return -1;
    }

    // Lock the mutex
    pthread_mutex_lock(&db->mutex);

    // Create a new record
    record_t *record = &db->records[db->record_count];
    record->id = db->record_count + 1;
    strncpy(record->name, name, sizeof(record->name) - 1);
    record->name[sizeof(record->name) - 1] = '\0'; // Ensure null termination
    record->value = value;
    record->is_active = true;

    // Increment record count
    db->record_count++;

    // Unlock the mutex
    pthread_mutex_unlock(&db->mutex);

    return 0;
}

int main()
{
    printf("Starting memory-mapped database writer\n");

    // Open the file
    int fd = open(MMAP_FILE_PATH, O_RDWR);
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

    // Map the file into memory
    void *addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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

    // Seed random number generator
    srand(time(NULL));

    // Interactive menu
    char choice;
    char name[64];
    double value;

    do
    {
        printf("\nDatabase Writer Menu:\n");
        printf("1. Add random record\n");
        printf("2. Add custom record\n");
        printf("3. Show record count\n");
        printf("q. Quit\n");
        printf("Choice: ");

        scanf(" %c", &choice);

        switch (choice)
        {
        case '1':
            // Generate random record
            snprintf(name, sizeof(name), "Random Record %d", rand() % 1000);
            value = (rand() % 10000) / 100.0;

            if (add_record(db, name, value) == 0)
            {
                printf("Added random record: '%s' with value %.2f\n", name, value);

                // Sync changes to disk
                if (msync(addr, sb.st_size, MS_SYNC) == -1)
                {
                    perror("Failed to sync memory to disk");
                }
            }
            break;

        case '2':
            // Get custom record details
            printf("Enter record name: ");
            scanf(" %63s", name);

            printf("Enter record value: ");
            scanf("%lf", &value);

            if (add_record(db, name, value) == 0)
            {
                printf("Added custom record: '%s' with value %.2f\n", name, value);

                // Sync changes to disk
                if (msync(addr, sb.st_size, MS_SYNC) == -1)
                {
                    perror("Failed to sync memory to disk");
                }
            }
            break;

        case '3':
            printf("Current record count: %u/%u\n", db->record_count, db->max_records);
            break;

        case 'q':
        case 'Q':
            printf("Exiting...\n");
            break;

        default:
            printf("Invalid choice\n");
        }

    } while (choice != 'q' && choice != 'Q');

    // Clean up
    munmap(addr, sb.st_size);
    close(fd);

    printf("Writer process completed\n");

    return 0;
}