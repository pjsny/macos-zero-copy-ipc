#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdatomic.h>
#include <math.h>

// External headers (assumed to provide shared_memory_t, simd_batch_t, vector_tanh(), etc.)
#include "buffer_shared.h"
#include "simd_shared.h"
#include "shared_memory.h"

// Test configuration
#define TEST_ITERATIONS 10000
#define WARMUP_ITERATIONS 1000
#define DATA_SIZE 1024 // Number of floats

// Semaphore names for normal and SIMD benchmarks
#define NORMAL_SEM_READY_NAME BUFFER_SEM_READY_NAME
#define NORMAL_SEM_DONE_NAME BUFFER_SEM_DONE_NAME
#define SIMD_SEM_READY_NAME "/simd_sem_ready"
#define SIMD_SEM_DONE_NAME "/simd_sem_done"

// If not defined in simd_shared.h
#ifndef SIMD_SHM_NAME
#define SIMD_SHM_NAME "/simd_shm"
#endif

// ===== ATTEMPT REAL-TIME POLICY ON MACOS ===== //
static int set_macos_realtime(void)
{
    // This function attempts to set a real-time scheduling policy for the *current* thread.
    // Requires privileges or special entitlements on macOS; may fail if not permitted.
    mach_timebase_info_data_t time_info;
    mach_timebase_info(&time_info);

    // We'll set a ~1ms period, 0.5ms of guaranteed CPU, 0.8ms constraint.
    // Adjust these to tune your real-time window.
    thread_time_constraint_policy_data_t policy;
    double one_millisecond_ns = 1e6; // 1ms in nanoseconds
    uint64_t period_ns = (uint64_t)one_millisecond_ns;
    uint64_t computation_ns = (uint64_t)(0.5 * one_millisecond_ns);
    uint64_t constraint_ns = (uint64_t)(0.8 * one_millisecond_ns);

    // Convert from absolute nanoseconds to mach "ticks"
    policy.period = period_ns * time_info.denom / time_info.numer;
    policy.computation = computation_ns * time_info.denom / time_info.numer;
    policy.constraint = constraint_ns * time_info.denom / time_info.numer;
    policy.preemptible = 1;

    kern_return_t kr = thread_policy_set(
        mach_thread_self(),
        THREAD_TIME_CONSTRAINT_POLICY,
        (thread_policy_t)&policy,
        THREAD_TIME_CONSTRAINT_POLICY_COUNT);
    if (kr != KERN_SUCCESS)
    {
        fprintf(stderr,
                "Warning: Real-time policy not set (kern_return_t=%d). "
                "Ensure you have appropriate privileges.\n",
                kr);
        return -1;
    }
    return 0;
}

// ===== RESULTS ===== //
typedef struct
{
    double total_ms;
    double producer_ms;
    double consumer_ms;
    double throughput;
} normal_buffer_result_t;

typedef struct
{
    double total_ms;
    double producer_ms;
    double consumer_ms;
    double throughput;
    double simd_ops_per_second;
} simd_buffer_result_t;

// ===== TIMING UTILITY ===== //
uint64_t get_time_ns()
{
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0)
    {
        mach_timebase_info(&timebase_info);
    }
    uint64_t time = mach_absolute_time();
    return time * timebase_info.numer / timebase_info.denom;
}

// ===== NORMAL BENCHMARK DATA / ARGS ===== //
typedef struct
{
    float data[DATA_SIZE];
    uint32_t update_count;
} extended_point_data_t;

typedef struct
{
    shared_memory_t shm;
    sem_t *sem_ready;
    sem_t *sem_done;
    volatile int *running;
    extended_point_data_t *point_data;
    uint64_t *consumer_time;
} normal_buffer_args_t;

// ===== SIMD BENCHMARK ARGS ===== //
typedef struct
{
    sem_t *sem_ready;
    sem_t *sem_done;
    volatile int *running;
    simd_batch_t *batch;
    uint64_t *consumer_time;
} simd_buffer_args_t;

