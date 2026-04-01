// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "vmmc.h"

static dev_t dev_num;
static int major;
static int minor;
struct cdev *vmmc_cdev;
static struct mutex mutex;

static int open(struct inode *inode, struct file *filp)
{
	filp->private_data = kzalloc(VMMC_MEMORY, GFP_KERNEL);
	if (!filp->private_data) {
		pr_err("vmmc: memory allocation for mmc card failed\n");
		return -ENOMEM;
	}
	pr_info("vmmc: device opened\n");
	return 0;
}

static int release(struct inode *inode, struct file *filp)
{
	kfree(filp->private_data);
	filp->private_data = NULL;
	pr_info("vmmc: device closed\n");
	return 0;
}

static inline int validate_single_operation(struct vmmc_cmd *cmd_data)
{
	if (cmd_data->cur_block >= MAX_BLOCKS) {
		pr_err("vmmc: MMC card memory overflow\n");
		return -EINVAL;
	}
	if (cmd_data->block_num != 1) {
		pr_err("vmmc: wrong number of blocks\n");
		return -EINVAL;
	}
	return 0;
}

static inline int validate_multiple_operation(struct vmmc_cmd *cmd_data)
{
	if ((cmd_data->cur_block + cmd_data->block_num) >= MAX_BLOCKS) {
		pr_err("vmmc: MMC card memory overflow\n");
		return -EINVAL;
	}
	if (!cmd_data->block_num) {
		pr_err("vmmc: wrong number of blocks\n");
		return -EINVAL;
	}
	return 0;
}

static int read_blocks(struct vmmc_cmd *cmd_data, char *tmp, char *vmmc_buffer)
{
	unsigned int i;

	for (i = 0; i < cmd_data->block_num; i++) {
		char *start_copy = vmmc_buffer +
			(cmd_data->cur_block + i) * ONE_BLOCK_SIZE;

		memcpy(tmp, start_copy, ONE_BLOCK_SIZE);

		if (copy_to_user(cmd_data->data + i * ONE_BLOCK_SIZE,
				 tmp, ONE_BLOCK_SIZE)) {
			pr_err("vmmc: error copy data to user\n");
			return -EFAULT;
		}
	}
	return 0;
}

static int write_blocks(struct vmmc_cmd *cmd_data, char *tmp, char *vmmc_buffer)
{
	unsigned int i;

	for (i = 0; i < cmd_data->block_num; i++) {
		char *start_paste = vmmc_buffer +
			(cmd_data->cur_block + i) * ONE_BLOCK_SIZE;

		if (copy_from_user(tmp, cmd_data->data + i * ONE_BLOCK_SIZE,
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
	int ret = 0;
	char *vmmc_buffer = filp->private_data;
	struct vmmc_cmd cmd_data;
	char *tmp;

	mutex_lock(&mutex);

	tmp = kmalloc(ONE_BLOCK_SIZE, GFP_KERNEL);
	if (!tmp) {
		pr_err("vmmc: memory allocation failed\n");
		ret = -ENOMEM;
		goto unlock_out;
	}

	ret = copy_from_user(&cmd_data, (struct vmmc_cmd __user *)arg,
			     sizeof(cmd_data));
	if (ret) {
		pr_err("vmmc: error copy data from user\n");
		ret = -EFAULT;
		goto free_out;
	}

	switch (cmd) {
	case VMMC_READ_SINGLE_BLOCK:
		ret = validate_single_operation(&cmd_data);
		if (!ret)
			ret = read_blocks(&cmd_data, tmp, vmmc_buffer);
		break;

	case VMMC_READ_MULTIPLE_BLOCK:
		ret = validate_multiple_operation(&cmd_data);
		if (!ret)
			ret = read_blocks(&cmd_data, tmp, vmmc_buffer);
		break;

	case VMMC_WRITE_SINGLE_BLOCK:
		ret = validate_single_operation(&cmd_data);
		if (!ret)
			ret = write_blocks(&cmd_data, tmp, vmmc_buffer);
		break;

	case VMMC_WRITE_MULTIPLE_BLOCK:
		ret = validate_multiple_operation(&cmd_data);
		if (!ret)
			ret = write_blocks(&cmd_data, tmp, vmmc_buffer);
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
	int ret;

	mutex_init(&mutex);

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

	return 0;

free_out:
	kfree(vmmc_cdev);
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
	pr_info("vmmc: unload module\n");
}

module_init(vmmc_init);
module_exit(vmmc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtual MMC driver with 1MB memory");
