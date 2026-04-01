#ifndef VMMC_H
#define VMMC_H

#include <linux/ioctl.h>

struct vmmc_cmd {
    unsigned int cur_block;
    unsigned int block_num;
    void *data;
};

#define ONE_BLOCK_SIZE 512
#define MAX_BLOCKS 2048
#define VMMC_MEMORY (ONE_BLOCK_SIZE * MAX_BLOCKS)
#define DEVICE_NAME "virtual_mmc_driver"
#define VMMC_MAGIC '#'
#define VMMC_READ_SINGLE_BLOCK    _IOR(VMMC_MAGIC, 1, struct vmmc_cmd)
#define VMMC_READ_MULTIPLE_BLOCK  _IOR(VMMC_MAGIC, 2, struct vmmc_cmd)
#define VMMC_WRITE_SINGLE_BLOCK   _IOW(VMMC_MAGIC, 3, struct vmmc_cmd)
#define VMMC_WRITE_MULTIPLE_BLOCK _IOW(VMMC_MAGIC, 4, struct vmmc_cmd)

#endif