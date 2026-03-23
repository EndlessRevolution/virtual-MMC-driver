#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define DEVICE_NAME "virtual_mmc_driver"
#define MMC_MAGIC '#'
#define MAX_BLOCKS 2048
#define BLOCK_SIZE 512
#define VMMC_MEMORY (BLOCK_SIZE * MAX_BLOCKS)

#define MMC_READ_SINGLE_BLOCK    _IOR(MMC_MAGIC, 1, struct vmmc_cmd)
#define MMC_READ_MULTIPLE_BLOCK  _IOR(MMC_MAGIC, 2, struct vmmc_cmd)
#define MMC_WRITE_SINGLE_BLOCK   _IOW(MMC_MAGIC, 3, struct vmmc_cmd)
#define MMC_WRITE_MULTIPLE_BLOCK _IOW(MMC_MAGIC, 4, struct vmmc_cmd)

struct vmmc_cmd {
    unsigned int cur_block;
    unsigned int block_num;
    void __user *data;
};

static int ret = 0;
static dev_t dev_n;
static int major;
static int minor = 0;
struct cdev *vmmc_cdev;
static struct mutex ioctl_lock;

static int init() {

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ALERT "Failed to allocate device number\n");
        return ret;
    }

    printk(KERN_INFO "DEVICE --> %s,[ Major = %d ], [ minor  = %d ]\n",DEVICE_NAME, major, minor);

    vmmc_cdev = cdev_alloc();
    vmmc_cdev->ops = &f_ops;
    vmmc_cdev->owner = THIS_MODULE;

    ret = cdev_add(vmmmc_cdev, dev_n, 1);
    if(ret){
        printk(KERN_ALERT "Failed to add cdev to the kernel\n");
        return ret;
    }
    mutex_init(&ioctl_lock);

    return 0;
}

static void exit() {

    cdev_del(my_cdev);
    unregister_chrdev_region(dev_n, 1);
    printk(KERN_INFO "Unload module\n");
    mutex_destroy(&ioctl_lock); 
}

