#include <errno.h>
#include <fuse.h>
#include <string.h>

#include "../def.h"
#include "../helper.h"
#include "operator.h"

int myfs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
  BlockID head_id = resolve_path(path);
  if (head_id == -1) return -ENOENT;

  Block head_block;
  read_block(head_id, &head_block);

  BlockID curr_id = head_id;
  Block curr_block = head_block;

  int32_t block_idx = offset / MAX_FILE_DATA_SIZE;
  int32_t block_offset = offset % MAX_FILE_DATA_SIZE;
  
  for (int i = 0; i < block_idx; i++) {
    if (curr_block.next_block == 0) {
      BlockID new_id = find_free_block();
      if (new_id == -1) return -ENOSPC;

      Block new_block = {0};
      new_block.id = new_id;
      new_block.type = _DATA_BLOCK;
      write_block(new_id, &new_block);

      curr_block.next_block = new_id;
      write_block(curr_id, &curr_block);
    }

    curr_id = curr_block.next_block;
    read_block(curr_id, &curr_block);
  }

  size_t total_written = 0;
  size_t original_request_size = size;

  while (size > 0) {
    int32_t space_in_block = MAX_FILE_DATA_SIZE - block_offset;
    int32_t write_size = (size < space_in_block) ? size : space_in_block;

    Byte *data_ptr = get_data_ptr(&curr_block);
    memcpy(data_ptr + block_offset, buf, write_size);
    write_block(curr_id, &curr_block);

    buf += write_size;
    size -= write_size;
    total_written += write_size;
    block_offset = 0;

    if (size > 0) {
      if (curr_block.next_block == 0) {
        BlockID new_id = find_free_block();
        if (new_id == -1) break;

        Block new_block = {0};
        new_block.id = new_id;
        new_block.type = _DATA_BLOCK;
        write_block(new_id, &new_block);

        curr_block.next_block = new_id;
        write_block(curr_id, &curr_block);
      }
      curr_id = curr_block.next_block;
      read_block(curr_id, &curr_block);
    }
  }

  read_block(head_id, &head_block);
  if (offset + total_written > head_block.size) {
    head_block.size = offset + total_written;
    write_block(head_id, &head_block);
  }

  return total_written;
}