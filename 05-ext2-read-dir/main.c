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

	int img = open("../rootfs.ext2", O_RDONLY);
	// int img = open("../image.iso", O_RDONLY);
	if (img < 0){
		printf("%s\n", strerror(-img));
		errx(1, "open(img) failed");
	}
	/* 2 is the inode nr. of the root directory */
	int r = dump_dir(img, 2);
	if (r < 0){
		// errx(1, "dump_file() failed");
		printf("%s", strerror(-r));
	}
	close(img);
	return 0;
}
