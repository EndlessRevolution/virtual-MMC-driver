/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/ioctl.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define VMMC_MEMORY    (ONE_BLOCK_SIZE * MAX_BLOCKS)
#define DEVICE_NAME    "virtual_mmc_driver"
#define MAX_BLOCKS     2048
#define ONE_BLOCK_SIZE 512

static struct device *vmmc_device;
static struct class *vmmc_class;
static struct cdev vmmc_cdev;
static dev_t dev_num;

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

static void validate_single_operation(struct mmc_ioc_cmd *wdata)
{
	__u32 status = 0;
	unsigned int cur_block = wdata->arg / ONE_BLOCK_SIZE;

	if (wdata->blocks != 1) {
		pr_err("vmmc: wrong number of blocks\n");
		status |= R1_BLOCK_LEN_ERROR;
	}

	if (wdata->arg % ONE_BLOCK_SIZE != 0) {
		pr_err("vmmc: offset must be 512 aligned\n");
		status |= R1_ADDRESS_ERROR;
	}

	if (cur_block >= MAX_BLOCKS) {
		pr_err("vmmc: mmc card memory overflow\n");
		status |= R1_OUT_OF_RANGE;
	}
	wdata->response[0] |= status;
}

static void validate_multiple_operation(struct mmc_ioc_cmd *wdata)
{
	__u32 status = 0;
	unsigned int cur_block = wdata->arg / ONE_BLOCK_SIZE;

	if (wdata->blocks <= 1) {
		pr_err("vmmc: wrong number of blocks\n");
		status |= R1_BLOCK_LEN_ERROR;
	}

	if (wdata->arg % ONE_BLOCK_SIZE != 0) {
		pr_err("vmmc: offset must be 512 aligned\n");
		status |= R1_ADDRESS_ERROR;
	}

	if (wdata->blocks > MAX_BLOCKS - cur_block) {
		pr_err("vmmc: mmc card memory overflow\n");
		status |= R1_OUT_OF_RANGE;
	}
	wdata->response[0] |= status;
}

static int read_blocks(struct mmc_ioc_cmd *wdata, char *vmmc_buffer)
{
	unsigned int cur_block = wdata->arg / ONE_BLOCK_SIZE;
	char __user *user_ptr = u64_to_user_ptr(wdata->data_ptr);

	for (unsigned int i = 0; i < wdata->blocks; i++) {
		char *start_copy =
			vmmc_buffer + (cur_block + i) * ONE_BLOCK_SIZE;

		if (copy_to_user(user_ptr + i * ONE_BLOCK_SIZE, start_copy,
				 ONE_BLOCK_SIZE)) {
			pr_err("vmmc: error copy buffer to user\n");
			return -EFAULT;
		}
	}
	return 0;
}

static int write_blocks(struct mmc_ioc_cmd *wdata, char *vmmc_buffer)
{
	unsigned int cur_block = wdata->arg / ONE_BLOCK_SIZE;
	char __user *user_ptr = u64_to_user_ptr(wdata->data_ptr);

	for (unsigned int i = 0; i < wdata->blocks; i++) {
		char *start_paste =
			vmmc_buffer + (cur_block + i) * ONE_BLOCK_SIZE;

		if (copy_from_user(start_paste, user_ptr + i * ONE_BLOCK_SIZE,
				   ONE_BLOCK_SIZE)) {
			pr_err("vmmc: error copy buffer from user\n");
			return -EFAULT;
		}
	}
	return 0;
}

static long vmmc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	char *vmmc_buffer = filp->private_data;
	struct mmc_ioc_cmd wdata;
	int ret = 0;

	if (cmd != MMC_IOC_CMD) {
		pr_err("vmmc: unknown ioctl command number\n");
		return -ENOTTY;
	}

	mutex_lock(&mutex);

	if (copy_from_user(&wdata, (struct mmc_ioc_cmd __user *)arg,
			   sizeof(wdata))) {
		pr_err("vmmc: error copy data from user\n");
		ret = -EFAULT;
		goto unlock_out;
	}

	memset(wdata.response, 0, sizeof(wdata.response));
	wdata.response[0] = R1_READY_FOR_DATA;

	switch (wdata.opcode) {
	case MMC_READ_SINGLE_BLOCK:
		validate_single_operation(&wdata);

		if (!R1_STATUS(wdata.response[0]))
			ret = read_blocks(&wdata, vmmc_buffer);

		break;

	case MMC_READ_MULTIPLE_BLOCK:
		validate_multiple_operation(&wdata);

		if (!R1_STATUS(wdata.response[0]))
			ret = read_blocks(&wdata, vmmc_buffer);

		break;

	case MMC_WRITE_BLOCK:
		validate_single_operation(&wdata);

		if (!R1_STATUS(wdata.response[0]))
			ret = write_blocks(&wdata, vmmc_buffer);

		break;

	case MMC_WRITE_MULTIPLE_BLOCK:
		validate_multiple_operation(&wdata);

		if (!R1_STATUS(wdata.response[0]))
			ret = write_blocks(&wdata, vmmc_buffer);

		break;

	default:
		pr_err("vmmc: unknown command\n");
		wdata.response[0] |= R1_ILLEGAL_COMMAND;

		break;
	}

	if (copy_to_user((struct mmc_ioc_cmd __user *)arg, &wdata,
			 sizeof(wdata))) {
		pr_err("vmmc: failed to copy response to user\n");
		ret = -EFAULT;
	}

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

	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (ret) {
		pr_err("vmmc: alloc_chrdev_region failed with code %d\n", ret);
		return ret;
	}

	pr_info("vmmc: registered device number major=%d minor=%d\n",
		MAJOR(dev_num), MINOR(dev_num));

	cdev_init(&vmmc_cdev, &vmmc_fops);

	ret = cdev_add(&vmmc_cdev, dev_num, 1);
	if (ret) {
		pr_err("vmmc: cdev_add failed with code %d\n", ret);
		goto unregister_region;
	}

	vmmc_class = class_create("vmmc");
	if (IS_ERR(vmmc_class)) {
		ret = PTR_ERR(vmmc_class);
		pr_err("vmmc: class_create failed with code %d\n", ret);
		goto del_cdev;
	}

	vmmc_device =
		device_create(vmmc_class, NULL, dev_num, NULL, DEVICE_NAME);

	if (IS_ERR(vmmc_device)) {
		ret = PTR_ERR(vmmc_device);
		pr_err("vmmc: device_create failed with code %d\n", ret);
		goto destroy_class;
	}

	vmmc_storage = kzalloc(VMMC_MEMORY, GFP_KERNEL);
	if (!vmmc_storage) {
		pr_err("vmmc: failed to allocate storage buffer\n");
		ret = -ENOMEM;
		goto destroy_device;
	}

	mutex_init(&mutex);
	pr_info("vmmc: module loaded\n");

	return 0;

destroy_device:
	device_destroy(vmmc_class, dev_num);

destroy_class:
	class_destroy(vmmc_class);

del_cdev:
	cdev_del(&vmmc_cdev);

unregister_region:
	unregister_chrdev_region(dev_num, 1);
	return ret;
}

static void vmmc_exit(void)
{
	mutex_destroy(&mutex);
	kfree(vmmc_storage);
	device_destroy(vmmc_class, dev_num);
	class_destroy(vmmc_class);
	cdev_del(&vmmc_cdev);
	unregister_chrdev_region(dev_num, 1);
	pr_info("module unloaded\n");
}

module_init(vmmc_init);
module_exit(vmmc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtual MMC driver with 1MB memory");
