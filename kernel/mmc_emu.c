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

static int ret = 0;
static dev_t dev_num;
static int major;
static int minor = 0;
struct cdev *vmmc_cdev;
static struct mutex ioctl_lock;

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

static long vmmc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

    mutex_lock(&ioctl_lock);
    char *vmmc_buffer = filp->private_data;
    char *tmp = kmalloc(ONE_BLOCK_SIZE, GFP_KERNEL);
    if (!tmp) goto err_mem_alloc;
    struct vmmc_cmd cmd_data;
    ret = copy_from_user(&cmd_data, (struct vmmc_cmd *)arg, sizeof(cmd_data));
    if (ret) goto err_copy_from;

    switch(cmd) {

    case MMC_READ_SINGLE_BLOCK: {

        if (cmd_data.cur_block >= MAX_BLOCKS) goto over_mem;
        if (cmd_data.block_num != 1) goto wrong_num;
        char *start_copy = vmmc_buffer + cmd_data.cur_block * ONE_BLOCK_SIZE;
        memcpy(tmp, start_copy, ONE_BLOCK_SIZE);
        ret = copy_to_user(cmd_data.data, tmp, ONE_BLOCK_SIZE);
        if (ret) goto err_copy_to;
        goto out;
    }
    case MMC_READ_MULTIPLE_BLOCK: {

        if ((cmd_data.cur_block +  cmd_data.block_num) >= MAX_BLOCKS) goto over_mem;
        if (!cmd_data.block_num) goto wrong_num;
        for (int i = 0; i < cmd_data.block_num; i++) {
            char *start_copy = vmmc_buffer + (cmd_data.cur_block + i) * ONE_BLOCK_SIZE;
            memcpy(tmp, start_copy, ONE_BLOCK_SIZE);
            ret = copy_to_user(cmd_data.data + i * ONE_BLOCK_SIZE, tmp, ONE_BLOCK_SIZE);
            if (ret) goto err_copy_to;
        }
        goto out;
    }
    case MMC_WRITE_SINGLE_BLOCK: {

        if (cmd_data.cur_block >= MAX_BLOCKS) goto over_mem;
        if (cmd_data.block_num != 1) goto wrong_num;
        char *start_paste = vmmc_buffer + cmd_data.cur_block * ONE_BLOCK_SIZE;
        ret = copy_from_user(tmp, cmd_data.data, ONE_BLOCK_SIZE);
        if (ret) goto err_copy_from;
        memcpy(start_paste, tmp, ONE_BLOCK_SIZE);
        goto out;
    }
    case MMC_WRITE_MULTIPLE_BLOCK: {

        if ((cmd_data.cur_block +  cmd_data.block_num) >= MAX_BLOCKS) goto over_mem;
        if (!cmd_data.block_num) goto wrong_num;
        for (int i = 0; i < cmd_data.block_num; i++) {
            char *start_paste = vmmc_buffer + (cmd_data.cur_block + i) * ONE_BLOCK_SIZE;
            ret = copy_from_user(tmp, cmd_data.data + i * ONE_BLOCK_SIZE, ONE_BLOCK_SIZE);
            if (ret) goto err_copy_from;
            memcpy(start_paste, tmp, ONE_BLOCK_SIZE);
        }
        goto out;
    }
    default:
        kfree(tmp);
        mutex_unlock(&ioctl_lock); 
        return -ENOIOCTLCMD;
    }

err_mem_alloc:
    printk(KERN_ERR "vmmc: memory allocation failed\n");
    mutex_unlock(&ioctl_lock);
    return -ENOMEM;
err_copy_from:
    printk(KERN_ERR "vmmc: error copy data from user\n");
    mutex_unlock(&ioctl_lock);
    kfree(tmp);
    return -EFAULT;
err_copy_to:
    printk(KERN_ERR "vmmc: error copy data to user\n");
    kfree(tmp);
    mutex_unlock(&ioctl_lock);
    return -EFAULT;
over_mem:
    printk(KERN_ERR "vmmc: MMC card memory overflow\n");
    kfree(tmp);
    mutex_unlock(&ioctl_lock);
    return -EINVAL;
wrong_num:
    printk(KERN_ERR "vmmc: wrong number of blocks\n");
    kfree(tmp);
    mutex_unlock(&ioctl_lock);
    return -EINVAL;
out:
    kfree(tmp);
    mutex_unlock(&ioctl_lock);
    return 0;
}

static const struct file_operations vmmc_fops = {
    .owner = THIS_MODULE,
    .open = open,
    .release = release,
    .unlocked_ioctl = vmmc_ioctl
};

static int vmmc_init(void) {

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ALERT "vmmc: failed to allocate device number\n");
        return ret;
    }
    major = MAJOR(dev_num);
    minor = MINOR(dev_num);

    printk(KERN_INFO "DEVICE --> %s,[ Major = %d ], [ minor  = %d ]\n", 
        DEVICE_NAME, major, minor);

    vmmc_cdev = cdev_alloc();
    if (!vmmc_cdev) {
        printk(KERN_ALERT "vmmc: failed to allocate cdev\n");
        unregister_chrdev_region(dev_num, 1);
        return -ENOMEM;
    }
    vmmc_cdev->ops = &vmmc_fops;
    vmmc_cdev->owner = THIS_MODULE;

    ret = cdev_add(vmmc_cdev, dev_num, 1);
    if(ret){
        printk(KERN_ALERT "vmmc: failed to add cdev to the kernel\n");
        return ret;
    }
    mutex_init(&ioctl_lock);

    return 0;
}

static void vmmc_exit(void) {

    cdev_del(vmmc_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "vmmc: unload module\n");
    mutex_destroy(&ioctl_lock); 
}

module_init(vmmc_init);
module_exit(vmmc_exit);

MODULE_LICENSE("GPL-2.0");
MODULE_DESCRIPTION("Virtual MMC driver with 1MB memory");
