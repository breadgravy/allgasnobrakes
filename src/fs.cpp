#include <cassert>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

#include "err.hpp"
#include "color.hpp"
#include "fs.hpp"

void printDiv(const char* str) {
    static int phaseno = 1;
    printf("\n-----------------------------------------------------------------"
           "---------------\n");
    printf(" Phase %d : %s\n", phaseno++, str);
    printf("-------------------------------------------------------------------"
           "-------------\n");
}

bool file_exists(std::string& filepath) { return access(filepath.c_str(), F_OK) == 0; }
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

[[nodiscard]] ErrCode slurpFile(char* filepath, char* contents) {

    if (!file_exists(filepath)) {
        printf("Input file '%s' does not exist\n", filepath);
        return FILE_ERR;
    }

    // printf("compiling file '%s'\n", get_abspath(filepath).c_str());

    // read file into a buffer
    size_t filesize = get_filesize(filepath);
    assert(filesize < MAX_FILE_SIZE);
    FILE* fp = fopen(filepath, "r");
    fread(contents, sizeof(char), filesize, fp);

    // detect errors if any and close fp
    if (ferror(fp) != 0) {
        fprintf(stderr, "Error reading file into memory '%s'\n", filepath);
        return FILE_ERR;
    }

    // make sure null terminated
    contents[filesize] = '\0';

    fclose(fp);

    return SUCCESS;
}

void dumpSourceListing(char* source_buf) {
    // returns ptr to rest of string if newline is found, or nullptr if nothing left
    auto getnewlinebound = [](char* str) {
        char* linebound = nullptr;
        for (; *str != '\n' && *str != '\0'; ++str)
            ;
        if (*str == '\n') {
            linebound = str;
        }
        return linebound;
    };

    printf("===========================================================\n");
    char *startofline = source_buf, *endofline = source_buf;
    int lineno = 0;
    while ((endofline = getnewlinebound(startofline)) != nullptr) {
        printf(CYAN "%3d:" RESET, lineno);
        for (; startofline < endofline; ++startofline) {
            putchar(*startofline);
        }
        printf("\n");
        ++startofline; // move to char
        ++lineno;
    }
    printf("===========================================================\n");
}
