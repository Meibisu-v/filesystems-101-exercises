#include <solution.h>

#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <string.h>
#include <stdio.h>
int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	int img = open("/home/elizaveta/rootfs.ext2_", O_RDONLY);
	// int img = open("../image.iso", O_RDONLY);
	int out = open("out", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);

	if (img < 0) {
		printf("%s\n", strerror(-img));
		errx(1, "open(img) failed");
	}
	if (out < 0)
		errx(1, "open(out) failed");

	/* 2 is the inode nr. of the root directory */
	int r = dump_file(img, 2, out);
	if (r < 0) {
		// errx(1, "dump_file() failed");
		printf("%s", strerror(-r));
	}

	close(out);
	close(img);

	return 0;
}
