/*
The simple file system implementation in C++.
This code was written to study libfuse and mounting 
file systems in user space. The permission of directories 
is always set to 755, and files are set to 666 (read/write 
for all).

# Compile command
g++ -Wall simplefs.cpp `pkg-config fuse3 --cflags --libs` -o simplefs

# mount
mkdir mnt
./simplefs mnt

# test
ls -l mnt/ 
cat mnt/simplefs

# unmount
fusermount3 -u mnt
*/

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

struct INode {
    std::string name;
    std::string content; 
    bool is_dir;
    int permissions;
    std::map<std::string, INode*> children; 

    INode(std::string n, bool dir) : name(n), is_dir(dir) {
        if (dir) permissions = 0755 | S_IFDIR;
        else permissions = 0666 | S_IFREG;
    }
};

class SimpleFS {
private:
    INode* root;
    INode* resolvePath(const char* path) {
        if (strcmp(path, "/") == 0) return root;
    
        std::string pathStr(path);
        std::stringstream ss(pathStr);
        std::string token;
        INode* curr = root;
    
        while (std::getline(ss, token, '/')) {
            if (token.empty()) continue; 
    
            if (curr->children.find(token) == curr->children.end()) {
                return nullptr; // Not found
            }
            
            curr = curr->children[token];
        }
        return curr;
    }
    // /root/first/second => {INode* to /root/first, "second"}
    std::pair<INode*, std::string> getParentAndName(const char* path) {
        std::string pathStr(path);
        
        size_t lastSlash = pathStr.find_last_of('/');
        
        std::string parentPath = pathStr.substr(0, lastSlash);
        std::string name = pathStr.substr(lastSlash + 1);

        if (parentPath.empty()) parentPath = "/";

        INode* parentNode = resolvePath(parentPath.c_str());
        
        return {parentNode, name};
    }

public:
    SimpleFS() {
        root = new INode("/", true);
        root->children["."] = root;
        root->children[".."] = root;
        INode* hello = new INode("hello", false);
        hello->content = "Hello from Memory!";
        root->children["hello"] = hello;
    }


    int getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
        (void) fi;
        memset(stbuf, 0, sizeof(struct stat));

        INode* node = resolvePath(path);
        if (!node) return -ENOENT;

        stbuf->st_mode = node->permissions;
        stbuf->st_nlink = node->is_dir ? 2 : 1;
        stbuf->st_size = node->is_dir ? 4096 : node->content.size();

