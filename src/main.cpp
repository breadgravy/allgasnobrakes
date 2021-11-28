#include <cassert>
#include <cstdio>
#include <ctype.h>

#include "cfg.hpp"
#include "re.hpp"
#include "color.hpp"
#include "err.hpp"
#include "fs.hpp"
#include "time.hpp"
#include "scan.hpp"
#include "parse.hpp"
#include "codegen.hpp"
#include "vm.hpp"
#include "err.hpp"

ErrCode run_file(char* filepath, bool dump_source) {

    auto starttime = getTime();

    char source_buf[MAX_FILE_SIZE];
    ErrCode stat = slurpFile(filepath, source_buf);
    if (stat != SUCCESS)
        return stat;

    if (dump_source){
        printDiv("Source Listing");
        dumpSourceListing(source_buf);
    }

    printDiv("Scanner");
    Scanner scanner(source_buf);
    auto tokens = scanner.scan();

    printDiv("Parser");
    Parser parser(tokens);
    std::vector<Expr*> statements = parser.ParseStatements();

    printDiv("Parser Output");
    for (auto& stmt : statements) {
        stmt->print(0, true);
    }

    printDiv("CodeGen");
    CodeGen codegen(statements);
    codegen.genCode();

    //printDiv("VM Startup");
    //dumpCode();

    printDiv("Cleanup");
    for (auto& stmt : statements) {
        delete(stmt);
    }

    printf(YELLOW "took %.3g ms\n" RESET,timeSinceMilli(starttime));
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
