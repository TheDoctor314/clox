#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

void repl();
void runFile(const char *path);

int main(int argc, char **argv) {
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fputs("Usage: clox [script]\n", stderr);
        exit(64);
    }

    freeVM();
    return 0;
}

void repl() {
    char *line = NULL;
    size_t line_len = 0;

    while (true) {
        fputs(">> ", stdout);
        fflush(stdout);

        if (getline(&line, &line_len, stdin) < 0) {
            fputs("\n", stdout);
            free(line);
            return;
        }

        interpret(line);
    }

    free(line);
}

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen: ");
        exit(74);
    }

    fseek(fp, 0l, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);

    char *buf = (char *)malloc((file_size) + 1);
    if (!buf) {
        perror("malloc: ");
        exit(74);
    }

    size_t bytes_read = fread(buf, sizeof(char), file_size, fp);
    if (bytes_read < file_size) {
        fprintf(stderr, "Could not read file '%s'", path);
        exit(74);
    }
    buf[bytes_read] = '\0';

    fclose(fp);
    return buf;
}

void runFile(const char *path) {
    char *src = read_file(path);
    InterpretResult ret = interpret(src);

    free(src);

    if (ret == INTERPRET_COMPILE_ERR)
        exit(65);
    if (ret == INTERPRET_RUNTIME_ERR)
        exit(70);
}
