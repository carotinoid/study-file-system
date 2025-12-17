#ifndef SIMPLEFS_OPERATOR_H
#define SIMPLEFS_OPERATOR_H

#include <fuse.h>
#include <sys/stat.h>

int myfs_mknod(const char *path, mode_t mode, dev_t rdev);
int myfs_getattr(const char *path, struct stat *stbuf,
                 struct fuse_file_info *fi);
int myfs_mkdir(const char *path, mode_t mode);
int myfs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi);
int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi,
                 enum fuse_readdir_flags flags);
int myfs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi);

#endif  // SIMPLEFS_OPERATOR_H