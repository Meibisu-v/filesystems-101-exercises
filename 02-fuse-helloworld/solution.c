#include <solution.h>
#include <errno.h>
#include <stdio.h>
#include <fuse.h>
#include <string.h>

static int
fs_readdir(const char *path, void *data, fuse_fill_dir_t filler,
           off_t off, struct fuse_file_info *ffi, enum fuse_readdir_flags fl)
{
    // printf( "--> Getting The List of Files of %s\n", path );
    (void) ffi;
    (void) off;
    (void) fl;
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(data, ".", NULL, 0, 0);
    filler(data, "..", NULL, 0, 0 );
    if ( strcmp( path, "/" ) == 0 ) 
        filler(data, "hello", NULL, 0, 0);
    return 0;
}

static int
fs_read(const char *path, char *buf, size_t size, off_t off,
        struct fuse_file_info *ffi)
{	
    // printf( "--> Trying to read %s, %lu, %lu\n", path, off, size );
    if(strcmp(path, "/hello") != 0)
        return -ENOENT;
    (void) ffi;
    ssize_t len;	
    struct fuse_context *fuse_pid = fuse_get_context();
    char hello_str[1024];
    sprintf(hello_str, "hello, %d\n%c", fuse_pid->pid, '\0');

    len = strlen(hello_str);

    if (off < len) {
        if (off + (ssize_t)size > len)
            size = len - off;
    }
    memcpy(buf, hello_str, size);

    return size;
}

static int
fs_open(const char *path, struct fuse_file_info *ffi)
{
    // printf("open called, path %s\n", path);
    if (strcmp(path, "/hello") != 0)
        return -ENOENT;

    if ((ffi->flags & O_ACCMODE) != O_RDONLY)
    	return -EROFS;
    (void) ffi;

    return 0;
}

static int
fs_getattr(const char *path, struct stat *st, struct fuse_file_info *ffi)
{
    
    // printf( "[getattr] Called\n" );	
    // printf( "\tAttributes of %s requested\n", path );
    (void) ffi;
    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_atime = time( NULL );
    st->st_mtime = time( NULL );
    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0775;
        st->st_nlink = 2;
    } else if (strcmp(path, "/hello") == 0){
        st->st_mode = S_IFREG | 0400;
        st->st_nlink = 1;
        st->st_size = strlen("hello \n") + 8;
    } else {
        return -ENOENT;
    }

    return 0;
}

static const struct fuse_operations hellofs_ops = {
    .readdir = fs_readdir,
    .read = fs_read,
    .open = fs_open,
    .getattr = fs_getattr,
};

int helloworld(const char *mntp)
{
    char *argv[] = {"exercise", "-f", (char *)mntp, NULL};
    return fuse_main(3, argv, &hellofs_ops, NULL);
}
