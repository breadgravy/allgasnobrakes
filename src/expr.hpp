#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "scan.hpp"
#include "util.hpp"
#include "vm.hpp"

const std::unordered_map<TokenType, OpCode> token_to_binop = {{PLUS, OP_ADD},
                                                              {MINUS, OP_SUB},
                                                              {MULT, OP_MULT},
                                                              {DIV, OP_DIV},
                                                              {OR, OP_OR},
                                                              {AND, OP_AND},
                                                              {CMP, OP_CMP}};

const std::unordered_map<TokenType, OpCode> token_to_unaryop = {
                                                              {MINUS, OP_NEG},
                                                              {BANG,  OP_NOT}};

/////////////////////////////////////////////////////////////////////////
// Expression Types
/////////////////////////////////////////////////////////////////////////

// fwd decl
struct Parser;

#define DEL_EXPR(expr)                                                                             \
    if (expr != nullptr)                                                                           \
        delete expr;

// Base Class
struct Expr {
    void print(int depth = 0, bool semicolon = false) {
        printf("%s%s\n\n", str().c_str(), semicolon ? ";" : "");
    }
    // pretty print expression at requested indentation
    virtual std::string str(int depth = 0) { return "(UNIMPLEMENTED)"; }
    virtual bool isNameExpr() { return false; }
    virtual void codegen(Chunk& code) { ERR("codegen for expr \n'%s' is UNIMPLEMENTED.\n",str(0).c_str()); }
    virtual ~Expr();
    std::string tabs(int depth) {
        std::string tabs;
        for (int i = 0; i < depth; i++)
            tabs.append("    ");
        return tabs;
    }
};
Expr::~Expr() {}

struct EmptyExpr : Expr {
    std::string str(int depth) { return tabs(depth) + "(EMPTY)"; }
};
struct NameExpr : Expr {
    NameExpr(std::string name) : name(std::move(name)) {}
    bool isNameExpr() { return true; }
    std::string str(int depth) { return name; }
    ~NameExpr() {}

    std::string name;
};
struct NumExpr : Expr {
    NumExpr(double num) : num(num) {}
    void codegen(Chunk& code) { code.addConstNum(num); }
    std::string str(int depth) {
        char buf[200];
        sprintf(buf, "%g", num);
        return tabs(depth) + buf;
    }
    ~NumExpr() {}

    double num;
};
struct BoolExpr : Expr {
    BoolExpr(bool val) : val(val) {}
    void codegen(Chunk& code) { code.addConstBool(val); }
    std::string str(int depth) {
        char buf[200];
        sprintf(buf, "%s", val ? "True" : "False");
        return tabs(depth) + buf;
    }
    ~BoolExpr() {}

    bool val;
};
struct UnaryOpExpr : Expr {
    UnaryOpExpr(TokenType type, Expr* right) : type(type), right(right) {
        assert(not right->isNameExpr());
    }
    void codegen(Chunk& code) {
        right->codegen(code);
        if (token_to_unaryop.count(type)) {
            code.addOp(token_to_unaryop.at(type));
        } else {
            ERR("codegen: tokentype '%s' in expr '%s' not implemented as unary op",
                token_to_typestr[type],
                str(0).c_str());
        }
    }
    std::string str(int depth) { return "(" + std::string(token_to_repr[type]) + right->str() + ")"; }
    ~UnaryOpExpr() { DEL_EXPR(right); }

    TokenType type;
    Expr* right;
};
struct BinaryOpExpr : Expr {
    BinaryOpExpr() = delete;
    BinaryOpExpr(Expr* left, TokenType type, Expr* right) : left(left), type(type), right(right) {}
    std::string str(int depth) {
        return tabs(depth) + "(" + left->str() + " " + token_to_repr[type] + " " + right->str() +
               ")";
    }
    void codegen(Chunk& code) {
        left->codegen(code);
        right->codegen(code);
        if (token_to_binop.count(type)) {
            code.addOp(token_to_binop.at(type));
        } else {
            ERR("codegen: tokentype '%s' in expr '%s' not implemented as binary op",
                token_to_typestr[type],
                str(0).c_str());
        }
    }
    ~BinaryOpExpr() {
        DEL_EXPR(left);
        DEL_EXPR(right);
    }

    Expr* left;
    TokenType type;
    Expr* right;
};

struct CallExpr : Expr {
    CallExpr() = delete;
    CallExpr(NameExpr* fn_name, Expr* args) : fn_name(fn_name), args(args) {}
    std::string str(int depth) {
        return tabs(depth) + BLUE + fn_name->name + RESET + "(" + args->str() + ")";
    }
    ~CallExpr() {
        DEL_EXPR(fn_name);
        DEL_EXPR(args);
    }

    NameExpr* fn_name;
    Expr* args;
};

