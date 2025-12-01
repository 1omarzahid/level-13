#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "md5.h"

/*
 * makehash
 * usage: ./makehash <inputfile> <outputfile>
 *
 * This program reads each line from the input file, removes the trailing
 * newline, runs md5() on that line, and writes the resulting 32-char
 * hex digest to the output file (one hash per line).
 */

int main(int argc, char *argv[]) {
    // check that we got exactly two filenames
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source file> <destination file>\n", argv[0]);
        return 1;
    }

    const char *src_filename = argv[1];
    const char *dst_filename = argv[2];

    // open input file for reading
    FILE *src = fopen(src_filename, "r");
    if (src == NULL) {
        perror("Error opening source file");
        return 1;
    }

    // open output file for writing
    FILE *dst = fopen(dst_filename, "w");
    if (dst == NULL) {
        perror("Error opening destination file");
        fclose(src);
        return 1;
    }

    // buffer to hold each line we read
    // assuming lines aren't longer than 1024 chars
    char buffer[1024];

    // read the file line by line
    while (fgets(buffer, sizeof(buffer), src) != NULL) {
        // trim the newline at the end (if there is one)
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len -= 1;
        }

        // get the md5 hash of this line (without the newline)
        char *hash = md5(buffer, (int)len);
        if (hash == NULL) {
            // shouldn't really happen, but just in case
            fprintf(stderr, "Error creating MD5 digest.\n");
            fclose(src);
            fclose(dst);
            return 1;
        }

        // write the hash to the output file
        fprintf(dst, "%s\n", hash);

        // md5() mallocs the string it returns, so we have to free it
        free(hash);
    }

    // close both files when we're done
    fclose(src);
    fclose(dst);

    return 0;
}
