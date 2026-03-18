/*
 * DMA-Based Device Simulator with Polling
 * Simulates device DMA to main memory, polled by userspace
 */

#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "dev_dma"
#define BUFFER_SIZE 4096

/* Shared memory structure - DMA target */
struct dma_shared {
  volatile uint32_t write_pos;   /* Updated by "device" */
  volatile uint32_t read_pos;    /* Updated by userspace */
  volatile uint32_t padding[14]; /* Cache line padding */
  uint32_t buffer[BUFFER_SIZE];
} __attribute__((aligned(64)));

struct dma_device {
  struct dma_shared *shared_mem;
  struct page *shared_pages;
  struct task_struct *dma_thread;
  atomic_t running;
  atomic_t dma_count;
};

static dev_t dev_num;
static struct class *dev_class;
static struct cdev dev_cdev;
static struct dma_device *dev;

static inline uint32_t next_pos(uint32_t pos) {
  return (pos + 1) & (BUFFER_SIZE - 1);
}

static inline int buffer_full(void) {
  return next_pos(dev->shared_mem->write_pos) == dev->shared_mem->read_pos;
}

/* Background thread simulating device DMA operations */
static int device_dma_thread(void *data) {
  uint32_t value = 0;

  while (!kthread_should_stop() && atomic_read(&dev->running)) {
    if (!buffer_full()) {
      /* Simulate DMA write directly to shared memory */
      dev->shared_mem->buffer[dev->shared_mem->write_pos] = value++;

      /* Memory barrier to ensure write is visible */
      smp_wmb();

      /* Update write position (simulates device register update) */
      dev->shared_mem->write_pos = next_pos(dev->shared_mem->write_pos);

      atomic_inc(&dev->dma_count);
    } else {
      /* Device would stall or drop data */
      usleep_range(1, 10);
    }

    /* Simulate continuous DMA transfers */
    if (value % 100 == 0)
      cond_resched();
  }

  return 0;
}

static int dev_mmap(struct file *filp, struct vm_area_struct *vma) {
  unsigned long size = vma->vm_end - vma->vm_start;
  unsigned long pfn;

  if (size > PAGE_ALIGN(sizeof(struct dma_shared)))
    return -EINVAL;

  pfn = virt_to_phys((void *)dev->shared_mem) >> PAGE_SHIFT;

  if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
    return -EAGAIN;

  return 0;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  switch (cmd) {
  case 0: /* Get DMA count */
    return atomic_read(&dev->dma_count);
  case 1: /* Reset DMA count */
    atomic_set(&dev->dma_count, 0);
    return 0;
  case 2: /* Reset positions */
    dev->shared_mem->write_pos = 0;
    dev->shared_mem->read_pos = 0;
    return 0;
  default:
    return -ENOTTY;
  }
}

static int dev_open(struct inode *inode, struct file *file) {
  atomic_set(&dev->running, 1);
  dev->dma_thread = kthread_run(device_dma_thread, NULL, "dev_dma");
  if (IS_ERR(dev->dma_thread)) {
    atomic_set(&dev->running, 0);
    return PTR_ERR(dev->dma_thread);
  }
  return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
  atomic_set(&dev->running, 0);
  if (dev->dma_thread) {
    kthread_stop(dev->dma_thread);
    dev->dma_thread = NULL;
  }
  return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .mmap = dev_mmap,
    .unlocked_ioctl = dev_ioctl,
};

static int __init dma_dev_init(void) {
  int ret;
  unsigned long virt_addr;

  dev = kzalloc(sizeof(struct dma_device), GFP_KERNEL);
  if (!dev)
    return -ENOMEM;

  virt_addr =
      __get_free_pages(GFP_KERNEL, get_order(sizeof(struct dma_shared)));
  if (!virt_addr) {
    kfree(dev);
    return -ENOMEM;
  }

  dev->shared_mem = (struct dma_shared *)virt_addr;
  dev->shared_pages = virt_to_page(virt_addr);
  SetPageReserved(dev->shared_pages);

  dev->shared_mem->write_pos = 0;
  dev->shared_mem->read_pos = 0;
  atomic_set(&dev->running, 0);
  atomic_set(&dev->dma_count, 0);

  ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
  if (ret < 0) {
    ClearPageReserved(dev->shared_pages);
    free_pages(virt_addr, get_order(sizeof(struct dma_shared)));
    kfree(dev);
    return ret;
  }

  cdev_init(&dev_cdev, &fops);
  ret = cdev_add(&dev_cdev, dev_num, 1);
  if (ret < 0) {
    unregister_chrdev_region(dev_num, 1);
    ClearPageReserved(dev->shared_pages);
    free_pages(virt_addr, get_order(sizeof(struct dma_shared)));
    kfree(dev);
    return ret;
  }

  dev_class = class_create(DEVICE_NAME);
  if (IS_ERR(dev_class)) {
    cdev_del(&dev_cdev);
    unregister_chrdev_region(dev_num, 1);
    ClearPageReserved(dev->shared_pages);
    free_pages(virt_addr, get_order(sizeof(struct dma_shared)));
    kfree(dev);
    return PTR_ERR(dev_class);
  }

  if (IS_ERR(device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME))) {
    class_destroy(dev_class);
    cdev_del(&dev_cdev);
    unregister_chrdev_region(dev_num, 1);
    ClearPageReserved(dev->shared_pages);
    free_pages(virt_addr, get_order(sizeof(struct dma_shared)));
    kfree(dev);
    return -1;
  }

  pr_info("DMA device driver loaded\n");
  return 0;
}

static void __exit dma_dev_exit(void) {
  unsigned long virt_addr = (unsigned long)dev->shared_mem;

  atomic_set(&dev->running, 0);
  if (dev->dma_thread)
    kthread_stop(dev->dma_thread);

  device_destroy(dev_class, dev_num);
  class_destroy(dev_class);
  cdev_del(&dev_cdev);
  unregister_chrdev_region(dev_num, 1);
  ClearPageReserved(dev->shared_pages);
  free_pages(virt_addr, get_order(sizeof(struct dma_shared)));
  kfree(dev);
  pr_info("DMA device driver unloaded\n");
}

module_init(dma_dev_init);
module_exit(dma_dev_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DMA device simulator with polling");
MODULE_VERSION("1.0");
