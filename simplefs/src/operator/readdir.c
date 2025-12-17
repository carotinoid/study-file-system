#include <errno.h>
#include <fuse.h>
#include <string.h>

#include "../def.h"
#include "../helper.h"
#include "operator.h"

int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi,
                 enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  (void)flags;

  BlockID id = resolve_path(path);
  if (id == -1) return -ENOENT;

  Block dir;
  read_block(id, &dir);

  if (dir.type != _DIRECTORY) return -ENOTDIR;

  filler(buf, ".", NULL, 0, 0);
  filler(buf, "..", NULL, 0, 0);

  for (int i = 0; i < MAX_CHILDREN; i++) {
    BlockID child_id = dir.content.dir.children[i];
    if (child_id != 0) {
      Block child;
      read_block(child_id, &child);
      filler(buf, child.name, NULL, 0, 0);
    }
  }

  return 0;
}