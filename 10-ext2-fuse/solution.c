#include <solution.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>
#include <fuse.h>
#include <assert.h>

#define BLOCK_INIT 1024
#define PATH_SIZE 4096

uint BLOCK_SIZE = 0;
static int ext2_img = 0;
//========================================================
static int read_s_block(struct ext2_super_block *s_block);

int handle_direct_blocks(int img, uint i_block, uint block_size, fuse_fill_dir_t fill, char*data, long *size);
int handle_ind_block(int img, uint i_block, uint block_size, fuse_fill_dir_t fill, char*data, long *size);
int handle_double_ind_block(int img, uint i_block, uint block_size, fuse_fill_dir_t fill, char*data, long *size);

int dump_dir(int img, int inode_nr, char* data, struct ext2_super_block *s_block, fuse_fill_dir_t fill);
int get_inode_num_by_path(int img, int *inode_nr, struct ext2_super_block *s_block, const char *path);
static int fs_read(const char *path, char *buf, size_t size, off_t off, struct fuse_file_info *ffi);
static int fs_getattr(const char *path, struct stat *st, struct fuse_file_info *ffi);

static int fs_open(const char *path, struct fuse_file_info *ffi);
static int fs_readdir(const char *path, void *data, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *ffi,  enum fuse_readdir_flags frf);
int handle_inode(int img, const int *inode_nr, const struct ext2_super_block *s_block,struct ext2_inode *inode);
//========================================================

static int fs_write(const char *path, const char *buf, size_t size, off_t off, struct fuse_file_info * ffi)
{
    (void)buf;
	(void)size;
	(void)off;
	(void)ffi;
	(void)path;
    return -EROFS;
}
static int fs_create(const char *path, mode_t mode, struct fuse_file_info *ffi)
{
    printf("fs create path:%s\n", path);
	(void)path;
	(void)mode; 
	(void)ffi;
	return -EROFS;
}
static void* fs_init(struct fuse_conn_info* conn, struct fuse_config* config)
{
    printf("fs_init\n");
	(void)conn;
	(void)config;
	return NULL;
}

//----------ext2-read-file-----------------------------------------------------------
//-----------fs_read-----------------------------------------------------------------
#define BLOCK_INIT 1024

