#include <errno.h>
#include <fuse.h>
#include <string.h>

#include "../def.h"
#include "../helper.h"
#include "operator.h"

int myfs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
  BlockID head_id = resolve_path(path);
  if (head_id == -1) return -ENOENT;

  Block head_block;
  read_block(head_id, &head_block);

  if (offset >= head_block.size) return 0;
  if (offset + size > head_block.size) {
    size = head_block.size - offset;
  }

  BlockID curr_id = head_id;
  Block curr_block = head_block;

  int32_t block_idx = offset / MAX_FILE_DATA_SIZE;
  int32_t block_offset = offset % MAX_FILE_DATA_SIZE;

  for (int i = 0; i < block_idx; i++) {
    if (curr_block.next_block == 0) {
      return 0;
    }
    curr_id = curr_block.next_block;
    read_block(curr_id, &curr_block);
  }

  size_t total_read = 0;

  while (size > 0) {
    int32_t data_in_block = MAX_FILE_DATA_SIZE - block_offset;
    int32_t read_size = (size < data_in_block) ? size : data_in_block;

    Byte *data_ptr = get_data_ptr(&curr_block);
    memcpy(buf, data_ptr + block_offset, read_size);

    buf += read_size;
    size -= read_size;
    total_read += read_size;
    block_offset = 0;

    if (size > 0) {
      if (curr_block.next_block == 0) {
        break;
      }
      curr_id = curr_block.next_block;
      read_block(curr_id, &curr_block);
    }
  }

  return total_read;
}