#ifndef SIMPLEFS_HELPER_H
#define SIMPLEFS_HELPER_H

#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "def.h"

Byte *get_data_ptr(Block *block);
void read_block(BlockID id, Block *block);
void write_block(BlockID id, Block *block);
BlockID find_free_block();
BlockID resolve_path(const char *path);
int create_node(const char *path, mode_t mode);

#endif  // SIMPLEFS_HELPER_H