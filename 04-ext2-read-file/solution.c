#include <solution.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>

#define BLOCK_INIT 1024

int handle_direct_blocks(int img, int out, uint i_block, uint block_size, 
                         long *offset);
int handle_ind_block(int img, int out, uint i_block, uint block_size, 
                     long *offset);
int handle_double_ind_block(int img, int out, uint i_block, uint block_size, 
                            long *offset);

int dump_file(int img, int inode_nr, int out) {
    (void) img;
    (void) inode_nr;
    (void) out;
    // read superblock
    struct ext2_super_block s_block;
    int ret = lseek(img, BLOCK_INIT, SEEK_SET);
    if (ret < 0) {
        return -errno;
    }
    ret = read(img, &s_block, sizeof(s_block));
    if (ret < 0) {
        return -errno;
    }	
    // 
    uint BLOCK_SIZE = BLOCK_INIT << s_block.s_log_block_size;
    // таблица дескрипторов
    struct ext2_group_desc g_desc;
    uint offset = (inode_nr - 1) / s_block.s_inodes_per_group * sizeof(g_desc);
    offset = (BLOCK_SIZE > BLOCK_INIT) ? (offset + BLOCK_SIZE) : (offset + BLOCK_SIZE*2);
    ret = lseek(img, offset, SEEK_SET);
    if (ret < 0) {
        return -errno;
    }
    ret = read(img, &g_desc, sizeof(g_desc));
    if (ret < 0) {
        return -errno;
    }
    // 
    struct ext2_inode inode;
    uint index = (inode_nr - 1) % s_block.s_inodes_per_group;
    uint pos = g_desc.bg_inode_table * BLOCK_SIZE + 
            (index * s_block.s_inode_size);
    ret = lseek(img, pos, SEEK_SET);
    if (ret < 0) {
        return -errno;
    }
    if (read(img, &inode, sizeof(inode)) < 0) {
        return -errno;
    }
    //copy
    long size = inode.i_size;
    for (size_t i = 0; i < EXT2_NDIR_BLOCKS; ++i) {
        int ret = handle_direct_blocks(img, out, inode.i_block[i], BLOCK_SIZE, 
                                    //    buffer, 
                                       &size);
        if (ret < 0) return ret;
    }
    ret = handle_ind_block(img, out, inode.i_block[EXT2_NDIR_BLOCKS + 1], BLOCK_SIZE,
                            &size);
    if (ret < 0) {
        return ret;
    }
    ret = handle_double_ind_block(img, out, inode.i_block[EXT2_NDIR_BLOCKS + 2], BLOCK_SIZE,
                            &size);
    if (ret < 0) {
        return ret;
    }    
    return 0;
}


int handle_direct_blocks(int img, int out, uint i_block, uint block_size,
                         long *offset) {
    char buffer[block_size];
    int ret = lseek(img, i_block * block_size, SEEK_SET);
    if (ret < 0) {
        return -errno;
    }
    ret = read(img, buffer, block_size) < 0;
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
int handle_ind_block(int img, int out, uint i_block, uint block_size, 
                      long *offset) {
    int ret = lseek(img, i_block * block_size, SEEK_SET);
    if (ret < 0) {
        return -errno;
    }
    char *buffer_indir = malloc(sizeof(char) * block_size);
    if (read(img, buffer_indir, block_size) < 0) {
        return -errno;
    }
    uint *buffer_int = (uint*) buffer_indir;
    for (uint i = 0; i < block_size / 4; ++i) {
        ret = handle_direct_blocks(img, out, buffer_int[i], block_size, offset);
        if (ret < 0) {
            return ret;
        }
        if (*offset < 0) break;
    }
    free(buffer_indir);
    return 0;
}
int handle_double_ind_block(int img, int out, uint i_block, uint block_size, 
                            long *offset) {
    int ret = lseek(img, i_block * block_size, SEEK_SET);
    if (ret < 0) {
        return -errno;
    }
    char* double_ind_block_buffer = malloc(sizeof(char) * block_size);
    if (read(img, double_ind_block_buffer, block_size) < 0) {
        return -errno;
    }
    uint *buffer_int = (uint*) double_ind_block_buffer;
    for (uint i = 0; i < block_size / 4; ++ i) {
        ret = handle_ind_block(img, out, buffer_int[i], block_size, offset);
        if (ret < 0) return ret;
        if (*offset < 0) {
            break;
        }
    }
    free(double_ind_block_buffer);
    return 0;
}
