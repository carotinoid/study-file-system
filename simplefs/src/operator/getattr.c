#include <errno.h>
#include <fuse.h>
#include <string.h>

#include "../def.h"
#include "../helper.h"
#include "operator.h"

int myfs_getattr(const char *path, struct stat *stbuf,
                 struct fuse_file_info *fi) {
  (void)fi;
  memset(stbuf, 0, sizeof(struct stat));

  BlockID id = resolve_path(path);
  if (id == -1) return -ENOENT;

  Block block;
  read_block(id, &block);

  if (block.type == _DIRECTORY) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (block.type == _FILE) {
    stbuf->st_mode = S_IFREG | 0644;
    stbuf->st_nlink = 1;
    stbuf->st_size = block.size;
  }

  return 0;
}