/* Benchmark: Interrupt-driven I/O */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bench_common.h"

#define DEVICE_PATH "/dev/dev_interrupt"
#define BATCH_SIZE 1024

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

  print_results(total, elapsed);

  free(buf);
  close(fd);
  return 0;
}
