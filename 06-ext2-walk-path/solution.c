#include <solution.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>
#include <string.h>
#include <assert.h>

#define BLOCK_INIT 1024
#define PATH_SIZE 4096

uint BLOCK_SIZE = 0;

int get_inode_num_by_path(int img, int *inode_nr, struct ext2_super_block *s_block, 
                            char *path);
int handle_inode(int img, const int *inode_nr, const struct ext2_super_block *s_block,
                    struct ext2_inode *inode);
int get_inode(int img, int inode_nr, struct ext2_super_block s_block);

int handle_direct_block(int img, int type, const char* path, int *inode_nr, 
                        int i_block);
int handle_ind_block(int img, int i_block, int type, const char*path, int *inode_nr,
                        uint *buf);                        
int handle_indir_block(int img, int i_block, int type, char *path, int *inode_nr,
                        uint *buf);
int copy_file(int img, int out, int inode_nr);

void fill_path(char *dest, const char* from, int len) {
    memset(dest, '\0', PATH_SIZE);
    strncpy(dest, from, len);
}
int dump_file(int img, const char *path, int out) {
    // read superblock
    struct ext2_super_block s_block;
    int ret =  pread(img, &s_block, sizeof(s_block), BLOCK_INIT);
    assert(ret >= 0);
    if (ret < 0) {
        return -errno;
    }    
    BLOCK_SIZE = BLOCK_INIT << s_block.s_log_block_size;
    //-----------------------------------------------
    int path_len = strlen(path);
    char path_copy[path_len + 1];    
    // memset(path_copy, '\0', path_len + 1);
    // strncpy(path_copy, path, path_len);
    snprintf(path_copy, path_len + 1, "%s", path);
    path_copy[path_len] = '\0';
    //------------------------------------------
    int inode_nr = EXT2_ROOT_INO;
    ret = get_inode_num_by_path(img, &inode_nr, &s_block, path_copy);
    assert(ret >= 0);
    if (ret < 0) {
        return ret;
    }
    // struct ext2_inode inode;
    // ret = handle_inode(img, &inode_nr, &s_block, &inode);
    // assert(ret == 0);
    ret = copy_file(img, out, inode_nr);
    assert(ret == 0);
    return ret;
}
int copy_direct_blocks(int img, int out, uint i_block, uint block_size,
                         long *offset) {
    char buffer[block_size];
    int ret = pread(img, buffer, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    int to_write = (block_size < *offset) ? block_size : *offset;
    if (write(out, buffer, to_write) < to_write) {
        return -errno;
    }
    *offset -= to_write;
    return 0;
}
int copy_ind_block(int img, int out, uint i_block, uint block_size, 
                      long *offset) {
    char *buffer_indir = malloc(sizeof(char) * block_size);
    int ret = pread(img, buffer_indir, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    uint *buffer_int = (uint*) buffer_indir;
    for (uint i = 0; i < block_size / 4; ++i) {
        if (buffer_int[i] == 0) {
            break;
        }
        ret = copy_direct_blocks(img, out, buffer_int[i], block_size, offset);
        if (ret < 0) {
            return ret;
        }
        if (*offset < 0) break;
    }
    free(buffer_indir);
    return 0;
}

int copy_double_ind_block(int img, int out, uint i_block, uint block_size, 
                            long *offset) {
    char* double_ind_block_buffer = malloc(sizeof(char) * block_size);
    int ret = pread(img, double_ind_block_buffer, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    uint *buffer_int = (uint*) double_ind_block_buffer;
    for (uint i = 0; i < block_size / 4; ++ i) {
        ret = copy_ind_block(img, out, buffer_int[i], block_size, offset);
        if (ret < 0) return ret;
        if (*offset < 0) {
            break;
        }
    }
    free(double_ind_block_buffer);
    return 0;
}

int copy_file(int img, int out, int inode_nr) {
    // read superblock
    struct ext2_super_block s_block;
    int ret =  pread(img, &s_block, sizeof(s_block), BLOCK_INIT);
    if (ret < 0) {
        return -errno;
    }	
    
    int BLOCK_SIZE = BLOCK_INIT << s_block.s_log_block_size;
    // таблица дескрипторов
    struct ext2_group_desc g_desc;
    uint offset = BLOCK_SIZE * (s_block.s_first_data_block + 1) 
            + (inode_nr - 1) / s_block.s_inodes_per_group * sizeof(g_desc);
    ret = pread(img, &g_desc, sizeof(g_desc), offset);
    if (ret < 0) {
        return -errno;
    }
    // 
    struct ext2_inode inode;
    uint index = (inode_nr - 1) % s_block.s_inodes_per_group;
    uint pos = g_desc.bg_inode_table * BLOCK_SIZE + 
            (index * s_block.s_inode_size);
    if (pread(img, &inode, sizeof(inode), pos) < 0) {
        return -errno;
    }
    long size = inode.i_size;
    for (size_t i = 0; i < EXT2_N_BLOCKS; ++i) {
        if (i < EXT2_NDIR_BLOCKS) {
            ret = copy_direct_blocks(img, out, inode.i_block[i], BLOCK_SIZE, 
                                        &size);
            if (ret < 0) return ret;
        }
        if (i == EXT2_IND_BLOCK) {
            ret = copy_ind_block(img, out, inode.i_block[i], BLOCK_SIZE,
                                    &size);
            if (ret < 0) {
                return ret;
            }
        }
        if (i == EXT2_DIND_BLOCK) {
            ret = copy_double_ind_block(img, out, inode.i_block[i], BLOCK_SIZE,
                                    &size);
            if (ret < 0) {
                return ret;
            }  
        }
    }
    return 0;
}
// 
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
    struct ext2_inode inode_n;
    if (pread(img, inode, sizeof(inode_n), pos) < 0) {
        return -errno;
    }
    return 0;
}

int goto_next_dir(int img, int *inode_nr, 
                    struct ext2_super_block *s_block, const char* path) {
    char *next_path = strchr(path + 1, '/');
    // fprintf(stderr, "%s \n", next_path);
    return get_inode_num_by_path(img, inode_nr, s_block, next_path);
}
int get_inode_num_by_path(int img, int *inode_nr, struct ext2_super_block *s_block, 
                            char *path) {
    // printf("get_inode_num_by_path %s\n", path);
    int ret = -1;
    // ++path;
    struct ext2_inode inode;
    ret = handle_inode(img, inode_nr, s_block, &inode);
    assert(ret >= 0);
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
    // printf("dir: %s\n remain_path_len %d\n path : %s\n",dir, remain_path_len, path);
    int type = EXT2_FT_DIR;
    if (remain_path_len== 0) {
        type = EXT2_FT_UNKNOWN;
    }    
    //----------------------------
    //-------------------------------
    for (size_t i = 0; i < EXT2_N_BLOCKS; ++i) {
        int ret = 0;
        if (inode.i_block[i] == 0) {
            return -ENOENT;
        }
        if (i < EXT2_NDIR_BLOCKS) {
            ret = handle_direct_block(img, type, path, inode_nr, inode.i_block[i]);
        } else
        if (i == EXT2_IND_BLOCK) {
            uint dir_buf[BLOCK_SIZE];
            ret = handle_ind_block(img, type, inode.i_block[i], path, inode_nr, dir_buf);
            // free(dir_buf);
        assert(ret >= 0);  
        }else 
        if (i == EXT2_DIND_BLOCK) {
            uint dind_buf[BLOCK_SIZE];
            ret = handle_indir_block(img, type, inode.i_block[i], path, inode_nr, dind_buf);
            // free(dind_buf);
        assert(ret >= 0);  
        } else {
            assert(1);
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
int compare_dir_name(const char* dir1, const char* dir2, int len1, int len2) {
    if (len1 == len2)
        if (strncmp(dir1, dir2, len1) == 0) return 1;
    return 0;
}
int handle_direct_block(int img, int type, const char* path, int *inode_nr, 
                        int i_block) {
    struct ext2_dir_entry_2 dir_entry;
    off_t start = BLOCK_SIZE * i_block;
    char path_copy[PATH_SIZE];
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
        snprintf(path_copy, path_len + 1, "%s", path);
        path_copy[path_len] = '\0';
        char *next_dir = strtok(path_copy, "/");
        int next_dir_len = strlen(next_dir);
        // printf("entry name: %s, dir_name: %s\n", next_dir, name);
        if (compare_dir_name(dir_entry.name, next_dir, next_dir_len, dir_entry.name_len)==1)       {
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
int handle_ind_block(int img, int i_block, int type, const char*path, int *inode_nr,
                        uint *buf) {
    int ret = pread(img, buf, BLOCK_SIZE, BLOCK_SIZE * i_block);
    if (ret < 0) {
        return -errno;
    }
    for (uint i = 0; i < BLOCK_SIZE / sizeof(uint); ++i) {
        if (buf[i] == 0) {          
            assert(1);  
            return -ENOENT;
        }
        ret = handle_direct_block(img, type, path, inode_nr, buf[i]);
        assert(ret >= 0);  
        if (ret <= 0) {
            return ret;
        }
    }
    return ret;
}

int handle_indir_block(int img, int i_block, int type, char *path, int *inode_nr,
                        uint *buf) {
    uint *dir_buf = calloc(1, BLOCK_SIZE);
    int ret = pread(img, buf, BLOCK_SIZE, i_block * BLOCK_SIZE);
    if (ret < 0) {
        return -errno;
    }
    for (uint i = 0; i < BLOCK_SIZE / sizeof(uint); ++i) {
        ret = handle_ind_block(img, buf[i], type, path, inode_nr, dir_buf);
        if (ret <= 0) {
            free(dir_buf);
            return ret;
        }
    }
    free(dir_buf);
    return 0;
}