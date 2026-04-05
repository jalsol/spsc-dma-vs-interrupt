/* Benchmark: DMA with polling */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/dev_dma"
#define BUFFER_SIZE 4096
#define NUM_OPS 1000000

struct dma_shared {
  volatile uint32_t write_pos;
  volatile uint32_t read_pos;
  uint32_t buffer[BUFFER_SIZE];
} __attribute__((aligned(64)));

static inline uint32_t next_pos(uint32_t pos) {
  return (pos + 1) % BUFFER_SIZE;
}

static uint64_t get_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

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

  printf("\nResults:\n");
  printf("  Operations:  %zu\n", total);
  printf("  Time:        %.3f sec\n", elapsed / 1e9);
  printf("  Throughput:  %.0f ops/sec\n", total / (elapsed / 1e9));
  printf("  Latency:     %.0f ns/op\n", (double)elapsed / total);

  munmap(mem, sizeof(struct dma_shared));
  close(fd);
  return 0;
}
