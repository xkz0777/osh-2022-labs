#define SYS_HELLO 548
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    long res1, res2;
    char buf1[5]; // length not enough
    char buf2[17]; // length enough
    size_t len1, len2;
    len1 = 4;
    len2 = 16;
    res1 = syscall(SYS_HELLO, buf1, len1);
    printf("Test1: buffer size too small, return value: %ld\n", res1);
    res2 = syscall(SYS_HELLO, buf2, len2);
    printf("Test2: buffer size is enough, return value: %ld, contents in buf2: %s", res2, buf2);
    while (1) {}
}
