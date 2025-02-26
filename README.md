# macOS Zero-Copy IPC

A collection of examples demonstrating zero-copy shared memory communication between processes on macOS.

## Overview

This repository contains examples of inter-process communication using zero-copy shared memory on macOS. The examples demonstrate different approaches to shared memory:

- POSIX shared memory (shm_open) with memory mapping
- Memory-mapped files (mmap) for persistent storage
- Lock-free synchronization with atomic operations
- SIMD-accelerated processing with Apple Silicon optimizations

Each approach has different performance characteristics and use cases.

## Examples

### 1. Countdown

A simple demonstration where one process counts down from 10 to 0, sending each number to another process that processes it. Uses POSIX shared memory (shm_open) and semaphores for synchronization.

### 2. Buffer Transfer

An example of continuous data streaming between processes. One process generates random X,Y coordinates and the other displays them in real-time. Uses POSIX shared memory (shm_open) and semaphores for synchronization.

### 3. Lock-Free Ring Buffer

A high-performance lock-free ring buffer implementation using atomic operations for thread-safe communication without mutexes. Demonstrates efficient producer-consumer pattern with non-blocking I/O. Uses POSIX shared memory (shm_open) but replaces semaphores with atomic operations.

### 4. Memory-Mapped File Database

A persistent shared memory example using memory-mapped files (mmap) with regular files. Demonstrates how to create a simple database that persists between program executions and can be accessed by multiple processes simultaneously. Uses pthread mutexes for synchronization.

### 5. SIMD-Accelerated Processing

A high-performance example optimized for Apple Silicon (M-series) processors. Uses SIMD vector instructions (ARM NEON) to process data in parallel, huge pages for better TLB efficiency, and cache-line alignment to prevent false sharing. Demonstrates how to achieve maximum performance on modern hardware.

## Cloning the Repository

This repository uses Git submodules for external dependencies. To clone the repository with all submodules:

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/pjsny/macos-zero-copy-ipc.git

# Or if you already cloned without --recursive:
git submodule init
git submodule update
```

## Building

```bash
# Build all examples
make

# Build a specific example
make countdown
make buffer_transfer
make ring_buffer
make mmap_file
make simd_processing
```

## Credits

The SIMD-Accelerated Processing example uses the [math_intrinsics](https://github.com/Geolm/math_intrinsics) library by Geolm for efficient SIMD math operations. This excellent library provides transcendental math functions (cos, sin, acos, etc.) implemented using 100% AVX/Neon instructions with no branching.