        return 0;
    }

    int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
        (void) offset; (void) fi; (void) flags;

        INode* node = resolvePath(path);
        if (!node || !node->is_dir) return -ENOENT;

        filler(buf, ".", NULL, 0, (fuse_fill_dir_flags)0);
        filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);

        for (auto const& [name, child] : node->children) {
            filler(buf, name.c_str(), NULL, 0, (fuse_fill_dir_flags)0);
        }
        return 0;
    }

    int read(const char *path, char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi) {
        (void) fi;
        
        INode* node = resolvePath(path);
        if (!node || node->is_dir) return -ENOENT;

        size_t len = node->content.size();
        if (offset < (long long)(len)) {
            if (offset + size > len) size = len - offset;
            memcpy(buf, node->content.c_str() + offset, size);
        } else {
            size = 0;
        }
        return size;
    }
    int write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
        (void) fi;

        INode* node = resolvePath(path);
        if (!node || node->is_dir) return -ENOENT;

        if (offset + size > node->content.size()) {
            node->content.resize(offset + size);
        }
        memcpy(&node->content[offset], buf, size);
        return size;
    }

    int truncate(const char *path, off_t size, struct fuse_file_info *fi) {
        (void) fi;
        
        INode* node = resolvePath(path);
        if (!node || node->is_dir) return -ENOENT;

        node->content.resize(size);
        return 0;
    }

    int mkdir(const char *path, mode_t mode) {
        auto [parentNode, name] = getParentAndName(path);
        if (!parentNode) return -ENOENT;

        if (parentNode->children.find(name) != parentNode->children.end()) {
            return -EEXIST; // Already exists
        }

        INode* newDir = new INode(name, true);
        parentNode->children[name] = newDir;
        newDir->children["."] = newDir;
        newDir->children[".."] = parentNode;
        return 0;
    }

    int unlink(const char *path) {
        auto [parentNode, name] = getParentAndName(path);
        if (!parentNode) return -ENOENT;

        if (parentNode->children.find(name) == parentNode->children.end()) {
            return -ENOENT; // Not found
        }

        INode* target = parentNode->children[name];
        if (target->is_dir) {
            return -EISDIR; // Is a directory
        }

        parentNode->children.erase(name);
        delete target;
        return 0;
    }
    int rmdir(const char *path) {
        auto [parentNode, name] = getParentAndName(path);
        if (!parentNode) return -ENOENT;

        if (parentNode->children.find(name) == parentNode->children.end()) {
            return -ENOENT; // Not found
        }

        INode* target = parentNode->children[name];
        if (!target->is_dir) {
            return -ENOTDIR; // Not a directory
        }

        if (target->children.size() > 2) { // . and ..
            return -ENOTEMPTY; // Directory not empty
        }

        parentNode->children.erase(name);
        delete target;
        return 0;
    }
    int create(const char *path, mode_t mode, struct fuse_file_info *fi) {
        auto [parentNode, name] = getParentAndName(path);
        if (!parentNode) return -ENOENT;

        if (parentNode->children.find(name) != parentNode->children.end()) {
            return -EEXIST;
        }

        INode* newFile = new INode(name, false);
        parentNode->children[name] = newFile;
        return 0;
    }

    int rename(const char *oldpath, const char *newpath, unsigned int flags) {
        auto [oldParent, oldName] = getParentAndName(oldpath);
        auto [newParent, newName] = getParentAndName(newpath);

        if (!oldParent || !newParent) return -ENOENT;

        if (oldParent->children.find(oldName) == oldParent->children.end()) {
            return -ENOENT; // Old path not found
        }

        if (flags & RENAME_NOREPLACE) {
            if (newParent->children.find(newName) != newParent->children.end()) {
                return -EEXIST; // New path exists
            }
        }

        INode* target = oldParent->children[oldName];
        oldParent->children.erase(oldName);
        newParent->children[newName] = target;
        target->name = newName;

        return 0;
        
    }

};

SimpleFS fs_instance;


static int wrap_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) { return fs_instance.getattr(path, stbuf, fi); }

static int wrap_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) { return fs_instance.readdir(path, buf, filler, offset, fi, flags); }

static int wrap_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) { return fs_instance.read(path, buf, size, offset, fi); }
static int wrap_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) { return fs_instance.write(path, buf, size, offset, fi); }
static int wrap_truncate(const char *path, off_t size, struct fuse_file_info *fi) { return fs_instance.truncate(path, size, fi); }
static int wrap_mkdir(const char *path, mode_t mode) { return fs_instance.mkdir(path, mode); }
static int wrap_unlink(const char *path) { return fs_instance.unlink(path); }
static int wrap_rmdir(const char *path) { return fs_instance.rmdir(path); }
static int wrap_create(const char *path, mode_t mode, struct fuse_file_info *fi) { return fs_instance.create(path, mode, fi); }
static int wrap_rename(const char *oldpath, const char *newpath, unsigned int flags) { return fs_instance.rename(oldpath, newpath, flags); }


static struct fuse_operations simplefs_oper = {
    .getattr = wrap_getattr,
    .mkdir = wrap_mkdir,
    .unlink = wrap_unlink,
    .rmdir = wrap_rmdir,
    .rename = wrap_rename,
    .truncate = wrap_truncate,
    .read = wrap_read,
    .write = wrap_write,
    .readdir = wrap_readdir,
    .create = wrap_create,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &simplefs_oper, NULL);
}