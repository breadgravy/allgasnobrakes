#include <cassert>
#include <cstdio>
#include <vector>

#include "cfg.hpp"
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
    Value() : tag(Tag::VAL_NULL), num(-1) {}
    Value(double val) {
        tag = Tag::VAL_NUM;
        num = val;
    }
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
    void addConstOp(double val, int lineno = -1) {
        auto idx = regConstVal(val);
        addOp(OP_CONST);
        // constant is Constidx stored directly in bytecode stream
        addOp(OpCode(idx));
    }
    ConstIdx regConstVal(double constant) {
        assert(constants.size() < 255);
        constants.push_back(Value(constant));
        return constants.size() - 1;
    }
    Value getConst(ConstIdx idx) { return constants.at(idx); }

    void finalize() {
        // chunk always ends in EOF token
        if (code.back() != OP_EOF) {
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
            printf(CYAN "%d" RESET ": %s (%d) \n", i, opcode_to_str[op], op);
            if (op == OP_CONST) {
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

#define DEBUG(...)                                                                                 \
    if (debug)                                                                                     \
        printf(__VA_ARGS__);

// note stack can be modified in place! 
#define UNARY_OP(__op__)                                                                           \
    {                                                                                              \
        tos().num = -tos().num;                                                                        \
    }

// NOTE: must pop b before a, as a will be pushed unto the stack first!
#define BINARY_OP(__op__)                                                                          \
    {                                                                                              \
        auto b = pop().num;                                                                        \
        auto a = pop().num;                                                                        \
        push(Value(a __op__ b));                                                                   \
    }

enum class VMStatus { OK, ERR, INF_LOOP };
struct VM {
    // returns pair of {op code , offset in bytecode chunk }
    std::pair<OpCode, int> readOp() { return {*ip++, ip - code.begin()}; }
    VM() { push(Value()); }
    VMStatus run() {
        ip = code.begin();
        constexpr int max_icount = 50;
        auto end = code.end();
        for (int icount = 0; icount < max_icount && ip != end; icount++) {
            auto op_pair = readOp();
            OpCode op = op_pair.first;
            int pos = op_pair.second;

            auto printOp = [this, op, pos]() {
                DEBUG("%3d: %-8s %-3g\n", pos, opcode_to_str[op], this->tos().num);
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
            if (debug_vmstack) {
                printf("\t\tSTACK \n\t\t{\n");
                for (int i = stack.size() - 1; i > 0; --i) {
                    printf("\t\t\t[%2d] %g\n", i, stack[i].num);
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

void dumpCode() {
    VM vm;
    auto& code = vm.code;
    // (20-10) * 4 * 4 / 40
    code.addConstOp(1);
    code.addConstOp(4);
    code.addConstOp(40);
    code.addConstOp(4);
    code.addConstOp(4);
    code.addConstOp(20);
    code.addConstOp(-10);
    code.addOp(OP_NEG);
    code.addOp(OP_SUB);
    code.addOp(OP_MULT);
    code.addOp(OP_MULT);
    code.addOp(OP_DIV);
    code.addOp(OP_SUB);
    code.addOp(OP_OR);
    code.addOp(OP_RET);
    code.finalize();

    code.list();

    printDiv("VM Start");
    auto printStatus = [](auto arg) { printf("\n--\nVm exited with status %s\n", arg); };
    switch (auto stat = vm.run()) {
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
        printf("\tUNKNOWN status is %d\n", stat);
        assert(0);
        break;
    }
}
