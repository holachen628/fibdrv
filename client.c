#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

char *bn_to_string(const unsigned int *number, int size)
{
    // log10(x) = log2(x) / log2(10) ~= log2(x) / 3.322
    size_t len = (8 * sizeof(int) * size) / 3 + 2;
    char *s = malloc(len);
    char *p = s;

    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    /* src.number[0] contains least significant bits */
    for (int i = size - 1; i >= 0; i--) {
        /* walk through every bit of bn */
        for (unsigned int d = 1U << 31; d; d >>= 1) {
            /* binary -> decimal string */
            int carry = !!(d & number[i]);
            for (int j = len - 2; j >= 0; j--) {
                s[j] += s[j] - '0' + carry;
                carry = (s[j] > '9');
                if (carry)
                    s[j] -= 10;
            }
        }
    }
    // skip leading zero
    while (p[0] == '0' && p[1] != '\0') {
        p++;
    }
    memmove(s, p, strlen(p) + 1);
    return s;
}

int main()
{
    long long sz;

    char buf[10000];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    // for (int i = 0; i <= offset; i++) {
    //     sz = write(fd, write_buf, strlen(write_buf));
    //     printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    // }

    // for (int i = 0; i <= offset; i++) {
    //     lseek(fd, i, SEEK_SET);
    //     sz = read(fd, buf, 1);
    //     printf("Reading from " FIB_DEV
    //            " at offset %d, returned the sequence "
    //            "%lld.\n",
    //            i, sz);
    // }

    // for (int i = offset; i >= 0; i--) {
    //     lseek(fd, i, SEEK_SET);
    //     sz = read(fd, buf, 1);
    //     printf("Reading from " FIB_DEV
    //            " at offset %d, returned the sequence "
    //            "%lld.\n",
    //            i, sz);
    // }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        int len = read(fd, buf, 1);
        unsigned int *tmp = (unsigned int *) buf;
        char *s = bn_to_string(tmp, len);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s\n",
               i, s);
    }
    close(fd);
    return 0;
}
