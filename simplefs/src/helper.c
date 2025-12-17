#include "helper.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "def.h"

Byte *get_data_ptr(Block *block) {
  if (block->type == _FILE) return block->content.file.data;
  if (block->type == _DATA_BLOCK) return block->content.raw.raw_data;
  return NULL;
}

void read_block(BlockID id, Block *block) {
  if (pread(disk_fd, block, sizeof(Block), id * BLOCK_SIZE) != sizeof(Block)) {
    memset(block, 0, sizeof(Block));
  }
}

void write_block(BlockID id, Block *block) {
  pwrite(disk_fd, block, sizeof(Block), id * BLOCK_SIZE);
}

BlockID find_free_block() {
  Block block;
  for (BlockID i = 0; i < MAX_BLOCKS; i++) {
    read_block(i, &block);
    if (block.type == _FREE) {
      return i;
    }
  }
  return -1;  // No free block found
}

BlockID resolve_path(const char *path) {
  if (strcmp(path, "/") == 0) return 0;

  Block current_dir;
  read_block(0, &current_dir);

  if (strlen(path) > MAX_PATH_LEN - 1) return -1;
  char path_copy[MAX_PATH_LEN];

  strncpy(path_copy, path, MAX_PATH_LEN - 1);
  path_copy[MAX_PATH_LEN - 1] = '\0';

  char *token = strtok(path_copy, "/");

  while (token != NULL) {
    if (current_dir.type != _DIRECTORY) {
      return -1;
    }
    int found = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) {
      BlockID child_id = current_dir.content.dir.children[i];
      if (child_id != 0) {
        Block child;
        read_block(child_id, &child);
        if (strcmp(child.name, token) == 0) {
          current_dir = child;
          token = strtok(NULL, "/");
          found = 1;
          break;
        }
      }
    }
    if (!found) return -1;
  }
  return current_dir.id;
}

int create_node(const char *path, mode_t mode) {
  char parent_path[MAX_PATH_LEN];
  char filename[MAX_FILENAME_LEN];

  if (strlen(path) >= MAX_PATH_LEN) return -ENAMETOOLONG;

  char *last_slash = strrchr(path, '/');
  if (!last_slash) return -EINVAL;

  if (last_slash == path)
    strcpy(parent_path, "/");
  else {
    size_t len = last_slash - path;
    strncpy(parent_path, path, len);
    parent_path[len] = '\0';
  }

  strncpy(filename, last_slash + 1, MAX_FILENAME_LEN - 1);
  filename[MAX_FILENAME_LEN - 1] = '\0';

  BlockID parent_id = resolve_path(parent_path);
  if (parent_id == -1) return -ENOENT;

  Block parent;
  read_block(parent_id, &parent);

  BlockID new_id = find_free_block();
  if (new_id == -1) return -ENOSPC;

  int added = 0;
  for (int i = 0; i < MAX_CHILDREN; i++) {
    if (parent.content.dir.children[i] == 0) {
      parent.content.dir.children[i] = new_id;
      added = 1;
      break;
    }
  }
  if (!added) return -ENOSPC;
  write_block(parent_id, &parent);

  Block new_block = {0};
  new_block.id = new_id;
  strcpy(new_block.name, filename);
  new_block.type = (mode & S_IFDIR) ? _DIRECTORY : _FILE;
  write_block(new_id, &new_block);

  return 0;
}