/* Benchmark: DMA with polling */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>

#include "bench_common.h"
#include "common.h"

#define DEVICE_PATH "/dev/dev_dma"
#define SPIN_BEFORE_YIELD 2048

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
    uint32_t local_read_pos = __atomic_load_n(&mem->read_pos, __ATOMIC_RELAXED);
    uint32_t local_write_pos =
        __atomic_load_n(&mem->write_pos, __ATOMIC_ACQUIRE);
    uint32_t spin_count = 0;

    while (local_read_pos == local_write_pos) {
      __asm__ __volatile__("pause" ::: "memory");
      if (++spin_count == SPIN_BEFORE_YIELD) {
        sched_yield();
        spin_count = 0;
      }
      local_read_pos = __atomic_load_n(&mem->read_pos, __ATOMIC_RELAXED);
      local_write_pos = __atomic_load_n(&mem->write_pos, __ATOMIC_ACQUIRE);
    }

    uint32_t value = mem->buffer[local_read_pos];
    __atomic_store_n(&mem->read_pos, next_pos(local_read_pos), __ATOMIC_RELEASE);
    total++;

    (void)value;
  }

  uint64_t elapsed = get_ns() - start;

  print_results(total, elapsed);

  munmap(mem, sizeof(struct dma_shared));
  close(fd);
  return 0;
}
