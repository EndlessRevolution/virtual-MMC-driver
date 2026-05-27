#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/mmc/ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define SEC_TO_NS                1000000000LL
#define REPEATS                  1000000
#define ONE_BLOCK_SIZE           512
#define DEVICE_PATH              "/dev/virtual_mmc_driver"

#define MMC_RSP_SPI_R1           (1 << 0)
#define MMC_RSP_R1               (1 << 1)
#define MMC_CMD_ADTC             (1 << 5)

#define R1_READY_FOR_DATA        (1 << 8)
#define R1_OUT_OF_RANGE          (1 << 31)
#define R1_ADDRESS_ERROR         (1 << 30)
#define R1_BLOCK_LEN_ERROR       (1 << 29)
#define R1_ILLEGAL_COMMAND       (1 << 22)
#define R1_STATUS(x)             ((x) & 0xFFF9A000)

#define MMC_READ_SINGLE_BLOCK    17
#define MMC_READ_MULTIPLE_BLOCK  18
#define MMC_WRITE_BLOCK          24
#define MMC_WRITE_MULTIPLE_BLOCK 25

enum { OPT_OP = 256, OPT_OFFSET, OPT_COUNT, OPT_INPUT, OPT_OUTPUT };

int time_measure(struct timespec *start, struct timespec *end,
                 struct mmc_ioc_cmd *wdata, int fd) {
    int ret = 0;
    clock_gettime(CLOCK_MONOTONIC_RAW, start);

    for (int i = 0; i < REPEATS; i++) {

        ret = ioctl(fd, MMC_IOC_CMD, wdata);
        if (ret < 0 || R1_STATUS(wdata->response[0]))
            break;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, end);
    return ret;
}

int write_full(int fd, char *data, size_t size) {
    size_t  write_bytes = 0;
    ssize_t ret;

    while (write_bytes < size) {
        ret = write(fd, data + write_bytes, size - write_bytes);
        if (ret < 0) {
            if (errno == EINTR)
                continue;

            return 1;
        }

        if (ret == 0)
            return 1;

        write_bytes += (size_t)ret;
    }
    return 0;
}

int read_full(int fd, char *data, size_t size) {
    size_t  read_bytes = 0;
    ssize_t ret;

    while (read_bytes < size) {
        ret = read(fd, data + read_bytes, size - read_bytes);
        if (ret < 0) {
            if (errno == EINTR)
                continue;

            return 1;
        }

        if (ret == 0)
            return 1;

        read_bytes += (size_t)ret;
    }
    return 0;
}

int parse_int(const char *str, unsigned long *out) {
    char *endptr;
    long  val;

    if (!str || *str == '\0')
        return -1;
    val = strtol(str, &endptr, 10);
    if (*endptr != '\0')
        return -1;
    if (val < 0)
        return -1;
    *out = (unsigned long)val;
    return 0;
}

