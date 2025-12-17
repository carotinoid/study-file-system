#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <getopt.h>
#include <stdlib.h>
#include <fuse3/fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#pragma push(pack, 1)[]

#define BLOCK_SIZE 512
#define MAX_BLOCKS 1024 
#define MAX_PATH_LEN 256
#define MAX_FILENAME_LEN 32
#define HEADER_SIZE (MAX_FILENAME_LEN + sizeof(enum Type) + sizeof(int32_t) + sizeof(BlockID))
#define MAX_CHILDREN ((BLOCK_SIZE - HEADER_SIZE) / sizeof(BlockID))
#define MAX_FILE_DATA_SIZE (BLOCK_SIZE - HEADER_SIZE)

typedef uint8_t Byte;
typedef int32_t BlockID;

enum Type {
    _FREE = 0, 
    _DIRECTORY = 1,
    _FILE = 2,
    _DATA_BLOCK = 99
};

typedef struct { // size: 512 bytes
    BlockID id;
    enum Type type;
    int32_t size;
    char name[MAX_FILENAME_LEN];
    union {
        struct { // _FILE
            Byte data[MAX_FILE_DATA_SIZE];
        } file;

        struct { // _DIRECTORY
            BlockID children[MAX_CHILDREN]; 
        } dir;
        
        struct { // _DATA_BLOCK
            Byte raw_data[MAX_FILE_DATA_SIZE]; 
        } raw;
    } content;
    BlockID next_block;
} Block;

#pragma pop(pack)

static int disk_fd = -1;

Byte* get_data_ptr(Block *block) {
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
    return -1; // No free block found
}

BlockID resolve_path(const char *path) {
    if (strcmp(path, "/") == 0) return 0;

    Block current_dir;
    read_block(0, &current_dir);

    if(strlen(path) > MAX_PATH_LEN - 1) return -1;
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

static int myfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
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

static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;

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

static int create_node(const char *path, mode_t mode) {
    char parent_path[MAX_PATH_LEN];
    char filename[MAX_FILENAME_LEN];
    
    if(strlen(path) >= MAX_PATH_LEN) return -ENAMETOOLONG;

    char *last_slash = strrchr(path, '/');
    if (!last_slash) return -EINVAL;

    if (last_slash == path) strcpy(parent_path, "/");
    else {
        size_t len = last_slash - path;
        strncpy(parent_path, path, len);
        parent_path[len] = '\0';
    }
    // 파일명 추출
    strncpy(filename, last_slash + 1, MAX_FILENAME_LEN - 1);
    filename[MAX_FILENAME_LEN - 1] = '\0';

    BlockID parent_id = resolve_path(parent_path);
    if (parent_id == -1) return -ENOENT;

    Block parent;
    read_block(parent_id, &parent);

    // 빈 공간 찾기
    BlockID new_id = find_free_block();
    if (new_id == -1) return -ENOSPC;

    // 부모의 자식 목록에 추가
    int added = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (parent.content.dir.children[i] == 0) {
            parent.content.dir.children[i] = new_id;
            added = 1;
            break;
        }
    }
    if (!added) return -ENOSPC; // 디렉토리 꽉 참
    write_block(parent_id, &parent); // 부모 업데이트

    // 새 블록 초기화
    Block new_block = {0};
    new_block.id = new_id;
    strcpy(new_block.name, filename);
    new_block.type = (mode & S_IFDIR) ? _DIRECTORY : _FILE;
    write_block(new_id, &new_block);

    return 0;
}

static int myfs_mkdir(const char *path, mode_t mode) {
    return create_node(path, S_IFDIR | mode);
}