// ===== NORMAL CONSUMER THREAD ===== //
void *normal_consumer_thread(void *arg)
{
    // Try real-time scheduling (best-effort).
    set_macos_realtime();

    normal_buffer_args_t *args = (normal_buffer_args_t *)arg;
    extended_point_data_t *point_data = args->point_data;
    uint32_t last_count = UINT32_MAX;
    uint64_t total_process_time = 0;

    while (*(args->running))
    {
        if (sem_wait(args->sem_done) != 0)
        {
            perror("sem_wait");
            break;
        }

        // Check if new data arrived
        if (point_data->update_count != last_count)
        {
            uint64_t start_time = get_time_ns();
            for (int i = 0; i < DATA_SIZE; i++)
            {
                float value = point_data->data[i];
                float processed = tanh(value);
                point_data->data[i] = processed * processed;
            }
            uint64_t end_time = get_time_ns();
            total_process_time += (end_time - start_time);
            last_count = point_data->update_count;
        }

        if (sem_post(args->sem_ready) != 0)
        {
            perror("sem_post");
            break;
        }
    }

    *args->consumer_time = total_process_time;
    return NULL;
}

// ===== SIMD CONSUMER THREAD ===== //
void *simd_consumer_thread(void *arg)
{
    // Try real-time scheduling (best-effort).
    set_macos_realtime();

    simd_buffer_args_t *args = (simd_buffer_args_t *)arg;
    simd_batch_t *batch = args->batch;
    uint64_t total_process_time = 0;

    while (*(args->running))
    {
        if (sem_wait(args->sem_done) != 0)
        {
            perror("sem_wait");
            break;
        }

        uint64_t start_time = get_time_ns();
        for (int i = 0; i < DATA_SIZE; i += 4)
        {
            float32x4_t data = vld1q_f32(&batch->data[i]);
            float32x4_t tanh_val = vector_tanh(data);
            float32x4_t squared = vmulq_f32(tanh_val, tanh_val);
            vst1q_f32(&batch->data[i], squared);
        }
        uint64_t end_time = get_time_ns();
        total_process_time += (end_time - start_time);

        if (sem_post(args->sem_ready) != 0)
        {
            perror("sem_post");
            break;
        }
    }

    *args->consumer_time = total_process_time;
    return NULL;
}

