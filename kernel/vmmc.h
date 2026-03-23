#ifndef VMMC_H
#define VMMC_H

#include <linux/ioctl.h>

struct vmmc_cmd {
    unsigned int cur_block;
    unsigned int block_num;
    void __user *data;
};

#define MMC_MAGIC '#'
#define MMC_READ_SINGLE_BLOCK    _IOR(MMC_MAGIC, 1, struct vmmc_cmd)
#define MMC_READ_MULTIPLE_BLOCK  _IOR(MMC_MAGIC, 2, struct vmmc_cmd)
#define MMC_WRITE_SINGLE_BLOCK   _IOW(MMC_MAGIC, 3, struct vmmc_cmd)
#define MMC_WRITE_MULTIPLE_BLOCK _IOW(MMC_MAGIC, 4, struct vmmc_cmd)

#endif