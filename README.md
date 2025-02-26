# macOS Shared Memory Example

A simple demonstration of zero-copy shared memory communication between two processes on macOS using POSIX shared memory.

## Overview

This project demonstrates how to implement shared memory communication between two processes with zero-copy reads and writes. It uses:

- POSIX shared memory (`shm_open`)
- POSIX semaphores for synchronization
- Memory mapping for zero-copy access

## Building

```bash
# Build everything
make

# Build specific targets
make process1
make process2
```

## Running

You need to run the processes in separate terminal windows:

```bash
# Terminal 1
make run1
# or
./process1

# Terminal 2
make run2
# or
./process2
```

## Cleaning Up

```bash
# Clean object files and executables
make clean

# Clean any leftover shared memory objects
make clean-shm
```

## How It Works

1. `process1` creates a shared memory region and waits for `process2` to connect
2. `process2` connects to the shared memory and begins a countdown from 10 to 0
3. For each number in the countdown, `process2` writes it to shared memory
4. `process1` reads each number, processes it (multiplies by 2), and waits for the next one
5. When `process1` receives 0, it completes processing and cleans up
6. Both processes eventually clean up their resources

## Demo Timeline

When you run the demo, expect the following sequence:

1. Start `process1` - it will wait for a connection
2. Start `process2` - it connects to the shared memory
3. `process2` begins counting down from 10 to 0, sending each number to `process1`
4. `process1` reads each number, processes it, and outputs the result
5. After `process2` sends 0, it completes and cleans up
6. When `process1` receives 0, it also completes and cleans up
