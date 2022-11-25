#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
int main() {
    open("../helloworld/a_file_that_does_not_exist.txt", O_RDONLY);
    char* errorbuf = strerror(errno);
    printf("%s %d", errorbuf, errno);
}