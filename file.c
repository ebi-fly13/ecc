#include "ecc.h"

char *read_file(char *path) {
    FILE *fp;
    if (!strcmp(path, "-")) {
        fp = stdin;
    } else {
        fp = fopen(path, "r");
        if (!fp) {
            error("cannot open %s: %s\n", path, strerror(errno));
        }
    }
    char *buf;
    size_t buflen;
    FILE *out = open_memstream(&buf, &buflen);
    for (;;) {
        char buf2[4096];
        int n = fread(buf2, 1, sizeof(buf2), fp);
        if (n == 0) {
            break;
        }
        fwrite(buf2, 1, n, out);
    }
    if (fp != stdin) {
        fclose(fp);
    }

    fflush(out);
    if (buflen == 0 || buf[buflen - 1] != '\n') {
        fputc('\n', out);
    }
    fputc('\0', out);
    fclose(out);
    return buf;
}