#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/mmc/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define SEC_TO_NS                1000000000LL
#define REPEATS                  1000
#define ONE_BLOCK_SIZE           512U
#define MAX_BLOCKS               2048U
#define MAX_BYTE_POS             1048064U
#define DEVICE_PATH              "/dev/virtual_mmc_driver"

#define MMC_RSP_SPI_R1           (1 << 0)
#define MMC_RSP_R1               (1 << 1)
#define MMC_CMD_ADTC             (1 << 5)

#define MMC_READ_SINGLE_BLOCK    17
#define MMC_READ_MULTIPLE_BLOCK  18
#define MMC_WRITE_BLOCK          24
#define MMC_WRITE_MULTIPLE_BLOCK 25

enum { OPT_OP = 1, OPT_BYTE, OPT_COUNT, OPT_INPUT, OPT_OUTPUT };

int parse_int(const char *str, unsigned int *out) {
    char *endptr;
    long  val;

    if (!str || *str == '\0')
        return -1;
    val = strtol(str, &endptr, 10);
    if (*endptr != '\0')
        return -1;
    if (val < 0)
        return -1;
    *out = (unsigned int)val;
    return 0;
}

int main(int argc, char *argv[]) {
    unsigned long long total_ns = 0;
    insigned int       total_blocks = 0;
    struct timespec    start, end;
    int                fd, ret;

    struct mmc_ioc_cmd wdata;
    memset(&wdata, 0, sizeof(wdata));
    wdata.blksz = ONE_BLOCK_SIZE;
    wdata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

    char                *operation = NULL;
    char                *byte_no_str = NULL;
    char                *count_str = NULL;
    char                *input_file = NULL;
    char                *output_file = NULL;
    unsigned int         count;
    unsigned int         byte_no;

    static struct option long_opts[] = {
        {"op", required_argument, 0, OPT_OP},
        {"byte", required_argument, 0, OPT_BYTE},
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

        case OPT_BYTE:
            byte_no_str = optarg;
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

    if (!operation || !byte_no_str || !count_str) {
        fprintf(stderr,
                "Usage:\n"
                "--op <read_single|read_multiple|write_single|write_multiple>\n"
                "--byte <num>\n"
                "--count <num>\n"
                "[--input file]  (for write only)\n"
                "[--output file] (for read only)\n");
        return 1;
    }

    if (strcmp(operation, "read_single") == 0) {
        wdata.opcode = MMC_READ_SINGLE_BLOCK;
        wdata.write_flag = 0;

    } else if (strcmp(operation, "read_multiple") == 0) {
        wdata.opcode = MMC_READ_MULTIPLE_BLOCK;
        wdata.write_flag = 0;

    } else if (strcmp(operation, "write_single") == 0) {
        wdata.opcode = MMC_WRITE_BLOCK;
        wdata.write_flag = 1;

    } else if (strcmp(operation, "write_multiple") == 0) {
        wdata.opcode = MMC_WRITE_MULTIPLE_BLOCK;
        wdata.write_flag = 1;

    } else {
        fprintf(stderr, "Unknown operation\n");
        return 1;
    }

    if (parse_int(byte_no_str, &byte_no) < 0 || byte_no > MAX_BYTE_POS) {
        fprintf(stderr, "--byte_no argument must be a non-negative number not "
                        "exceeding 1 048 064\n");
        return 1;
    }

    if (parse_int(count_str, &count) < 0 || count == 0 || count > MAX_BLOCKS) {
        fprintf(
            stderr,
            "--count argument must be a positive number not exceeding 2048\n");
        return 1;
    }

    if (byte_no % ONE_BLOCK_SIZE != 0) {
        fprintf(stderr, "--byte_no argument must be 512 aligned\n");
        return 1;
    }

    if (byte_no + count * ONE_BLOCK_SIZE > MAX_BYTE_POS + 1) {
        fprintf(stderr,
                "--count argument must be matched with --byte_no argument\n");
        return 1;
    }
    wdata.arg = (__u32)byte_no;
    wdata.blocks = count;

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "ERROR: failed to open device\n");
        return 1;
    }

    char *data = NULL;
    if (wdata.write_flag) {
        if (output_file) {
            fprintf(stderr, "--output not allowed for write operations\n");
            return 1;
        }
        if (!input_file) {
            fprintf(stderr, "Write requires --input file\n");
            return 1;
        }
        FILE *f = fopen(input_file, "rb");
        if (!f) {
            fprintf(stderr, "ERROR: cannot open input file\n");
            close(fd);
            return 1;
        }

        data = malloc(count * ONE_BLOCK_SIZE);
        if (!data) {
            fprintf(stderr, "ERROR: memory allocation failed\n");
            fclose(f);
            close(fd);
            return 1;
        }

        size_t r = fread(data, 1, count * ONE_BLOCK_SIZE, f);

        fclose(f);

        if (r != (size_t)count * ONE_BLOCK_SIZE) {
            fprintf(stderr, "ERROR: file is not full or read failed\n");
            free(data);
            close(fd);
            return 1;
        }
    } else {
        if (input_file) {
            fprintf(stderr, "--input not allowed for read operations\n");
            return 1;
        }
        if (!output_file) {
            fprintf(stderr, "Read requires --output file\n");
            return 1;
        }
        data = malloc(count * ONE_BLOCK_SIZE);
        if (!data) {
            fprintf(stderr, "ERROR: memory allocation failed\n");
            close(fd);
            return 1;
        }
    }

    mmc_ioc_cmd_set_data(wdata, data);

    for (int i = 0; i < REPEATS; i++) {

        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        ret = ioctl(fd, MMC_IOC_CMD, &wdata);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        if (ret < 0) {
            fprintf(stderr, "Operation failed with code %d\n", ret);
            free(data);
            close(fd);
            return 1;
        }
        total_ns += (end.tv_sec - start.tv_sec) * SEC_TO_NS +
                    (end.tv_nsec - start.tv_nsec);
    }
    total_blocks = REPEATS * count;

    if (!wdata.write_flag) {

        FILE *f = fopen(output_file, "wb");
        if (!f) {
            fprintf(stderr, "ERROR: cannot open output file\n");
            free(data);
            close(fd);
            return 1;
        }

        fwrite(data, 1, count * ONE_BLOCK_SIZE, f);
        fclose(f);
    }

    printf("Average time: %.5f ns\n", (double)total_ns / REPEATS);
    printf("Average blocks per ns: %.5f ns\n", (double)total_blocks / total_ns);

    free(data);
    close(fd);
    return 0;
}
