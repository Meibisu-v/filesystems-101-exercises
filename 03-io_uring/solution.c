#include <solution.h>
#include <liburing.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define ENTRIES 4
#define READ_SIZE (256*1024)

struct io_data {
    int index;
    off_t offset;
    struct iovec iovec;
};

static size_t current_entries;

static int read_write_queue(struct io_uring *ring, off_t length, off_t offset, int in, int out) {

    int ret = 0;
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        return errno;
    }
    struct io_data* iovecs;
    void *ptr = malloc(length + sizeof(*iovecs));
    if (!ptr) {
        return errno;
    }
    iovecs = ptr + length;            
    iovecs->iovec.iov_len = length;
    iovecs->iovec.iov_base = ptr;
    iovecs->offset = offset;
    iovecs->index = 0;
    io_uring_prep_readv(sqe, in, &iovecs->iovec, 1, offset);
    sqe->flags |= IOSQE_IO_LINK;
    io_uring_sqe_set_data(sqe, iovecs);

    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        // printf("%s", strerror(errno));
        ret = errno;
    }
    io_uring_prep_writev(sqe, out, &iovecs->iovec, 1, offset);
    io_uring_sqe_set_data(sqe, iovecs);
    return ret;
}


int get_file_size(int fd) {
	struct stat st;
	if (fstat(fd, &st) < 0)
		return -1;
	return st.st_size;
}

int copy(int in, int out)
{
    (void) in;
    (void) out;

    struct io_uring ring;

    // init queue
    int ret = io_uring_queue_init(ENTRIES, &ring, 0);
    if (ret < 0) {
        return -ret;
    }
    //file size
    int input_size = get_file_size(in);
    if (input_size < 1) {
        return -errno;
    }
    //copy
    off_t remain_to_read = input_size, offset = 0;
    while (remain_to_read) {
        int in_queue = current_entries;
        while (current_entries < ENTRIES && remain_to_read) {
            // printf("entries %ld\n remain to read %ld\n", current_entries, remain_to_read);
            off_t bytes_to_read = remain_to_read;
            if (bytes_to_read > READ_SIZE) {
                bytes_to_read = READ_SIZE;
            }
            // read
            int code = read_write_queue(&ring, bytes_to_read, offset, in, out);
            if (code < 0) {
                return -code;
            }
            // 
            remain_to_read -= bytes_to_read;
            offset += bytes_to_read;
            current_entries += 2;
        }
        if (in_queue != (int) current_entries) {
            ret = io_uring_submit(&ring);
            if (ret < 0) {
                return -ret;
            }
        }

        struct io_uring_cqe *cqe;
        size_t to_write = remain_to_read ? ENTRIES : 1;
        while (current_entries >= to_write) {
            ret = io_uring_wait_cqe(&ring, &cqe);
            if (ret < 0) {
                return -ret;
            }
            //
            struct io_data *iovecs = io_uring_cqe_get_data(cqe);
            ++iovecs->index;
            if (cqe->res < 0) {
                if (cqe->res == -ECANCELED) {
                    if (read_write_queue(&ring, READ_SIZE, iovecs->offset, in, out) < 0) {
                        // printf("%s", strerror(errno));
                        // return -errno;
                    }
                    current_entries += 2;
                } else {
                    return -errno;
                }
            }
            // printf("index = %d\n", iovecs->index);
            if (iovecs->index == 2) {
                void *ptr = (void*) iovecs - iovecs->iovec.iov_len;
                // printf("free\n");
                // printf("offset %ld\n", iovecs->offset);
                free(ptr);
            }
            io_uring_cqe_seen(&ring, cqe);
            --current_entries;
        }
    }
    io_uring_queue_exit(&ring);
    return 0;
}
