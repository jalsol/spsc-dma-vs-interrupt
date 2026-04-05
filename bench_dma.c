/* Benchmark: DMA with polling */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "bench_common.h"
#include "common.h"

#define DEVICE_PATH "/dev/dev_dma"

int main(void) {
  int fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  struct dma_shared *mem = mmap(NULL, sizeof(struct dma_shared),
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return 1;
  }

  printf("DMA polling: reading %d items...\n", NUM_OPS);

  uint64_t start = get_ns();
  size_t total = 0;

  while (total < NUM_OPS) {
    /* Poll for data */
    while (mem->read_pos == mem->write_pos)
      __asm__ __volatile__("pause" ::: "memory");

    /* Read from shared memory */
    uint32_t value = mem->buffer[mem->read_pos];
    __sync_synchronize();
    mem->read_pos = next_pos(mem->read_pos);
    total++;

    (void)value; /* Prevent optimization */
  }

  uint64_t elapsed = get_ns() - start;

  print_results(total, elapsed);

  munmap(mem, sizeof(struct dma_shared));
  close(fd);
  return 0;
}
