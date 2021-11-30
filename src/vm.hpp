#pragma once

#include <cassert>
#include <cstdio>
#include <vector>

#include "cfg.hpp"
#include "color.hpp"
#include "fs.hpp"
#include "time.hpp"
#include "util.hpp"

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

enum class Tag { VAL_NULL, VAL_NUM, VAL_BOOL };
struct Value {
    Value() : tag(Tag::VAL_NULL) {}
    Value(double val) {
        tag = Tag::VAL_NUM;
        num = val;
    }
    Value(int val) {
        tag = Tag::VAL_NUM;
        num = double(val);
    }
    Value(bool val) {
        tag = Tag::VAL_BOOL;
        boolean = val;
    }
    bool isNum() const { return tag == Tag::VAL_NUM; }
    bool isBool() const { return tag == Tag::VAL_BOOL; }
    double asNum() const {
        assert(tag == Tag::VAL_NUM);
        return num;
    }
    bool asBool() const {
        if (isBool())
            return boolean;
        else if (isNum())
            return bool(num);
        else
            return false;
    }

    std::string str() const {
        if (isBool()) {
            return boolean ? MAGENTA "True" RESET : MAGENTA "False" RESET;
        } else if (isNum()) {
            char buf[100];
            sprintf(buf, GREEN "%g" RESET, num);
            return buf;
        } else {
            return RED "nil" RESET;
        }
    }

    /////////////////////////////////
    Tag tag = Tag::VAL_NULL;
    union {
        double num;
        bool boolean;
    };
};

struct MetaData {
    int lineno = -1;
};

typedef int ConstIdx;
struct Chunk {
    Chunk() {}
    Chunk(const Chunk& initcode)
        : constants(initcode.constants), code(initcode.code), metadata(initcode.metadata) {}
    void addOp(OpCode op, int lineno = -1) {
        code.push_back(op);
        metadata.push_back({lineno});
    }
    void addConstNum(double val, int lineno = -1) {
        auto idx = regConstVal<double>(val);
        addOp(OP_CONST);
        // constant is Constidx stored directly in bytecode stream
        addOp(OpCode(idx));
    }
    void addConstBool(bool val, int lineno = -1) {
        auto idx = regConstVal<bool>(val);
        addOp(OP_CONST);
        // constant is Constidx stored directly in bytecode stream
        addOp(OpCode(idx));
    }
    template <typename T> ConstIdx regConstVal(T constant) {
        assert(constants.size() < 255);
        constants.push_back(Value(constant));
        return constants.size() - 1;
    }
    Value getConst(ConstIdx idx) { return constants.at(idx); }

    void finalize() {
        // chunk always ends in EOF token
        assert(code.size());
        if (code.back() != OP_EOF) {
            addOp(OP_RET);
            addOp(OP_EOF);
        }
        assert(metadata.size() == code.size());
    }
    auto begin() { return code.begin(); }
    auto end() { return code.end(); }

    void list() {
        int i = 0;
        printf(CYAN "== BYTECODE LISTING ==\n");
        for (auto it = code.begin(); it != code.end(); it++, i++) {
            OpCode op = *it;
            printf(CYAN "  %d" RESET ": %s \n", i, opcode_to_str[op]);
            if (op == OP_CONST) {
                op = *(++it);
                i++;
                printf(CYAN "  %d" RESET ": \tCONST=%s\n", i, getConst(op).str().c_str());
            }
        }
    }

    ////////////////////////////////////////////////////////////////////
  private:
    std::vector<Value> constants;
    std::vector<OpCode> code;
    std::vector<MetaData> metadata;
};

#define DEBUG(...)                                                                                 \
    if (debug)                                                                                     \
        printf(__VA_ARGS__);

// note stack can be modified in place!
#define UNARY_OP(__op__)                                                                           \
    {                                                                                              \
        Value operand = tos();                                                                     \
        if (operand.isNum()) {                                                                     \
            tos().num = __op__ operand.asNum();                                                    \
        } else if (operand.isBool()) {                                                             \
            tos().boolean = __op__ operand.asBool();                                               \
        }                                                                                          \
    }

// NOTE: must pop b before a, as a will be pushed unto the stack first!
#define BINARY_OP(__op__)                                                                          \
    {                                                                                              \
        Value B = pop();                                                                           \
        Value A = pop();                                                                           \
        if (A.isNum() and B.isNum()) {                                                             \
            push(A.asNum() __op__ B.asNum());                                                      \
        } else {                                                                                   \
            push(A.asBool() __op__ B.asBool());                                                    \
        }                                                                                          \
    }

