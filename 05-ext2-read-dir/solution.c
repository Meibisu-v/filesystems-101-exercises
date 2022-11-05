#include <solution.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>

#define BLOCK_INIT 1024

int handle_direct_blocks(int img, uint i_block, uint block_size);
// int handle_ind_block(int img, uint i_block, uint block_size, 
//                      long *offset);
// int handle_double_ind_block(int img, uint i_block, uint block_size, 
                            // long *offset);

int dump_dir(int img, int inode_nr) {
	// read superblock
    struct ext2_super_block s_block;
    int ret =  pread(img, &s_block, sizeof(s_block), BLOCK_INIT);
    if (ret < 0) {
        return -errno;
    }	
    
    int BLOCK_SIZE = BLOCK_INIT << s_block.s_log_block_size;
    //
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
    //copy
    // long size = inode.i_size;
    for (size_t i = 0; i < EXT2_N_BLOCKS; ++i) {
        if (i < EXT2_NDIR_BLOCKS) {
            int ret = handle_direct_blocks(img, inode.i_block[i], BLOCK_SIZE);
            if (ret < 0) return ret;
        }
        // if (i == EXT2_IND_BLOCK) {
        //     ret = handle_ind_block(img, inode.i_block[i], BLOCK_SIZE,
        //                             &size);
        //     if (ret < 0) {
        //         return ret;
        //     }
        // }
        // // if (i == EXT2_DIND_BLOCK) {
        //     ret = handle_double_ind_block(img, inode.i_block[i], BLOCK_SIZE,
        //                             &size);
        //     if (ret < 0) {
        //         return ret;
        //     }  
        // }
		// if (i == EXT2_TIND_BLOCK) {
		// 	ret = handle_tind_block();
		// 	if (ret < 0) {
		// 		return ret;
		// 	}
		// }
    }  
    return 0;
}

char file_type_ch(unsigned char file_type){
    if (file_type == EXT2_FT_REG_FILE) {
        return 'f';
    }
    if (file_type == EXT2_FT_DIR) {
        return 'd';
    }
    return -1;
}
void copy_name(const char* from, char* to, int len) {
    for (int i = 0; i < len; ++i) {
        to[i] = from[i];
    }
    to[len] = '\0';
}
int handle_direct_blocks(int img, uint i_block, uint block_size) {
    struct ext2_dir_entry_2 dir_entry;
    int next_dir_pos = i_block * block_size;
    int remains = block_size;
    while (1) {
        int ret = pread(img, &dir_entry, sizeof(dir_entry), next_dir_pos);
        if (dir_entry.inode == 0) break;
        if (!remains) break;
        if (ret < 0) {
            return -errno;
        }        
        char name[dir_entry.name_len + 1];
        copy_name(dir_entry.name, name, dir_entry.name_len);
        report_file(dir_entry.inode, file_type_ch(dir_entry.file_type), name);
        next_dir_pos += dir_entry.rec_len;
        remains -= dir_entry.rec_len;
    }
    return 0;
}
int handle_ind_block(int img, uint i_block, uint block_size) {
    char *buffer_indir = malloc(sizeof(char) * block_size);
    int ret = pread(img, buffer_indir, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    uint *buffer_int = (uint*) buffer_indir;
    for (uint i = 0; i < block_size / sizeof(uint); ++i) {
        ret = handle_direct_blocks(img, buffer_int[i], block_size);
        if (ret < 0) {
            return ret;
        }
    }
    free(buffer_indir);
    return 0;
}
int handle_double_ind_block(int img, uint i_block, uint block_size) {
    char* double_ind_block_buffer = malloc(sizeof(char) * block_size);
    int ret = pread(img, double_ind_block_buffer, block_size, i_block * block_size);
    if (ret < 0) {
        return -errno;
    }
    uint *buffer_int = (uint*) double_ind_block_buffer;
    for (uint i = 0; i < block_size / 4; ++ i) {
        ret = handle_ind_block(img, buffer_int[i], block_size);
        if (ret < 0) return ret;
    }
    free(double_ind_block_buffer);
    return 0;
}
