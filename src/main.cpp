#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

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
    for (; *str != '\n' && *str != '\0'; ++str)
        ;
    if (*str == '\n') {
        return str;
    } else {
        // printf("found end of string\n");
        return nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////

enum TokenType { INVALID = 0, ID, NUM, PLUS, MINUS, DIV, MULT };
const char* token_to_str[] = {"INVALID", "ID",  "NUM", "PLUS",
                              "MINUS",   "DIV", "MULT"};

struct Token {
    TokenType type = INVALID;
    const std::string str = "";
    int lineno = 0;
    int linepos = 0;
};

struct Scanner {
    Scanner() = delete;
    Scanner(const char* buf) : _buf(buf), _sz(strlen(buf)) {}
    void scan() {
        char ch = *_buf;
        lineno = 0;
        linepos = 0;
        while (ch != '\0') {
            switch (ch) {
            case 'a' ... 'z':
            case 'A' ... 'Z':
            case '_': {
                // printf("%3ld: found alpha  char '%c'\n", ch);
                // TODO: consume entire identifier
                tok(ID, consumeId());
                break;
            }
            case '0' ... '9': {
                // printf("%3ld: found digit  char '%c'\n", ch);
                tok(NUM, consumeNum());
                break;
            }
            case '+': {
                // printf("%3ld: found plus   char '%c'\n", ch);
                tok(PLUS, str(ch));
                break;
            }
            case '-': {
                // printf("%3ld: found minus  char '%c'\n", ch);
                tok(MINUS, str(ch));
                break;
            }
            case '/': {
                // printf("%3ld: found div    char '%c'\n", ch);
                tok(DIV, str(ch));
                break;
            }
            case '*': {
                // printf("%3ld: found mult   char '%c'\n", ch);
                tok(MULT, str(ch));
                break;
            }
            case '\n':
            case '\r':
            case '\t':
            case ' ': {
                if (ch == '\n') {
                    lineno++;
                    linepos = 0;
                }
                // printf("%3ld: found wspace char '%c'\n", i, ch);
                break;
            }
            default: {
                printf("Found unimpl char '%c' (%d) at lineno %d, pos %d\n", ch,
                       ch, lineno, linepos);
                exit(0);
                break;
            }
            }
            ch = advance();
        }

        for (auto tok : _tokens) {
            printf("tok %5s = %15s at %d,%d  \n", token_to_str[tok.type], tok.str.c_str(),tok.lineno,tok.linepos);
        }
    }
    std::string consumeId() {
        std::string s;
        char ch = *_buf;
        assert(isalpha(ch) or ch == '_');
        while (isalnum(ch) or ch == '_') {
            s.push_back(ch);
            ch = advance();
        }
        stepback();
        return s;
    }
    std::string consumeNum() {
        std::string s;
        char ch = *_buf;
        assert(isdigit(ch));
        while (ch >= '0' and ch <= '9') {
            s.push_back(ch);
            ch = advance();
        }
        stepback();
        return s;
    }
    std::string str(char c) { return std::string(1, c); }
    void tok(TokenType type, std::string str) {
        _tokens.push_back({.type = type,
                           .str = std::move(str),
                           .lineno = lineno,
                           .linepos = linepos});
    }
    char peek() { return _buf[1]; }
    char advance() {
        linepos++;
        return *(++_buf);
    }
    char stepback() {
        linepos--;
        return *(--_buf);
    }

    std::vector<Token> _tokens;
    int linepos = 0;
    int lineno = 0;
    const char* _buf = nullptr;
    const size_t _sz = 0;
};

///////////////////////////////////////////////////////////////////////////////

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
        printf("==========================================================="
               "===="
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
        printf("==========================================================="
               "===="
               "=================\n");
    }

    // get tokens from file
    Scanner scanner(filebuf);
    scanner.scan();
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
