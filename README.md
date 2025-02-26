# macOS Zero-Copy IPC

A collection of examples demonstrating zero-copy shared memory communication between processes on macOS.

## Overview

This repository contains examples of inter-process communication using zero-copy shared memory on macOS. All examples use POSIX shared memory (shm_open) and memory mapping for efficient data sharing without copying.

## Examples

### 1. Countdown

A simple demonstration where one process counts down from 10 to 0, sending each number to another process that processes it.

### 2. Buffer Transfer

An example of continuous data streaming between processes. One process generates random X,Y coordinates and the other displays them in real-time.

## Building

```bash
# Build all examples
make

# Build a specific example
make countdown
make buffer_transfer
```