// ===== NORMAL BUFFER BENCHMARK ===== //
normal_buffer_result_t benchmark_normal_buffer(float *random_data)
{
    normal_buffer_result_t result = {0};
    printf("Setting up normal buffer benchmark...\n");

    shared_memory_t shm = {0};
    sem_t *sem_ready, *sem_done;
    sem_unlink(NORMAL_SEM_READY_NAME);
    sem_unlink(NORMAL_SEM_DONE_NAME);
    shm_unlink(BUFFER_SHM_NAME);

    sem_ready = sem_open(NORMAL_SEM_READY_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_done = sem_open(NORMAL_SEM_DONE_NAME, O_CREAT, S_IRUSR | S_IWUSR, 0);
    if (sem_ready == SEM_FAILED || sem_done == SEM_FAILED)
    {
        perror("sem_open");
        goto cleanup_semaphores;
    }

    // Allocate larger than needed for alignment
    if (shared_memory_create(&shm, BUFFER_SHM_NAME,
                             sizeof(extended_point_data_t) + 64) != 0)
    {
        printf("Failed to create shared memory\n");
        goto cleanup_semaphores;
    }

    // Ensure alignment manually
    uintptr_t base_addr = (uintptr_t)shm.addr;
    size_t offset = (64 - (base_addr % 64)) % 64;
    extended_point_data_t *point_data = (extended_point_data_t *)(base_addr + offset);

    // Initialize
    point_data->update_count = 0;
    memset(point_data->data, 0, sizeof(point_data->data));

    pthread_t consumer_thread;
    volatile int running = 1;
    uint64_t consumer_time = 0;

    normal_buffer_args_t args = {
        .shm = shm,
        .sem_ready = sem_ready,
        .sem_done = sem_done,
        .running = &running,
        .point_data = point_data,
        .consumer_time = &consumer_time};

    // Start consumer
    if (pthread_create(&consumer_thread, NULL, normal_consumer_thread, &args) != 0)
    {
        printf("Failed to create consumer thread\n");
        goto cleanup_memory;
    }

    // Warm-up
    printf("Warming up normal buffer (%d iterations)...\n", WARMUP_ITERATIONS);
    for (int i = 0; i < WARMUP_ITERATIONS; i++)
    {
        if (sem_wait(sem_ready) != 0)
        {
            perror("sem_wait");
            break;
        }
        // Copy pre-generated data
        memcpy(point_data->data, &random_data[i * DATA_SIZE], DATA_SIZE * sizeof(float));

        point_data->update_count++;
        if (sem_post(sem_done) != 0)
        {
            perror("sem_post");
            break;
        }
    }

    // Actual test
    printf("Running normal buffer benchmark (%d iterations)...\n", TEST_ITERATIONS);
    uint64_t producer_start_time = get_time_ns();
    uint64_t producer_time = 0;

    // Start index in random_data after warm-up
    int start_index = WARMUP_ITERATIONS * DATA_SIZE;
    for (int i = 0; i < TEST_ITERATIONS; i++)
    {
        if (sem_wait(sem_ready) != 0)
        {
            perror("sem_wait");
            break;
        }
        uint64_t iteration_start = get_time_ns();

        memcpy(point_data->data,
               &random_data[start_index + (i * DATA_SIZE)],
               DATA_SIZE * sizeof(float));
        point_data->update_count++;

        producer_time += (get_time_ns() - iteration_start);

        if (sem_post(sem_done) != 0)
        {
            perror("sem_post");
            break;
        }
    }

    uint64_t end_time = get_time_ns();
    double elapsed_ms = (end_time - producer_start_time) / 1e6;

    running = 0;
    sem_post(sem_ready);
    sem_post(sem_done);
    pthread_join(consumer_thread, NULL);

    // Fill result
    result.total_ms = elapsed_ms;
    result.producer_ms = producer_time / 1e6;
    result.consumer_ms = consumer_time / 1e6;
    result.throughput = (double)TEST_ITERATIONS * DATA_SIZE * sizeof(float) /
                        (elapsed_ms / 1000.0) / (1024.0 * 1024.0);

    printf("\nNormal buffer performance metrics:\n");
    printf("Total time: %.2f ms for %d iterations (%.2f µs/iteration)\n",
           result.total_ms, TEST_ITERATIONS,
           result.total_ms * 1000 / TEST_ITERATIONS);
    printf("Producer time: %.2f ms\n", result.producer_ms);
    printf("Consumer time: %.2f ms\n", result.consumer_ms);
    printf("Data processing rate: %.2f MB/s\n", result.throughput);

cleanup_memory:
    shared_memory_destroy(&shm, 1);

cleanup_semaphores:
    if (sem_ready != SEM_FAILED)
        sem_close(sem_ready);
    if (sem_done != SEM_FAILED)
        sem_close(sem_done);
    sem_unlink(NORMAL_SEM_READY_NAME);
    sem_unlink(NORMAL_SEM_DONE_NAME);

    return result;
}

// ===== SIMD BUFFER BENCHMARK ===== //
simd_buffer_result_t benchmark_simd_buffer(float *random_data)
{
    simd_buffer_result_t result = {0};
    printf("Setting up SIMD buffer benchmark...\n");

    sem_t *sem_ready, *sem_done;
    sem_unlink(SIMD_SEM_READY_NAME);
    sem_unlink(SIMD_SEM_DONE_NAME);
    shm_unlink(SIMD_SHM_NAME);

    int shm_fd = shm_open(SIMD_SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shm_fd == -1)
    {
        perror("shm_open");
        return result;
    }

    size_t shm_size = sizeof(simd_batch_t) + 64;
    if (ftruncate(shm_fd, shm_size) == -1)
    {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SIMD_SHM_NAME);
        return result;
    }

    void *addr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        close(shm_fd);
        shm_unlink(SIMD_SHM_NAME);
        return result;
    }

    uintptr_t base_addr = (uintptr_t)addr;
    size_t offset = (64 - (base_addr % 64)) % 64;
    simd_batch_t *batch = (simd_batch_t *)(base_addr + offset);
    memset(batch, 0, sizeof(simd_batch_t));

    sem_ready = sem_open(SIMD_SEM_READY_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_done = sem_open(SIMD_SEM_DONE_NAME, O_CREAT, S_IRUSR | S_IWUSR, 0);
    if (sem_ready == SEM_FAILED || sem_done == SEM_FAILED)
    {
        perror("sem_open");
        if (sem_ready != SEM_FAILED)
            sem_close(sem_ready);
        if (sem_done != SEM_FAILED)
            sem_close(sem_done);
        munmap(addr, shm_size);
        close(shm_fd);
        shm_unlink(SIMD_SHM_NAME);
        return result;
    }

    pthread_t consumer_thread;
    volatile int running = 1;
    uint64_t consumer_time = 0;

    simd_buffer_args_t args = {
        .sem_ready = sem_ready,
        .sem_done = sem_done,
        .running = &running,
        .batch = batch,
        .consumer_time = &consumer_time};

    // Start SIMD consumer
    if (pthread_create(&consumer_thread, NULL, simd_consumer_thread, &args) != 0)
    {
        printf("Failed to create SIMD consumer thread\n");
        munmap(addr, shm_size);
        close(shm_fd);
        shm_unlink(SIMD_SHM_NAME);
        return result;
    }

    // Warm-up
    printf("Warming up SIMD buffer (%d iterations)...\n", WARMUP_ITERATIONS);
    for (int i = 0; i < WARMUP_ITERATIONS; i++)
    {
        if (sem_wait(sem_ready) != 0)
        {
            perror("sem_wait");
            break;
        }
        memcpy(batch->data, &random_data[i * DATA_SIZE], DATA_SIZE * sizeof(float));
        batch->batch_id = i;
        if (sem_post(sem_done) != 0)
        {
            perror("sem_post");
            break;
        }
    }

    // Actual test
    printf("Running SIMD buffer benchmark (%d iterations)...\n", TEST_ITERATIONS);
    uint64_t producer_start_time = get_time_ns();
    uint64_t producer_time = 0;
    int start_index = WARMUP_ITERATIONS * DATA_SIZE;

    for (int i = 0; i < TEST_ITERATIONS; i++)
    {
        if (sem_wait(sem_ready) != 0)
        {
            perror("sem_wait");
            break;
        }

        uint64_t iteration_start = get_time_ns();
        memcpy(batch->data,
               &random_data[start_index + (i * DATA_SIZE)],
               DATA_SIZE * sizeof(float));
        batch->batch_id = WARMUP_ITERATIONS + i;
        producer_time += (get_time_ns() - iteration_start);

        if (sem_post(sem_done) != 0)
        {
            perror("sem_post");
            break;
        }
    }

    uint64_t end_time = get_time_ns();
    double elapsed_ms = (end_time - producer_start_time) / 1e6;

    running = 0;
    sem_post(sem_ready);
    sem_post(sem_done);
    pthread_join(consumer_thread, NULL);

    // Compute performance metrics
    double ops_per_iteration = DATA_SIZE / 4.0; // ~ # of 4-wide SIMD ops
    double simd_ops_per_second =
        (ops_per_iteration * TEST_ITERATIONS) / (elapsed_ms / 1000.0);

    result.total_ms = elapsed_ms;
    result.producer_ms = producer_time / 1e6;
    result.consumer_ms = consumer_time / 1e6;
    result.throughput = (double)TEST_ITERATIONS * DATA_SIZE * sizeof(float) /
                        (elapsed_ms / 1000.0) / (1024.0 * 1024.0);
    result.simd_ops_per_second = simd_ops_per_second;

    printf("\nSIMD buffer performance metrics:\n");
    printf("Total time: %.2f ms for %d iterations (%.2f µs/iteration)\n",
           result.total_ms, TEST_ITERATIONS,
           result.total_ms * 1000 / TEST_ITERATIONS);
    printf("Producer time: %.2f ms\n", result.producer_ms);
    printf("Consumer time: %.2f ms\n", result.consumer_ms);
    printf("SIMD operations: %.2f million ops/second\n",
           result.simd_ops_per_second / 1e6);
    printf("Data processing rate: %.2f MB/s\n", result.throughput);

    munmap(addr, shm_size);
    close(shm_fd);
    shm_unlink(SIMD_SHM_NAME);
    sem_close(sem_ready);
    sem_close(sem_done);
    sem_unlink(SIMD_SEM_READY_NAME);
    sem_unlink(SIMD_SEM_DONE_NAME);

    return result;
}

