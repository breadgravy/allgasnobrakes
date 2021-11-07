#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "re.hpp"

bool file_exists(std::string& filepath) {
    return access(filepath.c_str(), F_OK) == 0;
}
bool file_exists(const char* filepath) { return access(filepath, F_OK) == 0; }

size_t get_filesize(const char* filepath) {
    FILE* fp = fopen(filepath, "r");
    struct stat stat;
    fstat(fileno(fp), &stat);
    fclose(fp);
    return stat.st_size;
}

std::string get_abspath(const char* relpath) {
    char actualpath[1000];
    realpath(relpath, actualpath);
    return std::string(actualpath);
}

// returns ptr to rest of string if newline is found, or nullptr if nothing left
char* getnewlinebound(char* str) {
    for (; *str != '\n' && *str != '\0'; ++str);
    if (*str == '\n') {
        return str;
    } else {
        // printf("found end of string\n");
        return nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////

void scanner(const char* filebuf){
    
}

void run_file(char* filepath, bool dump_source) {
    if (!file_exists(filepath)) {
        printf("Input file '%s' does not exist\n", filepath);
        return;
    }

    printf("compiling file '%s'\n", get_abspath(filepath).c_str());

    // read file into a buffer
    size_t filesize = get_filesize(filepath);
    char* filebuf = (char*)malloc(filesize + 1);
    FILE* fp = fopen(filepath, "r");
    fread(filebuf, sizeof(char), filesize, fp);

    // detect errors if any and close fp
    if (ferror(fp) != 0) {
        fprintf(stderr, "Error reading file into memory '%s'\n", filebuf);
        return;
    }
    filebuf[filesize] = '\0'; // make sure null terminated
    fclose(fp);

    // dump input to screen for debugging if requested
    if (dump_source) {
        printf("==============================================================="
               "=================\n");
        char *startofline = filebuf, *endofline = filebuf;
        int lineno = 0;
        while ((endofline = getnewlinebound(startofline)) != nullptr) {
            printf("\u001b[36m%3d:\u001b[0m ", lineno);
            for (; startofline < endofline; ++startofline) {
                putchar(*startofline);
            }
            printf("\n");
            ++startofline; // move to char
            ++lineno;
        }
        printf("==============================================================="
               "=================\n");
    }

    // get tokens from file 
    scanner(filebuf);
}

void run_prompt() { printf("prompt goes here\n"); }

int main(int argc, char** argv) {

    if (argc == 2) {
        run_file(argv[1], true);
    } else if (argc == 1) {
        run_prompt();
    } else {
        printf("too many args!\n");
    }

    return 0;
}
