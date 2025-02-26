#ifndef MMAP_SHARED_H
#define MMAP_SHARED_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// File path for the memory-mapped file
#define MMAP_FILE_PATH "/tmp/mmap_shared_example.dat"
#define MMAP_FILE_SIZE (1024 * 1024) // 1MB

// Structure for a record in our database
typedef struct
{
    uint32_t id;
    char name[64];
    double value;
    bool is_active;
} record_t;

// Structure for our memory-mapped database
typedef struct
{
    pthread_mutex_t mutex; // For synchronization between processes
    uint32_t record_count; // Number of records in the database
    uint32_t max_records;  // Maximum number of records that can be stored
    record_t records[];    // Flexible array member for records
} mmap_database_t;

// Helper functions
#define MAX_RECORDS(size) ((size - sizeof(mmap_database_t)) / sizeof(record_t))

#endif // MMAP_SHARED_H