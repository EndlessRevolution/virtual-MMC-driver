// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/ioctl.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define ONE_BLOCK_SIZE 512
#define MAX_BLOCKS 2048
#define VMMC_MEMORY (ONE_BLOCK_SIZE * MAX_BLOCKS)
#define DEVICE_NAME "virtual_mmc_driver"

static dev_t dev_num;
static int major;
static int minor;
struct cdev *vmmc_cdev;
static struct mutex mutex;
static char *vmmc_storage;

static int open(struct inode *inode, struct file *filp)
{
	if (!vmmc_storage) {
		pr_err("vmmc: persistent memory not initialized\n");
		return -ENOMEM;
	}
	filp->private_data = vmmc_storage;
	pr_info("vmmc: device opened\n");
	return 0;
}

static int release(struct inode *inode, struct file *filp)
{
	pr_info("vmmc: device closed\n");
	return 0;
}

static inline int validate_single_operation(struct mmc_ioc_cmd *wdata)
{
	if (wdata->arg % ONE_BLOCK_SIZE != 0) {
		pr_err("vmmc: indention must be 512 aligned\n");
	} 
	if ((wdata->arg / ONE_BLOCK_SIZE) >= MAX_BLOCKS) {
		pr_err("vmmc: mmc card memory overflow\n");
		return -EINVAL;
	}
	if (wdata->blocks != 1) {
		pr_err("vmmc: wrong number of blocks\n");
		return -EINVAL;
	}
	return 0;
}

static inline int validate_multiple_operation(struct mmc_ioc_cmd *wdata)
{
	unsigned int cur_block = wdata->arg / ONE_BLOCK_SIZE;
	if (wdata->arg % ONE_BLOCK_SIZE != 0) {
		pr_err("vmmc: indention must be 512 aligned\n");
	}
	if (cur_block >= MAX_BLOCKS) {
		pr_err("vmmc: mmc card memory overflow\n");
		return -EINVAL;
	}
	if (!wdata->blocks) {
		pr_err("vmmc: wrong number of blocks\n");
		return -EINVAL;
	}
	return 0;
}

static int read_blocks(struct mmc_ioc_cmd *wdata, char *tmp, char *vmmc_buffer)
{
	unsigned int cur_block = wdata->arg / ONE_BLOCK_SIZE;
	char __user *user_ptr = u64_to_user_ptr(wdata->data_ptr);
	for (unsigned int i = 0; i < wdata->blocks; i++) {
		char *start_copy = vmmc_buffer +
			(cur_block + i) * ONE_BLOCK_SIZE;

		memcpy(tmp, start_copy, ONE_BLOCK_SIZE);

		if (copy_to_user(user_ptr + i * ONE_BLOCK_SIZE,
				 tmp, ONE_BLOCK_SIZE)) {
			pr_err("vmmc: error copy data to user\n");
			return -EFAULT;
		}
	}
	return 0;
}

static int write_blocks(struct mmc_ioc_cmd *wdata, char *tmp, char *vmmc_buffer)
{
	unsigned int cur_block = wdata->arg / ONE_BLOCK_SIZE;
	char __user *user_ptr = u64_to_user_ptr(wdata->data_ptr);
	for (unsigned int i = 0; i < wdata->blocks; i++) {
		char *start_paste = vmmc_buffer +
			(cur_block + i) * ONE_BLOCK_SIZE;

		if (copy_from_user(tmp, user_ptr + i * ONE_BLOCK_SIZE,
				   ONE_BLOCK_SIZE)) {
			pr_err("vmmc: error copy data from user\n");
			return -EFAULT;
		}
		memcpy(start_paste, tmp, ONE_BLOCK_SIZE);
	}
	return 0;
}

static long vmmc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	if (cmd != MMC_IOC_CMD)
	{
		return -ENOIOCTLCMD;
	}

	char *vmmc_buffer = filp->private_data;
	struct mmc_ioc_cmd wdata;
	char *tmp = NULL;
	int ret = 0;

	mutex_lock(&mutex);

	tmp = kmalloc(ONE_BLOCK_SIZE, GFP_KERNEL);
	if (!tmp) {
		pr_err("vmmc: memory allocation failed\n");
		ret = -ENOMEM;
		goto unlock_out;
	}

	ret = copy_from_user(&wdata, (struct mmc_ioc_cmd __user *)arg, sizeof(wdata));
	if (ret) {
		pr_err("vmmc: error copy data from user\n");
		ret = -EFAULT;
		goto free_out;
	}

	switch (wdata.opcode) {
	case MMC_READ_SINGLE_BLOCK:
		ret = validate_single_operation(&wdata);
		if (!ret)
			ret = read_blocks(&wdata, tmp, vmmc_buffer);
		break;

	case MMC_READ_MULTIPLE_BLOCK:
		ret = validate_multiple_operation(&wdata);
		if (!ret)
			ret = read_blocks(&wdata, tmp, vmmc_buffer);
		break;

	case MMC_WRITE_BLOCK:
		ret = validate_single_operation(&wdata);
		if (!ret)
			ret = write_blocks(&wdata, tmp, vmmc_buffer);
		break;

	case MMC_WRITE_MULTIPLE_BLOCK:
		ret = validate_multiple_operation(&wdata);
		if (!ret)
			ret = write_blocks(&wdata, tmp, vmmc_buffer);
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

free_out:
	kfree(tmp);
unlock_out:
	mutex_unlock(&mutex);
	return ret;
}

static const struct file_operations vmmc_fops = {
	.owner = THIS_MODULE,
	.open = open,
	.release = release,
	.unlocked_ioctl = vmmc_ioctl,
};

static int vmmc_init(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		pr_alert("vmmc: failed to allocate device number\n");
		goto out;
	}

	major = MAJOR(dev_num);
	minor = MINOR(dev_num);
	pr_info("DEVICE --> %s,[ Major = %d ], [ minor  = %d ]\n",
		DEVICE_NAME, major, minor);

	vmmc_cdev = cdev_alloc();
	if (!vmmc_cdev) {
		pr_alert("vmmc: failed to allocate cdev\n");
		ret = -ENOMEM;
		goto unregister_out;
	}

	vmmc_cdev->ops = &vmmc_fops;
	vmmc_cdev->owner = THIS_MODULE;

	ret = cdev_add(vmmc_cdev, dev_num, 1);
	if (ret) {
		pr_alert("vmmc: failed to add cdev to the kernel\n");
		goto free_out;
	}

	vmmc_storage = kzalloc(VMMC_MEMORY, GFP_KERNEL);
	if (!vmmc_storage) {
		pr_alert("vmmc: failed to allocate memory for mmc\n");
		ret = -ENOMEM;
		goto free_out;
	}

	mutex_init(&mutex);

	return 0;

free_out:
	cdev_del(vmmc_cdev);
unregister_out:
	unregister_chrdev_region(dev_num, 1);
out:
	return ret;
}

static void vmmc_exit(void)
{
	mutex_destroy(&mutex);
	cdev_del(vmmc_cdev);
	unregister_chrdev_region(dev_num, 1);
	kfree(vmmc_storage);
	pr_info("vmmc: unload module\n");
}

module_init(vmmc_init);
module_exit(vmmc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtual MMC driver with 1MB memory");