int main(int argc, char *argv[]) {
    unsigned long long total_blocks = 0;
    unsigned long long total_ns = 0;
    struct timespec    start, end;
    int                ret = 0;
    int                rc = 0;

    struct mmc_ioc_cmd wdata;
    memset(&wdata, 0, sizeof(wdata));
    wdata.blksz = ONE_BLOCK_SIZE;
    wdata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

    char                *operation = NULL;
    char                *offset_str = NULL;
    char                *count_str = NULL;
    char                *input_file = NULL;
    char                *output_file = NULL;
    unsigned long        count;
    unsigned long        byte_no;
    unsigned long        opcode;

    static struct option long_opts[] = {
        {"op", required_argument, 0, OPT_OP},
        {"offset", required_argument, 0, OPT_OFFSET},
        {"count", required_argument, 0, OPT_COUNT},
        {"input", required_argument, 0, OPT_INPUT},
        {"output", required_argument, 0, OPT_OUTPUT},
        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
        case OPT_OP:
            operation = optarg;
            break;

        case OPT_OFFSET:
            offset_str = optarg;
            break;

        case OPT_COUNT:
            count_str = optarg;
            break;

        case OPT_INPUT:
            input_file = optarg;
            break;

        case OPT_OUTPUT:
            output_file = optarg;
            break;

        default:
            fprintf(stderr, "Unknown option or missing argument\n");
            return 1;
        }
    }

    if (!operation || !offset_str || !count_str) {
        fprintf(stderr, "Usage:\n"
                        "--op <operation_code>\n"
                        "--offset <offset_bytes>\n"
                        "--count <block_count>\n"
                        "[--input file]  (for write only)\n"
                        "[--output file] (for read only)\n");
        return 1;
    }

    if (parse_int(offset_str, &byte_no) < 0 ||
        parse_int(count_str, &count) < 0 || parse_int(operation, &opcode) < 0) {
        fprintf(stderr, "All arguments must be a non-negative number\n");
        return 1;
    }

    wdata.opcode = (__u32)opcode;
    wdata.arg = (__u32)byte_no;
    wdata.blocks = (unsigned int)count;

    switch (opcode) {
    case MMC_WRITE_BLOCK:
    case MMC_WRITE_MULTIPLE_BLOCK:
        wdata.write_flag = 1;
        break;

    default:
        wdata.write_flag = 0;
    }

    int dev_fd = open(DEVICE_PATH, O_RDWR);
    if (dev_fd < 0) {
        perror("open");
        return 1;
    }

    char *data = NULL;
    if (wdata.write_flag) {
        if (output_file) {
            fprintf(stderr, "--output not allowed for write operations\n");
            rc = 1;
            goto close_out;
        }
        if (!input_file) {
            fprintf(stderr, "Write requires --input file\n");
            rc = 1;
            goto close_out;
        }

        int input_fd = open(input_file, O_RDONLY);
        if (input_fd < 0) {
            perror("open");
            rc = 1;
            goto close_out;
        }

        data = malloc(count * ONE_BLOCK_SIZE);
        if (!data) {
            fprintf(stderr, "Memory allocation failed\n");
            close(input_fd);
            rc = 1;
            goto close_out;
        }

        if (read_full(input_fd, data, (size_t)(count * ONE_BLOCK_SIZE))) {
            fprintf(stderr, "File is not full or read failed\n");
            close(input_fd);
            rc = 1;
            goto free_out;
        }

        close(input_fd);
    } else {
        if (input_file) {
            fprintf(stderr, "--input not allowed for read operations\n");
            rc = 1;
            goto close_out;
        }
        if (!output_file) {
            fprintf(stderr, "Read requires --output file\n");
            rc = 1;
            goto close_out;
        }
        data = malloc(count * ONE_BLOCK_SIZE);
        if (!data) {
            fprintf(stderr, "Memory allocation failed\n");
            rc = 1;
            goto close_out;
        }
    }

    mmc_ioc_cmd_set_data(wdata, data);

    ret = time_measure(&start, &end, &wdata, dev_fd);

    if (ret < 0 || R1_STATUS(wdata.response[0])) {

        uint32_t status = wdata.response[0];

        if (status & R1_ADDRESS_ERROR)
            fprintf(stderr, "vmmc: Address error\n");

        if (status & R1_ILLEGAL_COMMAND)
            fprintf(stderr, "vmmc: Illegal command\n");

        if (status & R1_BLOCK_LEN_ERROR)
            fprintf(stderr, "vmmc: Block length error\n");

        if (status & R1_OUT_OF_RANGE)
            fprintf(stderr, "vmmc: Out of range\n");

        if (ret < 0)
            perror("ioctl");

        rc = 1;
        goto free_out;
    }

    total_ns =
        (end.tv_sec - start.tv_sec) * SEC_TO_NS + (end.tv_nsec - start.tv_nsec);
    total_blocks = REPEATS * count;

    if (!wdata.write_flag) {

        int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd < 0) {
            perror("open");
            rc = 1;
            goto free_out;
        }

        if (write_full(output_fd, data, (size_t)(count * ONE_BLOCK_SIZE))) {
            fprintf(stderr, "Write to output file failed\n");
            close(output_fd);
            rc = 1;
            goto free_out;
        }

        close(output_fd);
    }

    printf("Average time: %.5Lf ns\n", (long double)total_ns / REPEATS);
    printf("Average blocks per ns: %.5Lf\n",
           (long double)total_blocks / total_ns);

free_out:
    free(data);
close_out:
    close(dev_fd);
    return rc;
}
