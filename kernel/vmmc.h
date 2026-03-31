#ifndef VMMC_H
#define VMMC_H

#include <linux/ioctl.h>

struct vmmc_cmd {
    unsigned int cur_block;
    unsigned int block_num;
    void __user *data;
};

#define VMMC_MAGIC '#'
#define VMMC_READ_SINGLE_BLOCK    _IOR(VMMC_MAGIC, 1, struct vmmc_cmd)
#define VMMC_READ_MULTIPLE_BLOCK  _IOR(VMMC_MAGIC, 2, struct vmmc_cmd)
#define VMMC_WRITE_SINGLE_BLOCK   _IOW(VMMC_MAGIC, 3, struct vmmc_cmd)
#define VMMC_WRITE_MULTIPLE_BLOCK _IOW(VMMC_MAGIC, 4, struct vmmc_cmd)

#endif