enum class VMStatus { OK, ERR, INF_LOOP };
struct VM {
    // returns pair of {op code , offset in bytecode chunk }
    std::pair<OpCode, int> readOp() { return {*ip++, ip - code.begin()}; }
    VM(const Chunk& initcode) : code(initcode) {
        code.finalize();
        code.list();
        push(Value()); // push null val into first position on the stack
    }
    void printStatus(const char* arg) {
        printf(CYAN BOLD "Exit status = %s\n\n" RESET, arg);
    };
    VMStatus run() {
        printf(GREEN BOLD "\nVM Starting!\n"
                          "------------\n" RESET);

        auto starttime = getTime();
        auto stat = exec();
        printf(CYAN BOLD "\n-----------------------\n"
                         "VM completed in %.2g μs\n" RESET,
               timeSinceMicro(starttime));

        switch (stat) {
        case VMStatus::OK:
            printStatus(GREEN "OK" RESET);
            break;
        case VMStatus::INF_LOOP:
            break;
            printStatus(YELLOW "INF_LOOP" RESET);
            break;
        case VMStatus::ERR:
            break;
            printStatus(RED "ERR" RESET);
            break;
        default:
            printStatus(RED "UNKNOWN" RESET);
            ERR("\tUNKNOWN status is %d\n", stat);
            break;
        }
        return stat;
    }
    VMStatus exec() {
        ip = code.begin();
        constexpr int max_icount = 50;
        auto end = code.end();
        for (int icount = 0; icount < max_icount && ip != end; icount++) {
            auto op_pair = readOp();
            OpCode op = op_pair.first;
            int pos = op_pair.second;

            auto printOp = [this, op, pos]() {
                DEBUG("%3d: %-8s %s\n", pos, opcode_to_str[op], tos().str().c_str());
            };

            switch (op) {
            case OP_NOP: {
                printOp();
                break;
            }
            case OP_CONST: {
                push(code.getConst(readOp().first));
                printOp();
                break;
            }
            case OP_NOT: {
                UNARY_OP(!);
                printOp();
                break;
            }
            case OP_NEG: {
                UNARY_OP(-);
                printOp();
                break;
            }
            case OP_ADD: {
                BINARY_OP(+);
                printOp();
                break;
            }
            case OP_SUB: {
                BINARY_OP(-);
                printOp();
                break;
            }
            case OP_MULT: {
                BINARY_OP(*);
                printOp();
                break;
            }
            case OP_DIV: {
                BINARY_OP(/);
                printOp();
                break;
            }
            case OP_AND: {
                BINARY_OP(&&);
                printOp();
                break;
            }
            case OP_OR: {
                BINARY_OP(||);
                printOp();
                break;
            }
            case OP_CMP: {
                BINARY_OP(==);
                printOp();
                break;
            }
            case OP_PRINT: {
                printf(BOLD "vmprint: %s\n" RESET, tos().str().c_str());
                printOp();
                break;
            }
            case OP_EOF: {
                printOp();
                return VMStatus::ERR;
            }
            case OP_RET: {
                printOp();
                return VMStatus::OK;
            }
            default: {
                DEBUG(RED "%d: unimplemented op code %s (%d) \n" RESET, pos, opcode_to_str[op], op);
                assert(0);
            }
            }

            // print stack after opcode processed
            if (debug && debug_vmstack) {
                printf("\t\tSTACK \n\t\t{\n");
                for (int i = stack.size() - 1; i > 0; --i) {
                    Value val = stack[i];
                    printf("\t\t\t[%2d] %s\n", i, val.str().c_str());
                }
                printf("\t\t}\n");
            }
        }
        return VMStatus::INF_LOOP;
    }

    // stack manipulation
    bool empty() { return stack.size() <= 1; }
    void push(Value val) { stack.push_back(val); }
    Value pop() {
        assert(stack.size());
        Value tos = stack.back();
        stack.pop_back();
        return tos;
    }
    Value& tos() { return stack.back(); }

    ////////////////////////////////////////////////////////////////////
    Chunk code;
    std::vector<OpCode>::const_iterator ip;
    std::vector<Value> stack;
};

// use to hand test code sequences
void dumpCode() {
    Chunk code;
    // (20-10) * 4 * 4 / 40
    code.addConstNum(1);
    code.addConstNum(4);
    code.addConstNum(40);
    code.addConstNum(4);
    code.addConstNum(4);
    code.addConstNum(20);
    code.addConstNum(-10);
    code.addOp(OP_NEG);
    code.addOp(OP_SUB);
    code.addOp(OP_MULT);
    code.addOp(OP_MULT);
    code.addOp(OP_DIV);
    code.addOp(OP_SUB);
    code.addOp(OP_OR);
    code.addOp(OP_PRINT);
    code.addOp(OP_RET);
    code.finalize();
    code.list();

    VM vm(code);

    vm.run();
}
