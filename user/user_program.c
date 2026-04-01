#include "vmmc.h"
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SEC_TO_NS 1000000000LL
#define NS_TO_MS 1000000.0

enum { OPT_OP = 1, OPT_BLOCK, OPT_COUNT, OPT_INPUT, OPT_OUTPUT };

int parse_int(const char *str, unsigned int *out) {
  char *endptr;
  long val;

  if (!str || *str == '\0')
    return -1;

  val = strtol(str, &endptr, 10);

  if (*endptr != '\0')
    return -1;

  if (val < 0 || val > VMMC_MEMORY)
    return -1;

  *out = (int)val;
  return 0;
}

int main(int argc, char *argv[]) {
  struct vmmc_cmd cmd;
  struct timespec start, end;
  long long time_ns;
  int fd, ret;

  char *operation = NULL;
  char *input_file = NULL;
  char *output_file = NULL;

  cmd.cur_block = -1;
  cmd.block_num = -1;

  static struct option long_opts[] = {
      {"op", required_argument, 0, OPT_OP},
      {"block", required_argument, 0, OPT_BLOCK},
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

    case OPT_BLOCK:
      if (parse_int(optarg, &cmd.cur_block) < 0) {
        fprintf(stderr, "Invalid block\n");
        return 1;
      }
      break;

    case OPT_COUNT:
      if (parse_int(optarg, &cmd.block_num) < 0 || cmd.block_num <= 0) {
        fprintf(stderr, "Invalid count\n");
        return 1;
      }
      break;

    case OPT_INPUT:
      input_file = optarg;
      break;

    case OPT_OUTPUT:
      output_file = optarg;
      break;

    default:
      fprintf(stderr, "Invalid arguments\n");
      return 1;
    }
  }

  if (!operation || cmd.cur_block < 0 || cmd.block_num <= 0) {
    fprintf(stderr,
            "Usage:\n"
            "--op <read_single|read_multiple|write_single|write_multiple>\n"
            "--block <num>\n"
            "--count <num>\n"
            "[--input file]  (for write only)\n"
            "[--output file] (for read only)\n");
    return 1;
  }

  fd = open(DEVICE_NAME, O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "Failed to open device\n");
    return 1;
  }

  char *buffer = NULL;

  if (strstr(operation, "write")) {

    if (!input_file) {
      fprintf(stderr, "ERROR: write requires --input file\n");
      close(fd);
      return 1;
    }

    FILE *f = fopen(input_file, "rb");
    if (!f) {
      fprintf(stderr, "ERROR: cannot open input file\n");
      close(fd);
      return 1;
    }

    buffer = malloc(cmd.block_num * ONE_BLOCK_SIZE);
    if (!buffer) {
      fprintf(stderr, "Memory allocation failed\n");
      fclose(f);
      close(fd);
      return 1;
    }

    size_t r = fread(buffer, 1, cmd.block_num * ONE_BLOCK_SIZE, f);

    fclose(f);

    if (r == 0) {
      fprintf(stderr, "ERROR: file is empty or read failed\n");
      free(buffer);
      close(fd);
      return 1;
    }

    cmd.data = buffer;
  }

  if (strstr(operation, "read")) {

    if (!output_file) {
      fprintf(stderr, "ERROR: read requires --output file\n");
      close(fd);
      return 1;
    }

    buffer = malloc(cmd.block_num * ONE_BLOCK_SIZE);
    if (!buffer) {
      fprintf(stderr, "Memory allocation failed\n");
      close(fd);
      return 1;
    }

    cmd.data = buffer;
  }

  clock_gettime(CLOCK_MONOTONIC, &start);

  if (strcmp(operation, "read_single") == 0) {
    ret = ioctl(fd, VMMC_READ_SINGLE_BLOCK, &cmd);

  } else if (strcmp(operation, "read_multiple") == 0) {
    ret = ioctl(fd, VMMC_READ_MULTIPLE_BLOCK, &cmd);

  } else if (strcmp(operation, "write_single") == 0) {
    ret = ioctl(fd, VMMC_WRITE_SINGLE_BLOCK, &cmd);

  } else if (strcmp(operation, "write_multiple") == 0) {
    ret = ioctl(fd, VMMC_WRITE_MULTIPLE_BLOCK, &cmd);

  } else {
    fprintf(stderr, "Unknown operation\n");
    free(buffer);
    close(fd);
    return 1;
  }

  clock_gettime(CLOCK_MONOTONIC, &end);

  time_ns =
      (end.tv_sec - start.tv_sec) * SEC_TO_NS + (end.tv_nsec - start.tv_nsec);

  if (ret < 0) {
    fprintf(stderr, "Operation failed, code: %d\n", ret);
  } else {

    printf("Operation successful\n");
    printf("Time: %.8f ms\n", (double)time_ns / NS_TO_MS);

    if (strstr(operation, "read")) {

      FILE *f = fopen(output_file, "wb");
      if (!f) {
        fprintf(stderr, "ERROR: cannot open output file\n");
        free(buffer);
        close(fd);
        return 1;
      }

      fwrite(buffer, 1, cmd.block_num * ONE_BLOCK_SIZE, f);

      fclose(f);
    }
  }

  free(buffer);
  close(fd);
  return 0;
}
