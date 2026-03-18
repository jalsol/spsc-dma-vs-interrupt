/*
 * Interrupt-Driven Device Simulator
 * Simulates a device generating frequent data via interrupts
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define DEVICE_NAME "dev_interrupt"
#define BUFFER_SIZE 4096

struct interrupt_device {
  uint32_t *buffer;
  volatile uint32_t read_pos;
  volatile uint32_t write_pos;
  wait_queue_head_t read_queue;
  struct task_struct *generator_thread;
  atomic_t running;
  atomic_t irq_count;
  spinlock_t lock;
};

static dev_t dev_num;
static struct class *dev_class;
static struct cdev dev_cdev;
static struct interrupt_device *dev;

static inline uint32_t next_pos(uint32_t pos) {
  return (pos + 1) & (BUFFER_SIZE - 1);
}

static inline int buffer_empty(void) { return dev->read_pos == dev->write_pos; }

static inline int buffer_full(void) {
  return next_pos(dev->write_pos) == dev->read_pos;
}

/* Simulates hardware interrupt by waking up waiting readers */
static void simulate_interrupt(void) {
  atomic_inc(&dev->irq_count);
  wake_up_interruptible(&dev->read_queue);
}

/* Background thread simulating device generating data */
static int device_generator_thread(void *data) {
  uint32_t value = 0;

  while (!kthread_should_stop() && atomic_read(&dev->running)) {
    if (!buffer_full()) {
      dev->buffer[dev->write_pos] = value++;
      dev->write_pos = next_pos(dev->write_pos);

      /* Simulate interrupt for each new data item */
      simulate_interrupt();
    } else {
      /* Device would drop data or stall */
      usleep_range(1, 10);
    }

    /* Simulate device generating data continuously */
    if (value % 100 == 0)
      cond_resched();
  }

  return 0;
}

static ssize_t dev_read(struct file *filp, char __user *buf, size_t len,
                        loff_t *off) {
  size_t count = 0;
  uint32_t value;
  unsigned long flags;

  if (len % sizeof(uint32_t) != 0)
    return -EINVAL;

  while (count < len) {
    /* Wait for data (simulates waiting for interrupt) */
    if (buffer_empty()) {
      if (filp->f_flags & O_NONBLOCK)
        break;

      if (wait_event_interruptible(dev->read_queue, !buffer_empty()))
        return -ERESTARTSYS;
    }

    if (buffer_empty())
      break;

    spin_lock_irqsave(&dev->lock, flags);
    value = dev->buffer[dev->read_pos];
    dev->read_pos = next_pos(dev->read_pos);
    spin_unlock_irqrestore(&dev->lock, flags);

    if (copy_to_user(buf + count, &value, sizeof(uint32_t)))
      return -EFAULT;

    count += sizeof(uint32_t);
  }

  return count;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  switch (cmd) {
  case 0: /* Get IRQ count */
    return atomic_read(&dev->irq_count);
  case 1: /* Reset IRQ count */
    atomic_set(&dev->irq_count, 0);
    return 0;
  default:
    return -ENOTTY;
  }
}

static int dev_open(struct inode *inode, struct file *file) {
  atomic_set(&dev->running, 1);
  dev->generator_thread = kthread_run(device_generator_thread, NULL, "dev_gen");
  if (IS_ERR(dev->generator_thread)) {
    atomic_set(&dev->running, 0);
    return PTR_ERR(dev->generator_thread);
  }
  return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
  atomic_set(&dev->running, 0);
  if (dev->generator_thread) {
    kthread_stop(dev->generator_thread);
    dev->generator_thread = NULL;
  }
  return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .unlocked_ioctl = dev_ioctl,
};

static int __init interrupt_dev_init(void) {
  int ret;

  dev = kzalloc(sizeof(struct interrupt_device), GFP_KERNEL);
  if (!dev)
    return -ENOMEM;

  dev->buffer = kzalloc(BUFFER_SIZE * sizeof(uint32_t), GFP_KERNEL);
  if (!dev->buffer) {
    kfree(dev);
    return -ENOMEM;
  }

  init_waitqueue_head(&dev->read_queue);
  spin_lock_init(&dev->lock);
  atomic_set(&dev->running, 0);
  atomic_set(&dev->irq_count, 0);

  ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
  if (ret < 0) {
    kfree(dev->buffer);
    kfree(dev);
    return ret;
  }

  cdev_init(&dev_cdev, &fops);
  ret = cdev_add(&dev_cdev, dev_num, 1);
  if (ret < 0) {
    unregister_chrdev_region(dev_num, 1);
    kfree(dev->buffer);
    kfree(dev);
    return ret;
  }

  dev_class = class_create(DEVICE_NAME);
  if (IS_ERR(dev_class)) {
    cdev_del(&dev_cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(dev->buffer);
    kfree(dev);
    return PTR_ERR(dev_class);
  }

  if (IS_ERR(device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME))) {
    class_destroy(dev_class);
    cdev_del(&dev_cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(dev->buffer);
    kfree(dev);
    return -1;
  }

  pr_info("Interrupt device driver loaded\n");
  return 0;
}

static void __exit interrupt_dev_exit(void) {
  atomic_set(&dev->running, 0);
  if (dev->generator_thread)
    kthread_stop(dev->generator_thread);

  device_destroy(dev_class, dev_num);
  class_destroy(dev_class);
  cdev_del(&dev_cdev);
  unregister_chrdev_region(dev_num, 1);
  kfree(dev->buffer);
  kfree(dev);
  pr_info("Interrupt device driver unloaded\n");
}

module_init(interrupt_dev_init);
module_exit(interrupt_dev_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Interrupt-driven device simulator");
MODULE_VERSION("1.0");
