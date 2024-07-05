#include <stdio.h>

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream) {
    return __getdelim(lineptr, n, delim, stream);
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    return __getline(lineptr, n, stream);
}

void flockfile(FILE *fp) { (void)fp; }
void funlockfile(FILE *fp) { (void)fp; }
