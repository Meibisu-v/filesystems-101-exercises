#include <solution.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#define _STRUCT_TIMESPEC 1
#include <stdlib.h>
#include <ntfs-3g/volume.h>
#include <ntfs-3g/dir.h>

int dump_file(int img, const char *path, int out) {
	char path_to_fs[PATH_MAX];
	sprintf(path_to_fs, "/proc/self/fd/%d", img);
	char *filename = calloc(1, NAME_MAX);
	int ret = readlink(path_to_fs, filename, NAME_MAX);
	free(filename);
	if (ret < 0) {
		return -errno;
	}
	ntfs_volume *volume = ntfs_mount(path_to_fs, NTFS_MNT_RDONLY);
	if (volume == NULL) {
		return -errno;
	}
	ntfs_inode *inode = ntfs_pathname_to_inode(volume, NULL, path);
	if (inode == NULL) {
		ntfs_umount(volume, TRUE);		
		return -errno;
	}
	ntfs_attr *attribute = ntfs_attr_open(inode, AT_DATA, NULL, 0);
	if (!attribute) {
		ntfs_inode_close(inode);
		ntfs_umount(volume, TRUE);
		return -errno;
	}
	s64 pos = 0;
	char *buf = calloc(1, BUFSIZ);
	while (1) {
		s64 ret = ntfs_attr_pread(attribute, pos, BUFSIZ, buf);
		if (ret < 0) {
			free(buf);
			return -errno;
		}
		if (ret == 0) {
			// free(buf);
			break;
		}
		int write_ret = write(out, buf, ret);
		if (write_ret < 0) {
			free(buf);
			return -errno;
		}
		pos += ret;
	}
	free(buf);
	ntfs_attr_close(attribute);
	ntfs_inode_close(inode);
	ntfs_umount(volume, TRUE);
	return 0;
}