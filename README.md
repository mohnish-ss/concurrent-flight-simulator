# Concurrent Flight Systems Simulator

A multithreaded C simulation of interacting flight systems that consume and produce shared resources. The public implementation uses POSIX worker threads, synchronized resource access, atomic control state, and a heap-backed priority event queue.

## Technical Highlights

- Runs a manager thread alongside one worker thread per simulated subsystem.
- Protects every shared resource snapshot and mutation with process-local synchronization.
- Uses unnamed POSIX semaphores on supporting platforms and a pthread-mutex fallback on macOS.
- Stores alerts in a dynamically growing binary heap, ordered by priority and then FIFO sequence.
- Uses C11 atomics for lifecycle and subsystem operating modes.
- Avoids holding resource and event-queue locks at the same time.

## Architecture

`src/simulation.c` owns the manager/worker lifecycle and synchronized resource operations. `src/alert_buffer.c` implements the priority heap. `src/sync_lock.c` provides the platform synchronization abstraction. Public structures and interfaces are declared in `include/flight_sim.h`, while `src/main.c` provides the executable entry point.

This publication-focused implementation is independently structured and does not include external scaffolding from the original development environment.

## Building

```bash
make
```

## Running

```bash
./flight-simulator
```

## Testing

Verify priority and FIFO queue behavior:

```bash
make test
```

Run a complete simulation with AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
make asan
```

Run a complete simulation with ThreadSanitizer:

```bash
make tsan
```

Remove generated files with `make clean`.

## Technical Concepts

- C
- POSIX Threads
- Semaphores
- Mutexes
- C11 atomics
- Concurrency and synchronization
- Priority queues
- Dynamic memory management

## Authors

The original simulator was developed collaboratively by Mohnish Sheth and Kareem Mamooun using pair programming. This independently structured public implementation preserves that joint attribution.
