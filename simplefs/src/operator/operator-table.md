# FUSE Operations Priority Reference

## Priority 1: Essential Operations (Must Implement)

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `getattr` | `const char *, struct stat *, struct fuse_file_info *` | Get file attributes (similar to stat) |
| `readdir` | `const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *, enum fuse_readdir_flags` | Read directory contents |
| `open` | `const char *, struct fuse_file_info *` | Open a file |
| `read` | `const char *, char *, size_t, off_t, struct fuse_file_info *` | Read data from an open file |
| `release` | `const char *, struct fuse_file_info *` | Release an open file (called on close) |

## Priority 2: Basic File Operations

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `write` | `const char *, const char *, size_t, off_t, struct fuse_file_info *` | Write data to an open file |
| `create` | `const char *, mode_t, struct fuse_file_info *` | Create and open a file |
| `unlink` | `const char *` | Remove a file |
| `mkdir` | `const char *, mode_t` | Create a directory |
| `rmdir` | `const char *` | Remove a directory |
| `truncate` | `const char *, off_t, struct fuse_file_info *` | Change the size of a file |
| `opendir` | `const char *, struct fuse_file_info *` | Open directory |
| `releasedir` | `const char *, struct fuse_file_info *` | Release directory |

## Priority 3: Advanced File Operations

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `rename` | `const char *, const char *, unsigned int` | Rename a file (supports RENAME_EXCHANGE, RENAME_NOREPLACE) |
| `chmod` | `const char *, mode_t, struct fuse_file_info *` | Change the permission bits of a file |
| `chown` | `const char *, uid_t, gid_t, struct fuse_file_info *` | Change the owner and group of a file |
| `utimens` | `const char *, const struct timespec[2], struct fuse_file_info *` | Change access/modification times with nanosecond resolution |
| `access` | `const char *, int` | Check file access permissions |
| `statfs` | `const char *, struct statvfs *` | Get file system statistics |

## Priority 4: Link Operations

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `readlink` | `const char *, char *, size_t` | Read the target of a symbolic link |
| `symlink` | `const char *, const char *` | Create a symbolic link |
| `link` | `const char *, const char *` | Create a hard link to a file |

## Priority 5: Data Integrity Operations

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `flush` | `const char *, struct fuse_file_info *` | Flush cached data (called on each close) |
| `fsync` | `const char *, int, struct fuse_file_info *` | Synchronize file contents |
| `fsyncdir` | `const char *, int, struct fuse_file_info *` | Synchronize directory contents |

## Priority 6: Extended Attributes

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `setxattr` | `const char *, const char *, const char *, size_t, int` | Set extended attributes |
| `getxattr` | `const char *, const char *, char *, size_t` | Get extended attributes |
| `listxattr` | `const char *, char *, size_t` | List extended attributes |
| `removexattr` | `const char *, const char *` | Remove extended attributes |

## Priority 7: Lifecycle Operations

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `init` | `struct fuse_conn_info *, struct fuse_config *` | Initialize filesystem |
| `destroy` | `void *` | Clean up filesystem (called on exit) |

## Priority 8: Advanced I/O Operations

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `write_buf` | `const char *, struct fuse_bufvec *, off_t, struct fuse_file_info *` | Write buffer contents to file (optimized) |
| `read_buf` | `const char *, struct fuse_bufvec **, size_t, off_t, struct fuse_file_info *` | Store file data in buffer (zero-copy) |
| `fallocate` | `const char *, int, off_t, off_t, struct fuse_file_info *` | Allocate space for an open file |
| `copy_file_range` | `const char *, struct fuse_file_info *, off_t, const char *, struct fuse_file_info *, off_t, size_t, int` | Copy data range between files (optimized) |

## Priority 9: Locking Operations

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `lock` | `const char *, struct fuse_file_info *, int, struct flock *` | Perform POSIX file locking operation |
| `flock` | `const char *, struct fuse_file_info *, int` | Perform BSD file locking operation |

## Priority 10: Special Operations

| Operation | Arguments | Description |
|-----------|-----------|-------------|
| `mknod` | `const char *, mode_t, dev_t` | Create a file node (non-directory, non-symlink) |
| `ioctl` | `const char *, unsigned int, void *, struct fuse_file_info *, unsigned int, void *` | Perform ioctl operations |
| `poll` | `const char *, struct fuse_file_info *, struct fuse_pollhandle *, unsigned *` | Poll for IO readiness events |
| `bmap` | `const char *, size_t, uint64_t *` | Map block index within file to device block |
| `lseek` | `const char *, off_t, int, struct fuse_file_info *` | Find next data or hole after offset |