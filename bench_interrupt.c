/*
 * Benchmark for Interrupt-Driven Device
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/dev_interrupt"
#define NUM_OPERATIONS 1000000
#define BATCH_SIZE 1024

static inline uint64_t get_nanoseconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(void) {
  int fd;
  uint32_t *buffer;
  ssize_t ret;
  size_t total_read = 0;
  uint64_t start, end;
  long irq_count_start, irq_count_end;

  printf("=== Interrupt-Driven Device Benchmark ===\n\n");

  buffer = malloc(BATCH_SIZE * sizeof(uint32_t));
  if (!buffer) {
    perror("malloc failed");
    return 1;
  }

  fd = open(DEVICE_PATH, O_RDONLY);
  if (fd < 0) {
    perror("Failed to open device");
    free(buffer);
    return 1;
  }

  /* Reset IRQ counter */
  ioctl(fd, 1, 0);

  printf("Reading %d values from interrupt-driven device...\n", NUM_OPERATIONS);

  irq_count_start = ioctl(fd, 0, 0);
  start = get_nanoseconds();

  while (total_read < NUM_OPERATIONS) {
    ret = read(fd, buffer, BATCH_SIZE * sizeof(uint32_t));
    if (ret < 0) {
      perror("read failed");
      break;
    }
    if (ret == 0) {
      printf("No more data\n");
      break;
    }
    total_read += ret / sizeof(uint32_t);
  }

  end = get_nanoseconds();
  irq_count_end = ioctl(fd, 0, 0);

  close(fd);
  free(buffer);

  double time_sec = (end - start) / 1000000000.0;
  double throughput = total_read / time_sec;
  double latency_ns = (end - start) / (double)total_read;
  long total_irqs = irq_count_end - irq_count_start;
  double irqs_per_op = (double)total_irqs / total_read;

  printf("\n=== Results ===\n");
  printf("Operations:        %zu\n", total_read);
  printf("Time:              %.3f seconds\n", time_sec);
  printf("Throughput:        %.0f ops/sec\n", throughput);
  printf("Latency per op:    %.2f ns\n", latency_ns);
  printf("Total interrupts:  %ld\n", total_irqs);
  printf("Interrupts/op:     %.2f\n", irqs_per_op);

  return 0;
}
