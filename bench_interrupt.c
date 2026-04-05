/* Benchmark: Interrupt-driven I/O */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/dev_interrupt"
#define NUM_OPS 1000000
#define BATCH_SIZE 1024

static uint64_t get_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(void) {
  int fd = open(DEVICE_PATH, O_RDONLY);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  uint32_t *buf = malloc(BATCH_SIZE * sizeof(uint32_t));
  if (!buf) {
    perror("malloc");
    close(fd);
    return 1;
  }

  printf("Interrupt-driven: reading %d items...\n", NUM_OPS);

  uint64_t start = get_ns();
  size_t total = 0;

  while (total < NUM_OPS) {
    ssize_t n = read(fd, buf, BATCH_SIZE * sizeof(uint32_t));
    if (n <= 0)
      break;
    total += n / sizeof(uint32_t);
  }

  uint64_t elapsed = get_ns() - start;

  printf("\nResults:\n");
  printf("  Operations:  %zu\n", total);
  printf("  Time:        %.3f sec\n", elapsed / 1e9);
  printf("  Throughput:  %.0f ops/sec\n", total / (elapsed / 1e9));
  printf("  Latency:     %.0f ns/op\n", (double)elapsed / total);

  free(buf);
  close(fd);
  return 0;
}
