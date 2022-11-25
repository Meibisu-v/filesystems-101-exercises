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
	int out = open("out", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);

	if (img < 0) {
		printf("open error: %s\n", strerror(-img));
		errx(1, "open(img) failed");
	}
	if (out < 0) 
		errx(1, "open(out) failed");

	int r = dump_file(img, "/dir/dir2/dir3/make_config.sh", out);
	if (r >= 0) {
		printf("correct dump ret = %d\n", r);
	}// int r = dump_file(img, "/report.py", out); 
	else 
		printf("failed dump: %s\n", strerror(-r));

	// r = dump_file(img, "/dir/dir2/dir3/img", out);
	// if (r < 0){
	// 	// errx(1, "dump_file() failed");
	// 	printf("failed dump: %s\n", strerror(-r));
	// } else printf("success\n");
	// r = dump_file(img, "/dir", out);
	// if (r < 0){
	// 	// errx(1, "dump_file() failed");
	// 	printf("failed dump: %s\n num = %d", strerror(-r), r);
	// } else printf("success\n");
	close(out);
	close(img);

	return 0;
}
