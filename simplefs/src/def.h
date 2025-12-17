#ifndef SIMPLEFS_DEF_H
#define SIMPLEFS_DEF_H

#include <stdint.h>

#define BLOCK_SIZE 512
#define MAX_BLOCKS 1024
#define MAX_PATH_LEN 256
#define MAX_FILENAME_LEN 32
#define HEADER_SIZE \
  (MAX_FILENAME_LEN + sizeof(enum Type) + sizeof(int32_t) + sizeof(BlockID))
#define MAX_CHILDREN ((BLOCK_SIZE - HEADER_SIZE) / sizeof(BlockID))
#define MAX_FILE_DATA_SIZE (BLOCK_SIZE - HEADER_SIZE)

extern int disk_fd;

typedef uint8_t Byte;
typedef int32_t BlockID;

#pragma pack(push, 1)

enum Type { _FREE = 0, _DIRECTORY = 1, _FILE = 2, _DATA_BLOCK = 99 };

typedef struct {  // size: 512 bytes
  BlockID id;
  enum Type type;
  int32_t size;
  char name[MAX_FILENAME_LEN];
  union {
    struct {  // _FILE
      Byte data[MAX_FILE_DATA_SIZE];
    } file;

    struct {  // _DIRECTORY
      BlockID children[MAX_CHILDREN];
    } dir;

    struct {  // _DATA_BLOCK
      Byte raw_data[MAX_FILE_DATA_SIZE];
    } raw;
  } content;
  BlockID next_block;
} Block;

#pragma pack(pop)

#endif  // SIMPLEFS_DEF_H