int read_direct_blocks(int img, int block_size, int i_block, off_t *written, 
                        off_t *size, off_t *off, char*out) {
    char buffer[block_size];
    int ret = pread(img, buffer, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    if (*off > 0) {
        if (*off < block_size) {
            size_t read = (*size < block_size - *off) ? *size : block_size - *off;
            memcpy(out + *written, buffer + *off, read);
            *off = 0;
            *size -= read;
            *written += read;
        } else {
            *off -= block_size;
        }
    } else {
        size_t read = (*size < block_size) ? *size : block_size;
        memcpy(out + *written, buffer, read);
        *size -= read;
        *written += read;
    }
    return 0;
}
int read_ind_block(int img, int block_size, int i_block, off_t *written, 
                        off_t *size, off_t *off, char*out) {
    char buffer[block_size];
    int ret = pread(img, buffer, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    int *buffer_int = (int*) buffer;
    for (long int i = 0; i < block_size / 4; ++i) {
        if (buffer_int[i] == 0) break;
        ret = read_direct_blocks(img, block_size, buffer_int[i], written,
                                    size, off, out);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}
int read_double_ind_block(int img, int block_size, int i_block, off_t *written, 
                        off_t *size, off_t *off, char*out)  {                            
    char buffer[block_size];
    int ret = pread(img, buffer, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    int *buffer_int = (int*) buffer;
    for (long int i = 0; i < block_size / 4; ++ i) {
        if (buffer_int[i] == 0) break;
        ret = read_ind_block(img, block_size, buffer_int[i], written, size, off, out);
        if (ret < 0) return ret;
    }
    return 0;
}
int dump_file(int img, char* out, int block_size, struct ext2_inode *inode, off_t offset, size_t size) {
    //copy
    off_t written = 0;
    off_t off = offset;
    off_t size_ = size;
    int ret = 0;
    for (size_t i = 0; i < EXT2_N_BLOCKS; ++i) {
        if (inode->i_block[i] == 0) break;
        if (i < EXT2_NDIR_BLOCKS) {
            ret = read_direct_blocks(img, block_size, inode->i_block[i], &written, &size_, 
                                    &off, out);
            if (ret < 0) return ret;
        }
        if (i == EXT2_IND_BLOCK) {
            ret = read_ind_block(img, block_size, inode->i_block[i], &written, &size_, 
                                    &off, out);
            if (ret < 0) {
                return ret;
            }
        }
        if (i == EXT2_DIND_BLOCK) {
            ret = read_double_ind_block(img, block_size, inode->i_block[i], &written, &size_, 
                                    &off, out);
            if (ret < 0) {
                return ret;
            }  
        }
    }  
    return size;
}
//-----------------------------------------------------------------------------------
static int fs_read(const char *path, char *buf, size_t size, off_t off, struct fuse_file_info *ffi) 
{
    printf("fs_read path:%s\n", path);
    (void) ffi;
	struct ext2_super_block super_block;
	int ret =  read_s_block(&super_block);
	if (ret < 0){
		return ret;
	}
    int inode_nr = EXT2_ROOT_INO;
	ret = get_inode_num_by_path(ext2_img, &inode_nr, &super_block, path);
	if (ret < 0)
		return ret;
    //readfile
    printf("readfile\n");
    struct ext2_inode inode={};
    ret = handle_inode(ext2_img, &inode_nr, &super_block, &inode);
    assert(ret >= 0);
    ret =  dump_file(ext2_img, buf, BLOCK_SIZE, &inode, off, size);
    return ret;
}

static int read_s_block(struct ext2_super_block *s_block) {
    int ret =  pread(ext2_img, s_block, sizeof(struct ext2_super_block), BLOCK_INIT);
    assert(ret >= 0);
    if (ret < 0) {
        return -errno;
    }    
    BLOCK_SIZE = BLOCK_INIT << s_block->s_log_block_size;
	return 0;
}
static int fs_open(const char *path, struct fuse_file_info *ffi)
{
    printf("fs open path:%s\n", path);	
	struct ext2_super_block super_block;
	int ret =  read_s_block(&super_block);
	if (ret < 0){
		return ret;
	}
    int inode_nr = EXT2_ROOT_INO;
	ret = get_inode_num_by_path(ext2_img, &inode_nr, &super_block, path);
	if (ret < 0)
		return ret;
    if ((ffi->flags & O_ACCMODE) != O_RDONLY) {
    	return -EROFS;
    }
	return 0;
}
static int fs_readdir(const char *path, void *data, fuse_fill_dir_t fill, off_t off, 
						struct fuse_file_info *ffi,  enum fuse_readdir_flags frf)
{
    printf("fs readdir path:%s\n", path);
    (void) off;
	(void) ffi;
	(void) frf;
	struct ext2_super_block s_block;
	int ret = read_s_block(&s_block);
    assert(ret >= 0);
	if (ret < 0){
		return ret;
	}
    int inode_nr = EXT2_ROOT_INO;
	ret = get_inode_num_by_path(ext2_img, &inode_nr, &s_block, path);
	if (ret < 0)
		return ret;
	return dump_dir(ext2_img, inode_nr, data, &s_block, fill);
}


int handle_inode(int img, const int *inode_nr, const struct ext2_super_block *s_block,
                    struct ext2_inode *inode) {	
    int ret = -1;
    struct ext2_group_desc g_desc;
    uint offset = BLOCK_SIZE * (s_block->s_first_data_block + 1) 
            + (*inode_nr - 1) / s_block->s_inodes_per_group * sizeof(g_desc);
    ret = pread(img, &g_desc, sizeof(g_desc), offset);
    if (ret < 0) {
        return -errno;
    }
    uint index = (*inode_nr - 1) % s_block->s_inodes_per_group;
    uint pos = g_desc.bg_inode_table * BLOCK_SIZE + 
            (index * s_block->s_inode_size);
    if (pread(img, inode, sizeof(struct ext2_inode), pos) < 0) {
        return -errno;
    }
    return 0;
}

int goto_next_dir(int img, int *inode_nr, 
                    struct ext2_super_block *s_block, const char* path) {
    char *next_path = strchr(path + 1, '/');
    return get_inode_num_by_path(img, inode_nr, s_block, next_path);
}
int compare_dir_name(const char* dir1, const char* dir2, int len1, int len2) {
    if (len1 == len2)
        if (strncmp(dir1, dir2, len1) == 0) return 1;
    return 0;
}
int direct_block(int img, int type, const char* path, int *inode_nr, int i_block) {
    struct ext2_dir_entry_2 dir_entry = {};
    off_t start = BLOCK_SIZE * i_block;
    off_t cur = start;
    int path_len = strlen(path);
    while (cur < start + BLOCK_SIZE) {
        int ret = pread(img, &dir_entry, sizeof(dir_entry), cur);
        if (ret < 0) {
            return -errno;
        }
        if(dir_entry.inode == 0){
            return -ENOENT;
        }
        int path_size = strlen(path);
        char path_copy[path_size + 1];
        snprintf(path_copy, path_len + 1, "%s", path);
        path_copy[path_len] = '\0';
        char *next_dir = strtok(path_copy, "/");
        int next_dir_len = strlen(next_dir);
        if (compare_dir_name(dir_entry.name, next_dir, dir_entry.name_len, next_dir_len) == 1) {
            if (dir_entry.file_type != type && type == EXT2_FT_DIR) {
                return -ENOTDIR;
            }    
            *inode_nr = dir_entry.inode;
            return 0;
        }
        cur += dir_entry.rec_len;
    }
    return 1;
}
int ind_block(int img, int i_block, int type, const char*path, int *inode_nr,
                        uint *buf) {
    int ret = pread(img, buf, BLOCK_SIZE, BLOCK_SIZE * i_block);
    if (ret < 0) {
        return -errno;
    }
    for (uint i = 0; i < BLOCK_SIZE / sizeof(uint); ++i) {
        if (buf[i] == 0) {   
            return -ENOENT;
        }
        ret = direct_block(img, type, path, inode_nr, buf[i]);
        if (ret <= 0) {
            return ret;
        }
    }
    return ret;
}

int indir_block(int img, int i_block, int type, const char *path, int *inode_nr, uint *buf) {
    uint *dir_buf = calloc(1, BLOCK_SIZE);
    int ret = pread(img, buf, BLOCK_SIZE, i_block * BLOCK_SIZE);
    if (ret < 0) {
        return -errno;
    }
    for (uint i = 0; i < BLOCK_SIZE / sizeof(uint); ++i) {
        ret = ind_block(img, type, buf[i], path, inode_nr, dir_buf);
        if (ret <= 0) {
            free(dir_buf);
            return ret;
        }
    }
    free(dir_buf);
    return 0;
}
int get_inode_num_by_path(int img, int *inode_nr, struct ext2_super_block *s_block, 
                            const char *path) {
    if (strcmp(path, "/") == 0) {
        *inode_nr = 2;
        return 0;
    }
    int ret = -1;
    struct ext2_inode inode;
    const int *inode_nr_p = inode_nr;
    ret = handle_inode(img, inode_nr_p, s_block, &inode);
    if (ret < 0) {
        return ret;
    }
    //---------------------------
    int path_len = strlen(path);
    char copy[path_len + 1];    
    snprintf(copy, path_len + 1, "%s", path);
    copy[path_len] = '\0';
    char *dir = strtok(copy, "/");
    int remain_path_len = path_len - strlen(dir) - 1;
    int type = EXT2_FT_DIR;
    if (remain_path_len== 0) {
        type = EXT2_FT_REG_FILE;
    }    
    //----------------------------
    //-------------------------------
    for (size_t i = 0; i < EXT2_N_BLOCKS; ++i) {
        int ret = 0;
        if (inode.i_block[i] == 0) {
            return -ENOENT;
        }
        if (i < EXT2_NDIR_BLOCKS) {
            ret = direct_block(img, type, path, inode_nr, inode.i_block[i]);
        } else if (i == EXT2_IND_BLOCK) {
            uint *dir_buf = calloc(1, BLOCK_SIZE);
            ret = ind_block(img, inode.i_block[i], type, path, inode_nr, dir_buf);
            free(dir_buf);
        } else if (i == EXT2_DIND_BLOCK) {
            uint *dind_buf = calloc(1, BLOCK_SIZE);
            ret = indir_block(img, inode.i_block[i], type, path, inode_nr, dind_buf);
            free(dind_buf);
        } else {
            return -ENOENT;
        } 
        if (ret < 0) return ret;
        if (ret == 0) {
            if (remain_path_len != 0) {
                return goto_next_dir(img, inode_nr, s_block, path);
            }
            break;
        }
    }
    return 0;
}

static int fs_getattr(const char *path, struct stat *st, struct fuse_file_info *ffi) 
{
    printf("getattr path = %s\n", path);
    (void) ffi;
    struct ext2_super_block s_block;
    int ret = read_s_block(&s_block);
    assert(ret >= 0);
    int inode_nr = EXT2_ROOT_INO;
    ret = get_inode_num_by_path(ext2_img, &inode_nr, &s_block, path);
    if (ret < 0) return ret;
    struct ext2_inode inode={};
    ret = handle_inode(ext2_img, &inode_nr, &s_block, &inode);
    assert(ret >= 0);
    if (ret < 0) return ret;

	st->st_ino = inode_nr;
	st->st_mode = inode.i_mode;
	st->st_nlink = inode.i_links_count;
	st->st_uid = inode.i_uid;
	st->st_gid = inode.i_gid;
	st->st_size = inode.i_size;
	st->st_blksize = BLOCK_SIZE;
	st->st_blocks = inode.i_blocks;
	st->st_atime = inode.i_atime;
	st->st_mtime = inode.i_mtime;
	st->st_ctime = inode.i_ctime;
    return 0;	
}
#define BLOCK_INIT 1024
//---------------ext2-read-dir-----------------------------------------------------------
int dump_dir(int img, int inode_nr, char* fill_buf, struct ext2_super_block *s_block, 
                fuse_fill_dir_t fill) {
    int BLOCK_SIZE = BLOCK_INIT << s_block->s_log_block_size;
    //
    // printf("dump_dir \n");
    struct ext2_group_desc g_desc;
    uint offset = BLOCK_SIZE * (s_block->s_first_data_block + 1) 
            + (inode_nr - 1) / s_block->s_inodes_per_group * sizeof(g_desc);
    int ret = pread(img, &g_desc, sizeof(g_desc), offset);
    if (ret < 0) {
        return -errno;
    }
    // 
    struct ext2_inode inode;
    uint index = (inode_nr - 1) % s_block->s_inodes_per_group;
    uint pos = g_desc.bg_inode_table * BLOCK_SIZE + (index * s_block->s_inode_size);
    if (pread(img, &inode, sizeof(inode), pos) < 0) {
        return -errno;
    }
    //copy
    long size = inode.i_size;
    for (size_t i = 0; i < EXT2_N_BLOCKS; ++i) {
        if (size <= 0) break;
        if (i < EXT2_NDIR_BLOCKS) {
            int ret = handle_direct_blocks(img, inode.i_block[i], BLOCK_SIZE, fill, fill_buf, &size);
            assert(ret >= 0);
            if (ret < 0) return ret;
        }
        if (i == EXT2_IND_BLOCK) {
            ret = handle_ind_block(img, inode.i_block[i], BLOCK_SIZE, fill, fill_buf, &size);
            assert(ret >=0);
            if (ret < 0) {
                return ret;
            }
        }
        if (i == EXT2_DIND_BLOCK) {
            ret = handle_double_ind_block(img, inode.i_block[i], BLOCK_SIZE, fill, fill_buf, &size);
            assert(ret >= 0);
            if (ret < 0) {
                return ret;
            }  
        }
    }  
    // printf("end dump dir\n");
    return 0;
}
void copy_name(const char* from, char* to, int len) {
    for (int i = 0; i < len; ++i) {
        to[i] = from[i];
    }
    to[len] = '\0';
}
int handle_direct_blocks(int img, uint i_block, uint block_size, fuse_fill_dir_t fill, char*data, long *size) {
    char *buffer = malloc(block_size * sizeof(char));
    struct ext2_dir_entry_2 *dir_entry;
    int ret = pread(img, buffer, block_size, i_block * block_size);
    assert(ret >= 0);
    if (ret < 0) return ret;
    long int cur_pos = 0;
    while (cur_pos < *size && cur_pos < block_size) {
        dir_entry = (struct ext2_dir_entry_2*)(buffer + cur_pos);
        char name[dir_entry->name_len + 1];
        copy_name(dir_entry->name, name, dir_entry->name_len);
        struct stat stat = {};
        stat.st_ino = dir_entry->inode;        
        if(dir_entry->file_type == EXT2_FT_REG_FILE)
            stat.st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
        else if(dir_entry->file_type == EXT2_FT_DIR)
            stat.st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;
        fill(data, name, &stat, 0, 0);
        cur_pos += dir_entry->rec_len;
    }
    if (cur_pos > *size) {
        *size = 0;
    } else {
        *size -= block_size;
    }
    return 0;
}
int handle_ind_block(int img, uint i_block, uint block_size, fuse_fill_dir_t fill, char*data, long *size) {
    char *buffer_indir = malloc(sizeof(char) * block_size);
    int ret = pread(img, buffer_indir, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    uint *buffer_int = (uint*) buffer_indir;
    for (uint i = 0; i < block_size / sizeof(uint); ++i) {
        ret = handle_direct_blocks(img, buffer_int[i], block_size, fill, data, size);
        assert(ret >= 0);
        if (ret < 0) {
            return ret;
        }
    }
    free(buffer_indir);
    return 0;
}
int handle_double_ind_block(int img, uint i_block, uint block_size, fuse_fill_dir_t fill, char*data, long *size) {
    char* double_ind_block_buffer = malloc(sizeof(char) * block_size);
    int ret = pread(img, double_ind_block_buffer, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    uint *buffer_int = (uint*) double_ind_block_buffer;
    for (uint i = 0; i < block_size / 4; ++ i) {
        ret = handle_ind_block(img, buffer_int[i], block_size, fill, data, size);
        if (ret < 0) return ret;
    }
    free(double_ind_block_buffer);
    return 0;
}

//--------------------------------------------------------------------------------
static const struct fuse_operations ext2_ops = {
    .readdir = fs_readdir,
    .read = fs_read,
    .open = fs_open,
    .getattr = fs_getattr,
    .write = fs_write,
    .create = fs_create,
    //
    .init = fs_init,
};

int ext2fuse(int img, const char *mntp)
{
	ext2_img = img;

	char *argv[] = {"exercise", "-f", (char *)mntp, NULL};
	return fuse_main(3, argv, &ext2_ops, NULL);
}
