/*
 * Interrupt-driven device: generates data, wakes userspace via interrupts
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "common.h"

#define DEVICE_NAME "dev_interrupt"

struct device_state {
  uint32_t buffer[BUFFER_SIZE];
  uint32_t read_pos;
  uint32_t write_pos;
  wait_queue_head_t wait_queue;
  struct task_struct *producer;
  atomic_t running;
};

static dev_t dev_num;
static struct class *dev_class;
static struct cdev dev_cdev;
static struct device_state *state;

static inline bool buffer_empty(void) {
  return state->read_pos == state->write_pos;
}

static inline bool buffer_full(void) {
  return next_pos(state->write_pos) == state->read_pos;
}

static int producer_thread(void *data) {
  uint32_t value = 0;
  while (!kthread_should_stop() && atomic_read(&state->running)) {
    if (!buffer_full()) {
      state->buffer[state->write_pos] = value++;
      state->write_pos = next_pos(state->write_pos);
      wake_up_interruptible(&state->wait_queue);
    }
    if (value % 1000 == 0)
      usleep_range(1, 10);
  }
  return 0;
}

static ssize_t dev_read(struct file *f, char __user *buf, size_t len,
                        loff_t *off) {
  size_t count = 0;
  uint32_t value;

  while (count < len && count < BUFFER_SIZE * sizeof(uint32_t)) {
    uint32_t local_read_pos = state->read_pos;
    uint32_t local_write_pos = state->write_pos;
    
    if (local_read_pos == local_write_pos) {
      if (f->f_flags & O_NONBLOCK)
        break;
      if (wait_event_interruptible(state->wait_queue, !buffer_empty()))
        return -ERESTARTSYS;
      local_read_pos = state->read_pos;
      local_write_pos = state->write_pos;
    }

    if (local_read_pos == local_write_pos)
      break;

    value = state->buffer[local_read_pos];
    state->read_pos = next_pos(local_read_pos);

    if (copy_to_user(buf + count, &value, sizeof(uint32_t)))
      return -EFAULT;
    count += sizeof(uint32_t);
  }
  return count;
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
    .read = dev_read,
};

static int __init dev_init(void) {
  state = kzalloc(sizeof(*state), GFP_KERNEL);
  if (!state)
    return -ENOMEM;

  init_waitqueue_head(&state->wait_queue);

  if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0)
    goto fail_alloc;

  cdev_init(&dev_cdev, &fops);
  if (cdev_add(&dev_cdev, dev_num, 1) < 0)
    goto fail_cdev;

  dev_class = class_create(DEVICE_NAME);
  if (IS_ERR(dev_class))
    goto fail_class;

  if (IS_ERR(device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME)))
    goto fail_device;

  pr_info("Interrupt device loaded\n");
  return 0;

fail_device:
  class_destroy(dev_class);
fail_class:
  cdev_del(&dev_cdev);
fail_cdev:
  unregister_chrdev_region(dev_num, 1);
fail_alloc:
  kfree(state);
  return -1;
}

static void __exit dev_exit(void) {
  if (state->producer)
    kthread_stop(state->producer);
  device_destroy(dev_class, dev_num);
  class_destroy(dev_class);
  cdev_del(&dev_cdev);
  unregister_chrdev_region(dev_num, 1);
  kfree(state);
}

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Interrupt-driven SPSC benchmark device");
