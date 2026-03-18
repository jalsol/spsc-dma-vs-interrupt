/*
 * Benchmark for DMA Device with Polling
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/dev_dma"
#define BUFFER_SIZE 4096
#define NUM_OPERATIONS 1000000

struct dma_shared {
  volatile uint32_t write_pos;
  volatile uint32_t read_pos;
  volatile uint32_t padding[14];
  uint32_t buffer[BUFFER_SIZE];
} __attribute__((aligned(64)));

static inline uint32_t next_pos(uint32_t pos) {
  return (pos + 1) & (BUFFER_SIZE - 1);
}

static inline int buffer_empty(struct dma_shared *mem) {
  return mem->read_pos == mem->write_pos;
}

static inline uint64_t get_nanoseconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(void) {
  int fd;
  struct dma_shared *mem;
  size_t total_read = 0;
  uint64_t start, end;
  uint32_t value;
  long dma_count_start, dma_count_end;
  uint64_t poll_count = 0;

  printf("=== DMA Device with Polling Benchmark ===\n\n");

  fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("Failed to open device");
    return 1;
  }

  mem = mmap(NULL, sizeof(struct dma_shared), PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED) {
    perror("Failed to mmap");
    close(fd);
    return 1;
  }

  /* Reset counters */
  ioctl(fd, 1, 0);
  ioctl(fd, 2, 0);

  printf("Reading %d values from DMA device (polling)...\n", NUM_OPERATIONS);

  dma_count_start = ioctl(fd, 0, 0);
  start = get_nanoseconds();

  while (total_read < NUM_OPERATIONS) {
    /* Poll for data - no interrupts, direct memory access */
    while (buffer_empty(mem)) {
      poll_count++;
      __asm__ __volatile__("pause" ::: "memory");
    }

    /* Read directly from DMA buffer */
    value = mem->buffer[mem->read_pos];
    __sync_synchronize();
    mem->read_pos = next_pos(mem->read_pos);

    total_read++;
  }

  end = get_nanoseconds();
  dma_count_end = ioctl(fd, 0, 0);

  munmap(mem, sizeof(struct dma_shared));
  close(fd);

  double time_sec = (end - start) / 1000000000.0;
  double throughput = total_read / time_sec;
  double latency_ns = (end - start) / (double)total_read;
  long total_dmas = dma_count_end - dma_count_start;

  printf("\n=== Results ===\n");
  printf("Operations:        %zu\n", total_read);
  printf("Time:              %.3f seconds\n", time_sec);
  printf("Throughput:        %.0f ops/sec\n", throughput);
  printf("Latency per op:    %.2f ns\n", latency_ns);
  printf("Total DMA writes:  %ld\n", total_dmas);
  printf("Poll iterations:   %lu\n", poll_count);
  printf("Polls per op:      %.2f\n", (double)poll_count / total_read);

  return 0;
}
