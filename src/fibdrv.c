#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "bigNum.h"
MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 100

static dev_t fib_dev = 0;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static int major = 0, minor = 0;

static long long fib_helper(long long n, long long *f)
{
    if (n <= 2)
        return n ? (f[n] = 1) : (f[n] = 0);
    else if (f[n] != 0)
        return f[n];

    long long a = fib_helper(n >> 1, f);
    long long b = fib_helper((n >> 1) + 1, f);

    return f[n] = (n & 1) ? a * a + b * b : (a * ((b << 1) - a));

}

static long long fib_sequence_rec(long long k)
{
    long long *f = kmalloc((k + 2) * sizeof(long long), GFP_KERNEL);
    memset(f, 0, (k + 2) * sizeof(long long));
    fib_helper(k, f);
    return f[k];
}
static long long fib_sequence(long long k)
{
    if (k <= 2)
        return !!k;
    long long a = 1; 
    long long b = 1;
    uint8_t count = 63 - __builtin_clzll(k);
    for (uint64_t i = count; i-- > 0;) {
        long long c = a * ((b << 1) - a);
        long long d = a * a + b * b;
        if (k & (1UL << i)) {
            a = d;
            b = c + d;
        }
        else {
            a = c;
            b = d;
        }
    }
    return a;
}

static void fib_fdoubling_bn(bn *dest, unsigned int n)
{
    bn_resize(dest, 1);
    if (n < 2) { //Fib(0) = 0, Fib(1) = 1
        dest->number[0] = n;
        return;
    }

    bn *f1 = dest; /* F(k) */
    bn *f2 = bn_alloc(1); /* F(k+1) */
    f1->number[0] = 0;
    f2->number[0] = 1;
    bn *k1 = bn_alloc(1);
    bn *k2 = bn_alloc(1);

    for (unsigned int i = 1U << 31; i; i >>= 1) {
        /* F(2k) = F(k) * [ 2 * F(k+1) – F(k) ] */
        bn_cpy(k1, f2);
        bn_lshift(k1, 1);
        bn_sub(k1, f1, k1);
        bn_mult(k1, f1, k1);
        /* F(2k+1) = F(k)^2 + F(k+1)^2 */
        bn_mult(f1, f1, f1);
        bn_mult(f2, f2, f2);
        bn_cpy(k2, f1);
        bn_add(k2, f2, k2);
        if (n & i) {
            bn_cpy(f1, k2);
            bn_cpy(f2, k1);
            bn_add(f2, k2, f2);
        } else {
            bn_cpy(f1, k1);
            bn_cpy(f2, k2);
        }
    }
    bn_free(f2);
    bn_free(k1);
    bn_free(k2);
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use\n");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

static bn *fib_time_proxy(long long k)
{
    // kt = ktime_get();
    bn *dest = bn_alloc(1);
    fib_fdoubling_bn(dest, k);
    // kt = ktime_sub(ktime_get(), kt);
    return dest;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    // return (ssize_t) fib_sequence_rec(*offset);
    // return (ssize_t) fib_sequence(*offset);
    bn *dest = fib_time_proxy(*offset);
    // fib_fdoubling_bn(dest, *offset);
    int len = dest->size;
    copy_to_user(buf, (char *)dest->number, 4 * len);
    return (ssize_t)len;

}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;
    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = major = register_chrdev(major, DEV_FIBONACCI_NAME, &fib_fops);
    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev\n");
        rc = -2;
        goto failed_cdev;
    }
    fib_dev = MKDEV(major, minor);

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class\n");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device\n");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
failed_cdev:
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
