#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "vmmc.h"

#define DEVICE_NAME "virtual_mmc_driver"
#define MAX_BLOCKS 2048
#define ONE_BLOCK_SIZE 512
#define VMMC_MEMORY (ONE_BLOCK_SIZE * MAX_BLOCKS)

static dev_t dev_num;
static int major;
static int minor = 0;
struct cdev *vmmc_cdev;
static struct mutex mutex;

static int open(struct inode *inode, struct file *filp) {
    filp->private_data = kzalloc(VMMC_MEMORY, GFP_KERNEL);
    if (!filp->private_data) {
        printk(KERN_ERR "vmmc: memory allocation for mmc card failed\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "vmmc: device opened\n");
    return 0;
}

static int release(struct inode *inode, struct file *filp) {
    if (filp->private_data) {
        kfree(filp->private_data);
        filp->private_data = NULL;
    }
    printk(KERN_INFO "vmmc: device closed\n");
    return 0;
}

static inline int validate_single_operation(truct vmmc_cmd cmd_data) {
    if (cmd_data.cur_block >= MAX_BLOCKS) {
        printk(KERN_ERR "vmmc: MMC card memory overflow\n");
        return -EINVAL;
    }
    if (cmd_data.block_num != 1) {
        printk(KERN_ERR "vmmc: wrong number of blocks\n");
        return -EINVAL;
    }
    return 0;
}

static inline int validate_multiple_operation(truct vmmc_cmd cmd_data) {
    if ((cmd_data.cur_block + cmd_data.block_num) >= MAX_BLOCKS) {
        printk(KERN_ERR "vmmc: MMC card memory overflow\n");
        ret = -EINVAL;
    }
    if (!cmd_data.block_num) {
        printk(KERN_ERR "vmmc: wrong number of blocks\n");
        ret = -EINVAL;
    }
    return 0;
}

static int read_single_block(struct vmmc_cmd cmd_data, char *tmp, char *vmmc_buffer) {
    int ret = 0;
    ret = validate_single_operation(struct vmmc_cmd cmd_data);
    if (ret) {
        goto out;
    }
    char *start_copy = vmmc_buffer + cmd_data.cur_block * ONE_BLOCK_SIZE;
    memcpy(tmp, start_copy, ONE_BLOCK_SIZE);
    ret = copy_to_user(cmd_data.data, tmp, ONE_BLOCK_SIZE);
    if (ret) {
        printk(KERN_ERR "vmmc: error copy data to user\n");
        ret = -EFAULT;
    }
out:
    return ret;
}

static int read_multiple_block(struct vmmc_cmd cmd_data, char *tmp, char *vmmc_buffer) {
    int ret = 0;
    ret = validate_multiple_operation(struct vmmc_cmd cmd_data);
    if (ret) {
        goto out;
    }
    for (int i = 0; i < cmd_data.block_num; i++) {
        char *start_copy = vmmc_buffer + (cmd_data.cur_block + i) * ONE_BLOCK_SIZE;
        memcpy(tmp, start_copy, ONE_BLOCK_SIZE);

        ret = copy_to_user(cmd_data.data + i * ONE_BLOCK_SIZE, tmp, ONE_BLOCK_SIZE);
        if (ret) {
            printk(KERN_ERR "vmmc: error copy data to user\n");
            ret = -EFAULT;
        }
    }
out:
    return ret;
}

static int write_single_block(struct vmmc_cmd cmd_data, char *tmp, char *vmmc_buffer) {
    int ret = 0;
    ret = validate_single_operation(struct vmmc_cmd cmd_data);
    if (ret) {
        goto out;
    }
    char *start_paste = vmmc_buffer + cmd_data.cur_block * ONE_BLOCK_SIZE;
    ret = copy_from_user(tmp, cmd_data.data, ONE_BLOCK_SIZE);
    if (ret) {
        printk(KERN_ERR "vmmc: error copy data from user\n");
        ret = -EFAULT;
        goto out;
    }
    memcpy(start_paste, tmp, ONE_BLOCK_SIZE);
out:
    return ret;
}

static int write_multiple_block(struct vmmc_cmd cmd_data, char *tmp, char *vmmc_buffer) {
    int ret = 0;
    ret = validate_multiple_operation(struct vmmc_cmd cmd_data);
    if (ret) {
        goto out;
    }
    for (int i = 0; i < cmd_data.block_num; i++) {
        char *start_paste = vmmc_buffer + (cmd_data.cur_block + i) * ONE_BLOCK_SIZE;
        ret = copy_from_user(tmp, cmd_data.data + i * ONE_BLOCK_SIZE, ONE_BLOCK_SIZE);
        if (ret) {
            printk(KERN_ERR "vmmc: error copy data from user\n");
            ret = -EFAULT;
            goto out;
        }
        memcpy(start_paste, tmp, ONE_BLOCK_SIZE);
    }
    goto out;
out:
    return ret;
}

static long vmmc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    mutex_lock(&mutex);
    int ret = 0;
    char *vmmc_buffer = filp->private_data;
    struct vmmc_cmd cmd_data;
    char *tmp = kmalloc(ONE_BLOCK_SIZE, GFP_KERNEL);
    if (!tmp) {
        printk(KERN_ERR "vmmc: memory allocation failed\n");
        return -ENOMEM;
    }
    ret = copy_from_user(&cmd_data, (struct vmmc_cmd __user *)arg, sizeof(cmd_data));
    if (ret) {
        printk(KERN_ERR "vmmc: error copy data from user\n");
        return -EFAULT;
    }

    switch(cmd) {

    case VMMC_READ_SINGLE_BLOCK:
        ret = read_single_block(cmd_data, tmp, vmmc_buffer);
        break;

    case VMMC_READ_MULTIPLE_BLOCK:
        ret = read_multiple_block(cmd_data, tmp, vmmc_buffer);
        break;

    case VMMC_WRITE_SINGLE_BLOCK:
        ret = write_single_block(cmd_data, tmp, vmmc_buffer);
        break;

    case VMMC_WRITE_MULTIPLE_BLOCK:
        ret = write_multiple_block(cmd_data, tmp, vmmc_buffer);
        break;

    default:
        ret = -ENOIOCTLCMD;
    }
    goto out;
out:
    kfree(tmp);
    mutex_unlock(&mutex);
    return ret;
}

static const struct file_operations vmmc_fops = {
    .owner = THIS_MODULE,
    .open = open,
    .release = release,
    .unlocked_ioctl = vmmc_ioctl
};

static int vmmc_init(void) {
    mutex_init(&mutex);
    int ret = 0;
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ALERT "vmmc: failed to allocate device number, rc=%d\n", ret);
        goto out;
    }
    major = MAJOR(dev_num);
    minor = MINOR(dev_num);
    printk(KERN_INFO "DEVICE --> %s,[ Major = %d ], [ minor  = %d ]\n", 
        DEVICE_NAME, major, minor);
    vmmc_cdev = cdev_alloc();
    if (!vmmc_cdev) {
        printk(KERN_ALERT "vmmc: failed to allocate cdev\n");
        ret = -ENOMEM;
        goto unregister_out;
    }
    vmmc_cdev->ops = &vmmc_fops;
    vmmc_cdev->owner = THIS_MODULE;
    ret = cdev_add(vmmc_cdev, dev_num, 1);
    if(ret) {
        printk(KERN_ALERT "vmmc: failed to add cdev to the kernel, rc=%d\n", ret);
        goto free_out;
    }
    goto out;
free_out:
    kfree(vmmc_cdev);
unregister_out:
    unregister_chrdev_region(dev_num, 1);
out:
    return ret;
}

static void vmmc_exit(void) {
    mutex_destroy(&mutex);
    cdev_del(vmmc_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "vmmc: unload module\n");
}

module_init(vmmc_init);
module_exit(vmmc_exit);

MODULE_LICENSE("GPL-2.0");
MODULE_DESCRIPTION("Virtual MMC driver with 1MB memory");