struct ReturnExpr : Expr {
    ReturnExpr() = delete;
    ReturnExpr(Expr* value) : value(value) {}
    void codegen(Chunk& code) { code.addOp(OP_RET); }
    std::string str(int depth) { return tabs(depth) + BRIGHTMAGENTA "ret " RESET + value->str(); }
    ~ReturnExpr() { DEL_EXPR(value); }

    Expr* value;
};

struct VarExpr : Expr {
    VarExpr() = delete;
    VarExpr(Expr* expr) : expr(expr) {}
    std::string str(int depth) { return tabs(depth) + BRIGHTMAGENTA "var " RESET + expr->str(); }
    ~VarExpr() { DEL_EXPR(expr); }

    Expr* expr;
};

struct SubscriptExpr : Expr {
    SubscriptExpr() = delete;
    SubscriptExpr(Expr* array_name, Expr* index) : array_name(array_name), index(index) {}
    std::string str(int depth) {
        return tabs(depth) + array_name->str() + "[" + index->str() + "]";
    }
    ~SubscriptExpr() {
        DEL_EXPR(index);
        DEL_EXPR(array_name);
    }

    Expr* array_name;
    Expr* index;
};

struct CommaListExpr : Expr {
    CommaListExpr() = delete;
    CommaListExpr(std::vector<Expr*> exprs) : exprs(exprs) {}
    void codegen(Chunk& code) { ERR("CommaExpr '%s' has no codegen.",str(0).c_str()); }
    std::string str(int depth) {
        std::string str = tabs(depth);
        for (auto expr = exprs.begin(); expr != exprs.end(); expr++) {
            str += (*expr)->str();
            if (std::next(expr) != exprs.end())
                str += ", ";
        }
        return str;
    }
    ~CommaListExpr() {
        for (auto expr : exprs)
            DEL_EXPR(expr);
    }

    std::vector<Expr*> exprs;
};

struct BlockExpr : Expr {
    BlockExpr() = delete;
    BlockExpr(std::vector<Expr*> stmts) : stmts(stmts) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + "{";
        std::string joinstr = "\n";
        if (not stmts.size()) {
            return str + "}";
        }
        for (const auto expr : stmts) {
            if (expr->str(depth) != "") {
                str += joinstr + expr->str(depth + 1) + ";";
            }
        }
        return str + joinstr + tabs(depth) + "}";
    }
    ~BlockExpr() {
        for (auto expr : stmts)
            DEL_EXPR(expr);
    }

    std::vector<Expr*> stmts;
};

struct ForExpr : Expr {
    ForExpr() = delete;
    ForExpr(Expr* loop_var, Expr* range_expr, Expr* loop_body)
        : loop_var(loop_var), range_expr(range_expr), loop_body(loop_body) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + BRIGHTMAGENTA "for " RESET;
        str += loop_var->str();
        str += " : " + range_expr->str() + "\n";
        str += loop_body->str(depth);
        return str;
    }
    ~ForExpr() {
        DEL_EXPR(loop_var);
        DEL_EXPR(range_expr);
        DEL_EXPR(loop_body);
    }

    Expr* loop_var;
    Expr* range_expr;
    Expr* loop_body;
};

struct FnDefExpr : Expr {
    FnDefExpr() = delete;
    FnDefExpr(NameExpr* fn_name, Expr* args, Expr* body)
        : fn_name(fn_name), args(args), body(body) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + BRIGHTMAGENTA "fn " RESET;
        str += YELLOW + fn_name->str(0) + RESET;
        str += "(" + args->str() + ")\n";
        str += body->str(depth);
        return str;
    }
    ~FnDefExpr() {
        DEL_EXPR(fn_name);
        DEL_EXPR(args);
        DEL_EXPR(body);
    }

    NameExpr* fn_name;
    Expr* args;
    Expr* body;
};

struct IfExpr : Expr {
    IfExpr() = delete;
    IfExpr(bool hasElse, Expr* if_cond, Expr* if_body, Expr* else_body)
        : has_else(hasElse), if_cond(if_cond), if_body(if_body), else_body(else_body) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + BRIGHTMAGENTA "if " RESET;
        str += if_cond->str() + "\n";
        str += if_body->str(depth);
        if (has_else) {
            str += "\n" + tabs(depth) + "else\n" + else_body->str(depth);
        }
        return str;
    }
    ~IfExpr() {
        DEL_EXPR(if_cond);
        DEL_EXPR(if_body);
        DEL_EXPR(else_body);
    }

    bool has_else;
    Expr* if_cond;
    Expr* if_body;
    Expr* else_body;
};

struct PrintExpr : Expr {
    PrintExpr() = delete;
    PrintExpr(Expr* value) : value(value) {}
    void codegen(Chunk& code) { value->codegen(code); code.addOp(OP_PRINT); }
    std::string str(int depth) { return tabs(depth) + YELLOW "print " RESET + value->str(); }
    ~PrintExpr() { delete value; }

    Expr* value;
};
