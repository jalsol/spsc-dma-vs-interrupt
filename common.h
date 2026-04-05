#ifndef COMMON_H
#define COMMON_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define BUFFER_SIZE 4096

struct dma_shared {
  volatile uint32_t write_pos;
  uint8_t pad1[60];
  volatile uint32_t read_pos;
  uint8_t pad2[60];
  uint32_t buffer[BUFFER_SIZE];
} __attribute__((aligned(64)));

static inline uint32_t next_pos(uint32_t pos) {
  return (pos + 1) % BUFFER_SIZE;
}

#endif
