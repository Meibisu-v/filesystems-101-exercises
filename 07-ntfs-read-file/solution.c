#include <solution.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#define _STRUCT_TIMESPEC 1
#include <stdlib.h>
#include <ntfs-3g/volume.h>
// #include <ntfs-3g/dir.h>

ntfs_inode *ntfs_pathname_to_inode(ntfs_volume *vol, ntfs_inode *parent,
		const char *pathname);

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
		int return_val = -errno;
		ntfs_umount(volume, TRUE);		
		return return_val;
	}
	ntfs_attr *attribute = ntfs_attr_open(inode, AT_DATA, NULL, 0);
	if (!attribute) {
		int return_val = -errno;
		ntfs_inode_close(inode);
		ntfs_umount(volume, TRUE);
		return return_val;
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

#include <ntfs-3g/dir.h>
#include <string.h>
ntfs_inode *ntfs_pathname_to_inode(ntfs_volume *vol, ntfs_inode *parent,
		const char *pathname)
{
	u64 inum;
	int len, err = 0;
	char *p, *q;
	ntfs_inode *ni;
	ntfs_inode *result = NULL;
	ntfschar *unicode = NULL;
	char *ascii = NULL;

	if (!vol || !pathname) {
		errno = EINVAL;
		return NULL;
	}
	
	ntfs_log_trace("path: '%s'\n", pathname);
	
	ascii = strdup(pathname);
	if (!ascii) {
		ntfs_log_error("Out of memory.\n");
		err = ENOMEM;
		goto out;
	}

	p = ascii;
	/* Remove leading /'s. */
	while (p && *p && *p == PATH_SEP)
		p++;

	if (parent) {
		ni = parent;
	} else {
		ni = ntfs_inode_open(vol, FILE_root);
		if (!ni) {
			ntfs_log_debug("Couldn't open the inode of the root "
					"directory.\n");
			err = EIO;
			result = (ntfs_inode*)NULL;
			goto out;
		}
	}

	while (p && *p) {
		/* Find the end of the first token. */
		q = strchr(p, PATH_SEP);
		if (q != NULL) {
			*q = '\0';
		}
		len = ntfs_mbstoucs(p, &unicode);
		if (len < 0) {
			ntfs_log_perror("Could not convert filename to Unicode:"
					" '%s'", p);
			err = errno;
			goto close;
		} else if (len > NTFS_MAX_NAME_LEN) {
			err = ENAMETOOLONG;
			goto close;
		}
		inum = ntfs_inode_lookup_by_name(ni, unicode, len);

		if (inum == (u64) -1) {
			ntfs_log_debug("Couldn't find name '%s' in pathname "
					"'%s'.\n", p, pathname);
			err = ENOENT;
			goto close;
		}

		if (ni != parent)
			if (ntfs_inode_close(ni)) {
				err = errno;
				goto out;
			}

		inum = MREF(inum);
		ni = ntfs_inode_open(vol, inum);
		if (!ni) {
			ntfs_log_debug("Cannot open inode %llu: %s.\n",
					(unsigned long long)inum, p);
			err = EIO;
			goto close;
		}
//------------------------------------------------------
		if ((ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) && q) {
			err = ENOTDIR;
			goto close;
		}
//------------------------------------------------------
		free(unicode);
		unicode = NULL;

		if (q) *q++ = PATH_SEP; /* JPA */
		p = q;
		while (p && *p && *p == PATH_SEP)
			p++;
	}

	result = ni;
	ni = NULL;
close:
	if (ni && (ni != parent))
		if (ntfs_inode_close(ni) && !err)
			err = errno;
out:
	free(ascii);
	free(unicode);
	if (err)
		errno = err;
	return result;
}
