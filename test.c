#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


char *dev_node1 = "/dev/Evan0";
char *dev_node2 = "/dev/Evan1";

int main(int argc, char **argv)
{
    int fd, n;
    char buf[1024] = "hello word";

    fd = open(dev_node1, O_RDWR);
    if (fd < 0) {
        perror("Fail when open");
        return -1;
    }
    printf("open %s successful, fd = %d\n", dev_node1, fd);

    n = write(fd, buf, strlen(buf));
    if (n < 0) {
        perror("Fail to write");
        return -1;
    }
    printf("write %d bytes[%s] successful\n", n, buf);

    n = write(fd, buf, strlen(buf));
    if (n < 0) {
        perror("Fail to write");
        return -1;
    }
    printf("write %d bytes[%s] successful\n", n, buf);

    if (lseek(fd, 0, 0) < 0) {
        perror("Fail to lseek");
        return -1;
    }
    char data[1024] = { 0 };

    n = read(fd, data, 10);
    if (n < 0) {
        perror("Fail to read");
        return -1;
    }
    printf("read %d bytes[%s] successful\n", n, data);
    memset(data, 0, 1024);
    n = read(fd, data, 10);
    if (n < 0) {
        perror("Fail to read");
        return -1;
    }
    printf("read %d bytes[%s] successful\n", n, data);

    close(fd);

    return 0;
}
