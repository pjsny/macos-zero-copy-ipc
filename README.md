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
2. `process2` connects to the shared memory and signals it's ready
3. `process1` writes data to the shared memory and signals it's done
4. `process2` reads the data directly from the shared memory (zero-copy)
5. Both processes clean up resources

## License

[MIT License](LICENSE)
