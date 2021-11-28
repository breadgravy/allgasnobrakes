#include <cassert>
#include <cstdio>
#include <vector>

#include "color.hpp"
#include "fs.hpp"

enum OpCode {
#define DECL_ENUM
#include "opcode_def.hpp"
#undef DECL_ENUM
};
const char* opcode_to_str[] = {
#define DECL_STRING_TABLE
#include "opcode_def.hpp"
#undef DECL_STRING_TABLE
};

enum class Tag { VAL_NULL, VAL_NUM, VAL_OBJ };
struct Value {
    Tag tag = Tag::VAL_NULL;
    union {
        double num;
    };
};

struct MetaData {
    int lineno = -1;
};

typedef int ConstIdx;
struct Chunk {
    void addOp(OpCode op, int lineno = -1) {
        code.push_back(op);
        metadata.push_back({lineno});
    }
    void addConstOp(double val, int lineno=-1) {
        auto idx = regConstVal(val);
        addOp(OP_CONSTANT);
        // constant is Constidx stored directly in bytecode stream
        addOp(OpCode(idx));
    }
    ConstIdx regConstVal(double constant) {
        assert(constants.size() < 255);
        constants.push_back({.tag = Tag::VAL_NUM, .num = constant});
        return constants.size() - 1;
    }
    Value getConst(ConstIdx idx) { return constants.at(idx); }

    void finalize(){
        // chunk always ends in EOF token
        if (code.back() != OP_EOF){ addOp(OP_EOF); }
        assert(metadata.size() == code.size());
    }
    auto begin() { return code.begin(); }
    auto end() { return code.end(); }

    void list() {
        int i = 0;
        printf(CYAN "== BYTECODE LISTING ==\n");
        for (auto it = code.begin(); it != code.end(); it++, i++) {
            OpCode op = *it;
            printf(CYAN "%d" RESET ": %s (%d) \n", i, opcode_to_str[op], op);
            if (op == OP_CONSTANT) {
                op = *(++it);
                i++;
                // assume constant is a number
                printf(CYAN "%d" RESET ": \tCONST=%g\n", i, getConst(op).num);
            } 
        }
    }

    ////////////////////////////////////////////////////////////////////
    private:
    std::vector<Value> constants;
    std::vector<OpCode> code;
    std::vector<MetaData> metadata;
};

#define DEBUG(...) if (debug) printf(__VA_ARGS__);

enum class VMStatus { OK, ERR, INF_LOOP };
struct VM {
    OpCode readOp() { return *ip++; } 
    VMStatus run(){
        ip = code.begin();
        constexpr int max_icount = 50;
        auto end = code.end();
        for (int icount=0; icount < max_icount && ip != end; icount++){
            switch (OpCode op = readOp()){
                case OP_NOP: { 
                    DEBUG("%d: OP_NOP\n",icount);
                    break;
                }
                case OP_CONSTANT: { 
                    //auto cnst = code.getConst(readOp());
                    DEBUG("%d: OP_CONSTANT \n",icount);
                    break;
                }
                case OP_EOF: {
                    DEBUG(RED "%d: OP_EOF\n" RESET,icount);
                    return VMStatus::ERR;
                }
                case OP_RETURN: {
                    DEBUG("%d: OP_RETURN \n",icount);
                    return VMStatus::OK;
                }
                default: {
                    DEBUG("%d: found unknown op code %d\n",icount,op);
                    assert(0);
                }
            }
        }
        return VMStatus::INF_LOOP;
    }
    ////////////////////////////////////////////////////////////////////
    Chunk code;
    std::vector<OpCode>::const_iterator ip;
};

void dumpCode() {
    VM vm;
    auto& code = vm.code;
    code.addOp(OP_NOP);
    code.addOp(OP_NOP);
    code.addConstOp(10);
    code.addOp(OP_RETURN);
    code.finalize();

    code.list();

    printDiv("VM Start");
    auto printStatus = [](auto arg){ printf("\n--\nVm exited with status %s\n",arg); };
    switch(auto stat = vm.run()){
        case VMStatus::OK: 
            printStatus(GREEN "OK" RESET);
            break;
        case VMStatus::INF_LOOP: break;
            printStatus(YELLOW "INF_LOOP" RESET);
            break;
        case VMStatus::ERR: break;
            printStatus(RED "ERR" RESET);
            break;
        default:
            printStatus(RED "UNKNOWN" RESET);
            printf("\tUNKNOWN status is %d\n",stat);
            assert(0);
            break;
    }
}