// ===== PRINT COMPARISON ===== //
void print_comparison_results(double normal_total, double normal_producer, double normal_consumer,
                              double simd_total, double simd_producer, double simd_consumer,
                              double normal_throughput, double simd_throughput)
{
    printf("\n================================================\n");
    printf("            BENCHMARK COMPARISON RESULTS         \n");
    printf("================================================\n");
    printf("Metric                   | Normal Buffer | SIMD Buffer | Improvement\n");
    printf("--------------------------|--------------|------------|------------\n");
    printf("Total time (ms)          | %12.2f | %10.2f | %10.2f%%\n",
           normal_total, simd_total,
           normal_total > 0 ? (normal_total - simd_total) * 100.0 / normal_total : 0);
    printf("Time per iteration (µs)  | %12.2f | %10.2f | %10.2f%%\n",
           normal_total * 1000.0 / TEST_ITERATIONS,
           simd_total * 1000.0 / TEST_ITERATIONS,
           normal_total > 0 ? (normal_total - simd_total) * 100.0 / normal_total : 0);
    printf("Producer time (ms)       | %12.2f | %10.2f |    N/A\n",
           normal_producer, simd_producer);
    printf("Consumer time (ms)       | %12.2f | %10.2f |    N/A\n",
           normal_consumer, simd_consumer);
    printf("Throughput (MB/s)        | %12.2f | %10.2f |    N/A\n",
           normal_throughput, simd_throughput);
    printf("================================================\n");
}

