/*
 * Polling device: generates data directly to shared memory, userspace polls
 */
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>

#include "common.h"

#define DEVICE_NAME "dev_polling"

struct device_state {
  struct polling_shared *shared;
  struct page *pages;
  struct task_struct *producer;
  atomic_t running;
};

static dev_t dev_num;
static struct class *dev_class;
static struct cdev dev_cdev;
static struct device_state *state;

static inline bool buffer_full(void) {
  uint32_t write_pos = READ_ONCE(state->shared->write_pos);
  uint32_t read_pos = READ_ONCE(state->shared->read_pos);
  return (write_pos - read_pos) >= BUFFER_SIZE;
}

static int producer_thread(void *data) {
  uint32_t value = 0;
  while (!kthread_should_stop() && atomic_read(&state->running)) {
    uint32_t write_pos = READ_ONCE(state->shared->write_pos);
    uint32_t read_pos = smp_load_acquire(&state->shared->read_pos);

    if ((write_pos - read_pos) < BUFFER_SIZE) {
      state->shared->buffer[ring_index(write_pos)] = value++;
      smp_store_release(&state->shared->write_pos, next_pos(write_pos));
    }
    if (value % 1000 == 0)
      usleep_range(1, 10);
  }
  return 0;
}

static int dev_mmap(struct file *f, struct vm_area_struct *vma) {
  unsigned long pfn = virt_to_phys(state->shared) >> PAGE_SHIFT;
  if (remap_pfn_range(vma, vma->vm_start, pfn, vma->vm_end - vma->vm_start,
                      vma->vm_page_prot))
    return -EAGAIN;
  return 0;
}

static int dev_open(struct inode *inode, struct file *file) {
  atomic_set(&state->running, 1);
  state->producer = kthread_run(producer_thread, NULL, "producer");
  return IS_ERR(state->producer) ? PTR_ERR(state->producer) : 0;
}

static int dev_release(struct inode *inode, struct file *file) {
  atomic_set(&state->running, 0);
  if (state->producer) {
    kthread_stop(state->producer);
    state->producer = NULL;
  }
  return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .mmap = dev_mmap,
};

static int __init dev_init(void) {
  unsigned long addr;

  state = kzalloc(sizeof(*state), GFP_KERNEL);
  if (!state)
    return -ENOMEM;

  addr = __get_free_pages(GFP_KERNEL, get_order(sizeof(struct polling_shared)));
  if (!addr)
    goto fail_alloc;

  state->shared = (struct polling_shared *)addr;
  state->pages = virt_to_page(addr);
  SetPageReserved(state->pages);

  if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0)
    goto fail_chrdev;

  cdev_init(&dev_cdev, &fops);
  if (cdev_add(&dev_cdev, dev_num, 1) < 0)
    goto fail_cdev;

  dev_class = class_create(DEVICE_NAME);
  if (IS_ERR(dev_class))
    goto fail_class;

  if (IS_ERR(device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME)))
    goto fail_device;

  pr_info("Polling device loaded\n");
  return 0;

fail_device:
  class_destroy(dev_class);
fail_class:
  cdev_del(&dev_cdev);
fail_cdev:
  unregister_chrdev_region(dev_num, 1);
fail_chrdev:
  ClearPageReserved(state->pages);
  free_pages(addr, get_order(sizeof(struct polling_shared)));
fail_alloc:
  kfree(state);
  return -1;
}

static void __exit dev_exit(void) {
  unsigned long addr = (unsigned long)state->shared;
  if (state->producer)
    kthread_stop(state->producer);
  device_destroy(dev_class, dev_num);
  class_destroy(dev_class);
  cdev_del(&dev_cdev);
  unregister_chrdev_region(dev_num, 1);
  ClearPageReserved(state->pages);
  free_pages(addr, get_order(sizeof(struct polling_shared)));
  kfree(state);
}

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Polling shared-memory SPSC benchmark device");