static int open(struct inode *inode, struct file *filp) {

    filp->private_data = kzalloc(VMMC_MEMORY, GFP_KERNEL);
    if (!filp->private_data) {
        printk(KERN_ERR "Memory allocation for vmmc failed\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "Vmmc device opened\n");
    return 0;
}

static int release(struct inode *inode, struct file *filp) {

    if (filp->private_data) {
        kfree(filp->private_data);
        filp->private_data = NULL;
    }
    printk(KERN_INFO "Vmmc device closed\n");
    return 0;
}

static int vmmc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

    char *tmp = kmalloc(BLOCK_SIZE, GFP_KERNEL);
    if (!tmp) {
        goto err_mem_alloc;
    }

    switch(cmd) {

    case MMC_READ_SINGLE_BLOCK:
        return read_single_block(arg, tmp, filp);

    case MMC_READ_MULTIPLE_BLOCK:
        return read_multiple_block(arg, tmp, filp);

    case MMC_WRITE_SINGLE_BLOCK:
        return write_single_block(arg, tmp, filp);

    case MMC_WRITE_MULTIPLE_BLOCK:
        return write_multiple_block(arg, tmp, filp);

    default:
        kfree(tmp);
        return -ENOIOCTLCMD;
    }
}


static int read_single_block(unsigned long arg, char *tmp, struct file *filp) {

    mutex_lock(&ioctl_lock);
    char *vmmc_buffer = filp->private_data;
    struct vmmc_cmd cmd_data;
    ret = copy_from_user(&cmd_data, (struct vmmc_cmd __user *)arg, sizeof(cmd_data));
    if (ret) goto err_copy_from;
    if (cmd_data.cur_block >= MAX_BLOCKS) goto over_mem;
    if (cmd_data.block_num != 1) goto wrong_num;
    char *start_copy = vmmc_buffer + cmd_data.cur_block * BLOCK_SIZE;
    memcpy(tmp, start_copy, BLOCK_SIZE);
    ret = copy_to_user(cmd_data.data, tmp, BLOCK_SIZE);
    if (ret) goto err_copy_to;
    goto out;
}

static int read_multiple_block(unsigned long arg, char *tmp, struct file *filp) {

    mutex_lock(&ioctl_lock);
    char *vmmc_buffer = filp->private_data;
    struct vmmc_cmd cmd_data;
    ret = copy_from_user(&cmd_data, (struct vmmc_cmd __user *)arg, sizeof(cmd_data));
    if (ret) goto err_copy_from;
    if ((cmd_data.cur_block +  cmd_data.block_num) >= MAX_BLOCKS) goto over_mem;
    if (!cmd_data.block_num) goto wrong_num;
    for (int i = 0; i < cmd_data.block_num; i++) {
        char *start_copy = vmmc_buffer + (cmd_data.cur_block + i) * BLOCK_SIZE;
        memcpy(tmp, start_copy, BLOCK_SIZE);
        ret = copy_to_user(cmd_data.data + i * BLOCK_SIZE, tmp, BLOCK_SIZE);
        if (ret) goto err_copy_to;
    }
    goto out;
}

static int write_single_block(unsigned long arg, char *tmp, struct file *filp) {

    mutex_lock(&ioctl_lock);
    char *vmmc_buffer = filp->private_data;
    struct vmmc_cmd cmd_data;
    ret = copy_from_user(&cmd_data, (struct vmmc_cmd __user *)arg, sizeof(cmd_data));
    if (ret) goto err_copy_from;
    if (cmd_data.cur_block >= MAX_BLOCKS) goto over_mem;
    if (cmd_data.block_num != 1) goto wrong_num;
    char *start_paste = vmmc_buffer + cmd_data.cur_block * BLOCK_SIZE;
    ret = copy_from_user(tmp, cmd_data.data, BLOCK_SIZE);
    if (ret) goto err_copy_from;
    memcpy(start_paste, tmp, BLOCK_SIZE);
    goto out;
}

static int write_multiple_block(unsigned long arg, char *tmp, struct file *filp) {

    mutex_lock(&ioctl_lock);
    char *vmmc_buffer = filp->private_data;
    struct vmmc_cmd cmd_data;
    ret = copy_from_user(&cmd_data, (struct vmmc_cmd __user *)arg, sizeof(cmd_data));
    if (ret) goto err_copy_from;
    if ((cmd_data.cur_block +  cmd_data.block_num) >= MAX_BLOCKS) goto over_mem;
    if (!cmd_data.block_num) goto wrong_num;
    for (int i = 0; i < cmd_data.block_num; i++) {
        char *start_paste = vmmc_buffer + (cmd_data.cur_block + i) * BLOCK_SIZE;
        ret = copy_from_user(tmp, cmd_data.data + i * BLOCK_SIZE, BLOCK_SIZE);
        if (ret) goto err_copy_from;
        memcpy(start_paste, tmp, BLOCK_SIZE);
    }
    goto out;
}

err_mem_alloc:
    printk(KERN_ERR "Memory allocation failed\n");
    return -ENOMEM;
err_copy_from:
    printk(KERN_ERR "Error copy data from user\n");
    mutex_unlock(&ioctl_lock);
    kfree(tmp);
    return -EFAULT;
err_copy_to:
    printk(KERN_ERR "Error copy data to user\n");
    kfree(tmp);
    mutex_unlock(&ioctl_lock);
    return -EFAULT;
over_mem:
    printk(KERN_ERR "MMC card memory overflow\n");
    kfree(tmp);
    mutex_unlock(&ioctl_lock);
    return -EINVAL;
wrong_num:
    printk(KERN_ERR "Wrong number of blocks\n");
    kfree(tmp);
    mutex_unlock(&ioctl_lock);
    return -EINVAL;
out:
    kfree(tmp);
    mutex_unlock(&ioctl_lock);
    return 0;


static const struct file_operations vmmc_fops = {
    .owner = THIS_MODULE,
    .open = open,
    .release = release,
    .unlocked_ioctl = vmmc_ioctl
};

MODULE_LICENSE("GPL-2.0");

module_init(init);
module_exit(exit);
