#include <solution.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>
#include <string.h>

#define BLOCK_INIT 1024
#define PATH_SIZE 4096

int BLOCK_SIZE = 0;

int get_inode_num_by_path(int img, int *inode_nr, struct ext2_super_block *s_block, 
                            char *path);
int handle_inode(int img, const int *inode_nr, const struct ext2_super_block *s_block,
                    struct ext2_inode *inode);
int get_inode(int img, int inode_nr, struct ext2_super_block s_block);

int handle_direct_block(int img, int type, const char* path, int *inode_nr, 
                        int i_block);
int handle_ind_block(int img, int i_block, int type, char*path, int *inode_nr,
                        int *buf);                        
int handle_indir_block(int img, int i_block, int type, char *path, int *inode_nr,
                        int *buf);
int copy_file(int img, int out, struct ext2_inode *inode);

void fill_path(char *dest, const char* from, int len) {
    memset(dest, '\0', PATH_SIZE);
    strncpy(dest, from, len);
}
int dump_file(int img, const char *path, int out) {
    // read superblock
    struct ext2_super_block s_block;
    int ret =  pread(img, &s_block, sizeof(s_block), BLOCK_INIT);
    if (ret < 0) {
        return -errno;
    }    
    BLOCK_SIZE = BLOCK_INIT << s_block.s_log_block_size;
    //-----------------------------------------------
    int path_len = strlen(path);
    char path_copy[PATH_SIZE];
    fill_path(path_copy, path, path_len);
    //------------------------------------------
    int inode_nr = EXT2_ROOT_INO;
    ret = get_inode_num_by_path(img, &inode_nr, &s_block, path_copy);
    if (ret < 0) {
        return ret;
    }
    struct ext2_inode inode;
    handle_inode(img, &inode_nr, &s_block, &inode);
    return copy_file(img, out, &inode);
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
int copy_file(int img, int out, struct ext2_inode *inode) {

    long size = inode->i_size;
    int ret;
    for (size_t i = 0; i < EXT2_N_BLOCKS; ++i) {
        if (i < EXT2_NDIR_BLOCKS) {
            ret = copy_direct_blocks(img, out, inode->i_block[i], BLOCK_SIZE, 
                                        &size);
            if (ret < 0) return ret;
        }
        if (i == EXT2_IND_BLOCK) {
            ret = copy_ind_block(img, out, inode->i_block[i], BLOCK_SIZE,
                                    &size);
            if (ret < 0) {
                return ret;
            }
        }
        if (i == EXT2_DIND_BLOCK) {
            ret = copy_double_ind_block(img, out, inode->i_block[i], BLOCK_SIZE,
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
    return get_inode_num_by_path(img, inode_nr, s_block, next_path);
}
int get_inode_num_by_path(int img, int *inode_nr, struct ext2_super_block *s_block, 
                            char *path) {
    // printf("get_inode_num_by_path %s\n", path);
    int ret = -1;
    // ++path;
    struct ext2_inode inode;
    ret = handle_inode(img, inode_nr, s_block, &inode);
    if (ret < 0) {
        return ret;
    }
    //---------------------------
    char next_dir[PATH_SIZE];
    strcpy(next_dir, path);
    strtok(next_dir, "/");
    int next_dir_len = strlen(next_dir);
    int remain_path_len = strlen(path) - next_dir_len;
    int type = EXT2_FT_DIR;
    if (remain_path_len== 0) {
        type = EXT2_FT_REG_FILE;
    }    
    //----------------------------
    //-------------------------------
    for (size_t i = 0; i < EXT2_N_BLOCKS; ++i) {
        if (inode.i_block[i] == 0) {
            // printf("get_inode_num_by_path(i_block[%ld]=0) ", i);
            return -ENOENT;
        }
        if (i < EXT2_NDIR_BLOCKS) {
            int ret = handle_direct_block(img, type, path, inode_nr, inode.i_block[i]);
            if (ret < 0) return ret;
            if (ret == 0 && remain_path_len != 0) {
                return goto_next_dir(img, inode_nr, s_block, path);
            } else if (ret == 0) {
                break;
            }
            continue;
        }
        if (i == EXT2_IND_BLOCK) {
            int dir_buf[BLOCK_SIZE];
            ret = handle_ind_block(img, type, inode.i_block[i], path, inode_nr, dir_buf);
            if (ret < 0) {
                return ret;
            }
            if (ret == 0 && remain_path_len != 0) {                
                return goto_next_dir(img, inode_nr, s_block, path);
            } else if (ret == 0) {
                break;
            }
            continue;
        }
        if (i == EXT2_DIND_BLOCK) {
            int dind_buf[BLOCK_SIZE];
            ret = handle_indir_block(img, type, inode.i_block[i], path, inode_nr, dind_buf);
            if (ret < 0) {
                return ret;
            }
            if (ret == 0 && remain_path_len != 0) {
                return goto_next_dir(img, inode_nr, s_block, path);
            } else if (ret == 0) {
                break;
            }
            continue;
        }
        return -ENOENT;
    }
    return ret;
}
int compare_dir_name(const char* dir1, const char* dir2, int len1, int len2) {
    if ((len1 == len2) && !strncmp(dir1, dir2, len1)) return 1;
    return 0;
}
int handle_direct_block(int img, int type, const char* path, int *inode_nr, 
                        int i_block) {
    struct ext2_dir_entry_2 dir_entry;
    int start = BLOCK_SIZE * i_block;
    int cur = start;
    while (cur - start < BLOCK_SIZE) {
        int ret = pread(img, &dir_entry, sizeof(dir_entry), cur);
        if (ret < 0) {
            return -errno;
        }
        int path_len = strlen(path);
        char path_copy[PATH_SIZE];
        fill_path(path_copy, path, path_len);
        char name[PATH_SIZE];
        fill_path(name, dir_entry.name, dir_entry.name_len);
        char *next_dir = strtok(path_copy, "/");
        int next_dir_len = strlen(next_dir);
        // printf("entry name: %s, dir_name: %s\n", next_dir, name);
        if (compare_dir_name(name, next_dir, next_dir_len, dir_entry.name_len)==1)       {
            if (dir_entry.file_type != type) {
                return -ENOTDIR;
            }    
            *inode_nr = dir_entry.inode;
            return 0;
        }
        cur += dir_entry.rec_len;
    }
    return 1;
}
int handle_ind_block(int img, int i_block, int type, char*path, int *inode_nr,
                        int *buf) {
    int ret = pread(img, buf, BLOCK_SIZE, BLOCK_SIZE * i_block);
    if (ret < 0) {
        return -errno;
    }
    for (int i = 0; i < BLOCK_SIZE / 4; ++i) {
        if (buf[i] == 0) {            
            return -ENOENT;
        }
        ret = handle_direct_block(img, type, path, inode_nr, buf[i]);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

int handle_indir_block(int img, int i_block, int type, char *path, int *inode_nr,
                        int *buf) {
    int dir_buf[BLOCK_SIZE];
    int ret = pread(img, buf, BLOCK_SIZE, i_block * BLOCK_SIZE);
    if (ret < 0) {
        return -errno;
    }
    for (int i = 0; i < BLOCK_SIZE / 4; ++i) {
        ret = handle_ind_block(img, buf[i], type, path, inode_nr, dir_buf);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}