static int myfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    return create_node(path, S_IFREG | mode);
}
static int myfs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
    BlockID head_id = resolve_path(path);
    if (head_id == -1) return -ENOENT;

    Block head_block;
    read_block(head_id, &head_block);

    // 1. 쓰기를 시작할 블록까지 이동 (offset 기반)
    BlockID curr_id = head_id;
    Block curr_block = head_block;

    int32_t block_idx = offset / MAX_FILE_DATA_SIZE;     // 몇 번째 블록인가
    int32_t block_offset = offset % MAX_FILE_DATA_SIZE;  // 블록 내 오프셋

    // 해당 블록 인덱스까지 링크 타고 이동 (없으면 생성)
    for (int i = 0; i < block_idx; i++) {
        if (curr_block.next_block == 0) {
            // 중간에 블록이 끊겨있으면 새로 할당 (Sparse file 지원 X, 순차 할당)
            BlockID new_id = find_free_block();
            if (new_id == -1) return -ENOSPC;

            Block new_block = {0};
            new_block.id = new_id;
            new_block.type = _DATA_BLOCK;
            write_block(new_id, &new_block);

            curr_block.next_block = new_id;
            write_block(curr_id, &curr_block); // 이전 블록 업데이트
        }
        
        curr_id = curr_block.next_block;
        read_block(curr_id, &curr_block);
    }

    // 2. 데이터 쓰기 루프
    size_t total_written = 0;
    size_t original_request_size = size;

    while (size > 0) {
        // 현재 블록에 쓸 수 있는 공간 계산
        int32_t space_in_block = MAX_FILE_DATA_SIZE - block_offset;
        int32_t write_size = (size < space_in_block) ? size : space_in_block;

        // 데이터 복사
        Byte *data_ptr = get_data_ptr(&curr_block);
        memcpy(data_ptr + block_offset, buf, write_size);
        write_block(curr_id, &curr_block);

        // 포인터 및 사이즈 갱신
        buf += write_size;
        size -= write_size;
        total_written += write_size;
        block_offset = 0; // 다음 블록부터는 0번지부터 씀

        // 더 쓸 데이터가 남았는데 블록이 끝나면 새 블록 할당
        if (size > 0) {
            if (curr_block.next_block == 0) {
                BlockID new_id = find_free_block();
                if (new_id == -1) break; // 공간 부족시 중단 (TODO: 에러처리)

                Block new_block = {0};
                new_block.id = new_id;
                new_block.type = _DATA_BLOCK;
                write_block(new_id, &new_block);

                curr_block.next_block = new_id;
                write_block(curr_id, &curr_block); // 링크 연결
            }
            // 다음 블록으로 이동
            curr_id = curr_block.next_block;
            read_block(curr_id, &curr_block);
        }
    }

    // 3. 파일 전체 크기 업데이트 (헤드 블록에만 저장됨)
    // 주의: 헤드 블록을 다시 읽어서 업데이트해야 함 (curr_block이 헤드가 아닐 수 있음)
    // 단, curr_id == head_id인 경우(첫 블록에만 쓴 경우)는 최적화 가능하나, 안전하게 다시 읽음
    
    read_block(head_id, &head_block);
    if (offset + total_written > head_block.size) {
        head_block.size = offset + total_written;
        write_block(head_id, &head_block);
    }

    return total_written;
}

static int myfs_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    BlockID head_id = resolve_path(path);
    if (head_id == -1) return -ENOENT;

    Block head_block;
    read_block(head_id, &head_block);

    // 1. 읽기 범위 체크
    if (offset >= head_block.size) return 0;
    if (offset + size > head_block.size) {
        size = head_block.size - offset;
    }

    // 2. 읽기를 시작할 블록까지 이동
    BlockID curr_id = head_id;
    Block curr_block = head_block;

    int32_t block_idx = offset / MAX_FILE_DATA_SIZE;
    int32_t block_offset = offset % MAX_FILE_DATA_SIZE;

    for (int i = 0; i < block_idx; i++) {
        if (curr_block.next_block == 0) {
            // 논리적으로 여기까지 와야 하는데 블록이 끊겨있음 (손상된 파일 등)
            return 0; 
        }
        curr_id = curr_block.next_block;
        read_block(curr_id, &curr_block);
    }

    // 3. 데이터 읽기 루프
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
                break; // 더 읽을 블록이 없음
            }
            curr_id = curr_block.next_block;
            read_block(curr_id, &curr_block);
        }
    }

    return total_read;
}

static const struct fuse_operations myfs_oper = {
    .getattr = myfs_getattr,
    .readdir = myfs_readdir,
    .mkdir   = myfs_mkdir,
    .mknod   = myfs_mknod,
    .write   = myfs_write,
    .read    = myfs_read,
};

void format_disk(const char *filename) {
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("Open failed"); exit(1); }

    Block empty = {0};
    for(int i=0; i<MAX_BLOCKS; i++) {
        write(fd, &empty, sizeof(Block));
    }

    Block root = {0};
    root.id = 0;
    root.type = _DIRECTORY;
    strcpy(root.name, "/");
    pwrite(fd, &root, sizeof(Block), 0);

    printf("Disk formatted: %s (Size: %d bytes)\n", filename, MAX_BLOCKS * BLOCK_SIZE);
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

    char *fuse_argv[] = { argv[0], mount_point, "-f", NULL }; // -f: foreground
    int fuse_argc = 3;

    printf("Mounting %s to %s...\n", disk_file, mount_point);
    return fuse_main(fuse_argc, fuse_argv, &myfs_oper, NULL);
}

// Compile with:
// gcc -o myfs custom.c -D_FILE_OFFSET_BITS=64 $(pkg-config fuse3 --cflags --libs)