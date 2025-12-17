#include <errno.h>
#include <fuse.h>
#include <string.h>

#include "../def.h"
#include "../helper.h"
#include "operator.h"

int myfs_mknod(const char *path, mode_t mode, dev_t rdev) {
  return create_node(path, S_IFREG | mode);
}