#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <getopt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "def.h"
#include "helper.h"
#include "operator/operator.h"

int disk_fd = -1;

static const struct fuse_operations myfs_oper = {
    .getattr = myfs_getattr,
    .readdir = myfs_readdir,
    .mkdir = myfs_mkdir,
    .mknod = myfs_mknod,
    .write = myfs_write,
    .read = myfs_read,
};

void format_disk(const char *filename) {
  int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    perror("Open failed");
    exit(1);
  }

  Block empty = {0};
  for (int i = 0; i < MAX_BLOCKS; i++) {
    write(fd, &empty, sizeof(Block));
  }

  Block root = {0};
  root.id = 0;
  root.type = _DIRECTORY;
  strcpy(root.name, "/");
  pwrite(fd, &root, sizeof(Block), 0);

  printf("Disk formatted: %s (Size: %d bytes)\n", filename,
         MAX_BLOCKS * BLOCK_SIZE);
  close(fd);
}

int main(int argc, char *argv[]) {
  int opt;
  int is_format = 0;
  char *disk_file = NULL;

  // Getopt: -n <diskfile> for formatting
  while ((opt = getopt(argc, argv, "n:")) != -1) {
    switch (opt) {
      case 'n':
        is_format = 1;
        disk_file = optarg;
        break;
      default:
        fprintf(stderr, "Usage: %s [-n diskfile] [mountpoint]\n", argv[0]);
        return 1;
    }
  }

  if (is_format) {
    format_disk(disk_file);
    return 0;
  }

  if (optind + 1 >= argc) {
    fprintf(stderr, "Usage: %s <disk_image> <mount_point>\n", argv[0]);
    return 1;
  }

  disk_file = argv[optind];
  char *mount_point = argv[optind + 1];

  disk_fd = open(disk_file, O_RDWR);
  if (disk_fd < 0) {
    perror("Failed to open disk image");
    return 1;
  }

  char *fuse_argv[] = {argv[0], mount_point, "-f", NULL};  // -f: foreground
  int fuse_argc = 3;

  printf("Mounting %s to %s...\n", disk_file, mount_point);
  return fuse_main(fuse_argc, fuse_argv, &myfs_oper, NULL);
}
