#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "virtual_mmc_driver"

static const struct file_operations mmc_fops = {
    .owner = THIS_MODULE,
    .open = vmmc_open,
    .release = vmmc_release,
    .unlocked_ioctl = vmmc_ioctl
};

static int ret;
static dev_t dev_n;
static int major;
static int minor = 0;
struct cdev *vmmc_cdev; 

static int init() {

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ALERT "Failed to allocate device number\n");
        return ret;
    }

    printk(KERN_INFO "DEVICE --> %s,[ Major = %d ], [ minor  = %d ]\n",DEVICE_NAME, major, minor);
    printk(KERN_INFO "\tUSE --> \"mknod /dev/%s c %d %d\" to create device file\n", DEVICE_NAME, major, minor);

    vmmc_cdev = cdev_alloc();
    vmmc_cdev->ops = &f_ops;
    vmmc_cdev->owner = THIS_MODULE;

    ret = cdev_add(vmmmc_cdev, dev_n, 1);
    if(ret){
        printk(KERN_ALERT "Failed to add cdev to the kernel\n");
        return ret;
    }

    return 0;
}

static void exit() {

    cdev_del(my_cdev);
    unregister_chrdev_region(dev_n, 1);
    printk(KERN_INFO "Unload module\n");
}

module_init(init);
module_exit(exit);
