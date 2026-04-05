#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define NUM_OPS 100000000

static inline uint64_t get_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static inline void print_results(size_t total, uint64_t elapsed) {
  printf("\nResults:\n");
  printf("  Operations:  %zu\n", total);
  printf("  Time:        %.3f sec\n", elapsed / 1e9);
  printf("  Throughput:  %.0f ops/sec\n", total / (elapsed / 1e9));
  printf("  Latency:     %.0f ns/op\n", (double)elapsed / total);
}

#endif
