#include <cassert>
#include <cstdio>
#include <ctype.h>

#include "cfg.hpp"
#include "codegen.hpp"
#include "color.hpp"
#include "err.hpp"
#include "fs.hpp"
#include "parse.hpp"
#include "re.hpp"
#include "scan.hpp"
#include "time.hpp"
#include "vm.hpp"

ErrCode run_file(char* filepath, bool dump_source) {

    auto starttime = getTime();

    char source_buf[MAX_FILE_SIZE];
    printDiv("Read File");
    starttime = getTime();
    ErrCode stat = slurpFile(filepath, source_buf);
    printf(YELLOW "Read File took %.3g ms\n" RESET, timeSinceMilli(starttime));
    if (stat != SUCCESS)
        return stat;

    if (dump_source) {
        printDiv("Source Listing");
        dumpSourceListing(source_buf);
    }

    printDiv("Scanner");
    starttime = getTime();
    Scanner scanner(source_buf);
    auto tokens = scanner.scan();
    printf(YELLOW "Scanner took %.3g ms for %ld tokens\n" RESET,
           timeSinceMilli(starttime),
           tokens.size());

    printDiv("Parser");
    starttime = getTime();
    Parser parser(tokens);
    std::vector<Expr*> statements = parser.ParseStatements();
    printf(YELLOW "Parser took %.3g ms\n" RESET, timeSinceMilli(starttime));

    printDiv("Parser Output");
    for (auto& stmt : statements) {
        stmt->print(0, true);
    }

    printDiv("CodeGen");
    CodeGen codegen(statements);
    codegen.genCode();

    printDiv("Cleanup");
    for (auto& stmt : statements) {
        delete (stmt);
    }

    printf(YELLOW "took %.3g ms\n" RESET, timeSinceMilli(starttime));
    return SUCCESS;
}

void run_prompt() { printf("prompt goes here\n"); }

void run_vm() { }

int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc == 2) {
        run_file(argv[1], false);
    } else if (argc == 1) {
        run_vm();
    } else {
        printf("too many args!\n");
    }

    return 0;
}
