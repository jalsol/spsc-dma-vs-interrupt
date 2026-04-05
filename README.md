# DMA vs Interrupt Benchmark

Demonstrates that DMA with polling is faster than interrupt-driven I/O for frequent operations.

## Build & Run

```bash
make all        # Build everything
make run        # Run benchmarks (needs sudo)
```

**If modules are stuck:** Reboot your system. The kernel threads may not exit cleanly.

## Usage

```bash
# Simple way
make run

# Manual control
make load          # Load modules
./bench_interrupt  # Run interrupt test
./bench_dma        # Run DMA test
make unload        # Unload modules
```

## Expected Results

```
Interrupt-driven:  ~10-15M ops/sec, ~70-100 ns/op
DMA polling:       ~50M ops/sec, ~20 ns/op
Speedup:           4-5x faster
```

## Why DMA Wins

- Interrupt: syscall + context switch = ~100-700 ns overhead
- DMA: direct memory access = ~20 ns overhead

This is the principle behind DPDK, SPDK, and RDMA.

## Troubleshooting

**"Device or resource busy"**: Module already loaded
```bash
sudo rmmod device_interrupt device_dma
```

**"Invalid module format"**: Kernel version mismatch (6.19.6 vs 6.19.8)
- Solution: Reboot to newer kernel, or
- The Makefile uses `insmod -f` to force load

**Module won't unload**: Kernel thread stuck
- Solution: Reboot system
