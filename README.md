# SPSC Queue Benchmark: Polling vs Interrupts

Fair benchmark comparing two kernel-userspace communication patterns for Single Producer Single Consumer (SPSC) workloads, processing one element at a time.

## Approaches

### 1. DMA with Polling (`device_dma` / `bench_dma`)
- Kernel writes directly to shared memory
- Userspace polls for new data
- Zero syscalls in the hot path
- Wastes CPU cycles while waiting

### 2. Interrupt-driven I/O (`device_interrupt` / `bench_interrupt`)
- Kernel buffers data internally
- Userspace calls `read()` for each element
- Blocks efficiently using wait queues
- 1 million syscalls = 1 million context switches

## Code Structure

```
common.h         - Shared definitions (BUFFER_SIZE, dma_shared struct, next_pos())
bench_common.h   - Benchmark utilities (timing, result printing)
device_dma.c     - Kernel module: shared memory producer
device_interrupt.c - Kernel module: interrupt-driven producer
bench_dma.c      - Userspace: polling consumer
bench_interrupt.c - Userspace: syscall-based consumer
```

## Build & Run

```bash
make all        # Build kernel modules and benchmarks
make run        # Run both benchmarks (needs sudo)
```

**If modules are stuck:** Reboot your system. The kernel threads may not exit cleanly.

## Manual Usage

```bash
make load          # Load kernel modules
./bench_interrupt  # Run interrupt benchmark
./bench_dma        # Run polling benchmark
make unload        # Unload modules
```

## Expected Results

```
Interrupt-driven:  ~1-2M ops/sec, ~500-1000 ns/op
DMA polling:       ~50-100M ops/sec, ~10-20 ns/op
Speedup:           50-100x faster
```

## Why Polling Wins (for High Frequency)

**Per-operation overhead:**
- Interrupt: syscall + context switch + copy_to_user = ~500-1000 ns
- Polling: memory read + cache line fetch = ~10-20 ns

**Tradeoff:**
- Polling wastes CPU cycles (100% core usage even when idle)
- Interrupts save CPU but pay syscall overhead on every operation

This is why DPDK, SPDK, and RDMA use polling for high-throughput workloads.

## Implementation Details

### Cache Line Padding
The `dma_shared` struct uses 60-byte padding to separate `write_pos` and `read_pos` onto different cache lines (64 bytes), preventing false sharing between producer and consumer.

```c
struct dma_shared {
  volatile uint32_t write_pos;  // Producer's cursor
  uint8_t pad1[60];             // Padding to next cache line
  volatile uint32_t read_pos;   // Consumer's cursor
  uint8_t pad2[60];             // Padding to next cache line
  uint32_t buffer[BUFFER_SIZE]; // Ring buffer data
} __attribute__((aligned(64)));
```

### Fairness
Both benchmarks process **1 element per iteration** (1 million iterations total). This shows the true per-operation cost without batching advantages.

## Troubleshooting

**"Device or resource busy"**: Module already loaded
```bash
sudo rmmod device_interrupt device_dma
```

**"Invalid module format"**: Kernel version mismatch
- Solution: Reboot to newer kernel, or
- The Makefile uses `insmod -f` to force load

**Module won't unload**: Kernel thread stuck
- Solution: Reboot system

**Benchmark stalls**: Struct layout mismatch between kernel and userspace
- Solution: Ensure both use `common.h` and rebuild everything
