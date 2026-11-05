#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

/* Just in case it doesn't parse. Though it should. */
#define defer

int main(int argc, char** argv) {
    int k = 0;
    defer k++;

    fprintf(stderr, "poop\n");

    for (int i = 1; i < argc; i++) {
        FILE* f = fopen(argv[i], "r");
        defer { fclose(f); }

        if (!f) {
            fprintf(stderr, "Failed to open %s: %s\n", argv[i], strerror(errno));
            continue;
        }

        char buf[4096];
        buf[fread(buf, 1, 4095, f)] = 0;

        fprintf(stderr, "%s", buf);
        
        assert(f);

    }
    assert(k == 0);

    int j = 10;
    do {
        defer fprintf(stderr, "ligma: %i\n", j);
        j--;
        if (j == 5) break;
    } while (j > 0);

    return 0;
}

