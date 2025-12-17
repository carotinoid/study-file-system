#include <errno.h>
#include <fuse.h>
#include <string.h>

#include "../def.h"
#include "../helper.h"
#include "operator.h"

int myfs_mkdir(const char *path, mode_t mode) {
  return create_node(path, S_IFDIR | mode);
}