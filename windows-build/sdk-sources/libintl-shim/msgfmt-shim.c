#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *output = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
            continue;
        }
        if (strncmp(argv[i], "--output-file=", 14) == 0) {
            output = argv[i] + 14;
            continue;
        }
    }

    if (!output) {
        fprintf(stderr, "msgfmt-shim: missing -o output file\n");
        return 1;
    }

    FILE *file = fopen(output, "wb");
    if (!file) {
        perror(output);
        return 1;
    }

    /* Empty GNU MO header: magic, revision, strings, original table, translation table. */
    const unsigned int header[] = {
        0x950412deu,
        0u,
        0u,
        28u,
        28u,
        0u,
        28u,
    };
    if (fwrite(header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}