// ===== MAIN ===== //
int main(void)
{
    set_macos_realtime();

    // Pre-generate random data for both normal & SIMD tests
    // so each iteration doesn't call rand() in a tight loop.
    static float random_data[(TEST_ITERATIONS + WARMUP_ITERATIONS) * DATA_SIZE];

    // Use a single fixed seed for reproducibility
    srand(12345);

    // Fill the pre-generated array with random floats in [-1, 1]
    for (int i = 0; i < (TEST_ITERATIONS + WARMUP_ITERATIONS) * DATA_SIZE; i++)
    {
        random_data[i] = (float)(((double)rand() / (double)RAND_MAX) * 2.0 - 1.0);
    }

    printf("===== SHARED MEMORY BUFFER BENCHMARK =====\n");
    printf("Comparing normal approach vs. SIMD implementation\n");
    printf("Test configuration: %d iterations, %d float data size\n",
           TEST_ITERATIONS, DATA_SIZE);
    printf("--------------------------------------------\n");

    printf("\n[1/2] NORMAL BUFFER BENCHMARK\n");
    normal_buffer_result_t normal_result =
        benchmark_normal_buffer(random_data);

    printf("Cleaning up resources...");
    sleep(1);

    printf("\n[2/2] SIMD BUFFER BENCHMARK\n");
    simd_buffer_result_t simd_result =
        benchmark_simd_buffer(random_data);

    print_comparison_results(
        normal_result.total_ms, normal_result.producer_ms, normal_result.consumer_ms,
        simd_result.total_ms, simd_result.producer_ms, simd_result.consumer_ms,
        normal_result.throughput, simd_result.throughput);

    printf("=== BENCHMARK COMPLETE ===");
    return 0;
}
