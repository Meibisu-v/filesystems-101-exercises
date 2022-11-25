#include <solution.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <string.h>
int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	int img = open("/home/elizaveta/howtogeek.img", O_RDONLY);
	int out = open("out", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
	printf("img %d\n", img);
	if (img < 0) {
		printf("%s\n", strerror(-img));
		errx(1, "open(img) failed");
	}

	if (out < 0)
		errx(1, "open(out) failed");

	int r = dump_file(img, "/dir1/dir3/make_config.sh", out);
	if (r < 0) {
		printf("%s\n", strerror(-r));
		// errx(1, "dump_file() failed");
	}

	close(out);
	close(img);

	return 0;
}
