#pragma once

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctype.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cfg.hpp"
#include "color.hpp"
#include "err.hpp"
#include "re.hpp"
#include "scan.hpp"

/*
    parse functions fit into these categories:
        prefix
        infix

    infix and prefix  can exist for the same token type; this allows
    differentiating between, for example, '(' as a grouping operator
    (prefix) and as a call operator (infix)

    func    (       arg1, arg2)
     ^      ^       ^
     LHS    infix   RHS

    pratt parser core loop works like so:

    parse(prior_prec){
        tok = consume()
        prefix_func = get_prefix_function(tok)

        // fully parse expr starting at tok, recursively if needed
        expr = prefix_func(tok)

        // THE CORE TRICK
        // ---------------
        // if prior_prec >= next_prec that means that we should just return
        // immediately. The enclosing context binds stronger than the next
        // token and we should complete parsing the prior expr.
        //
        // if prior_prec < next_prec, the current expr *belongs* to the
        // expression that follows
        //
        next_prec = get_next_precedence()
        if (prior_prec < next_prec){
            return expr;
        }
        else {
            while (prior_precedence < getPredence(nexttok)){
                // next token must have a valid infix op defined, or prec would be 0!
                tok = consume()
                infix_func = get_infix_function(tok)

                // prev expr is subsumed within infix_func
                // this happens repeatedly until precedence stops it
                lhs = expr
                expr = infix_func(tok,lhs)
            }
            return expr
        }
    }

*/

// fwd decl
struct Parser;

struct Expr {
    void print(int depth = 0, bool semicolon = false) { printf("%s%s\n", str().c_str(), semicolon ? ";" : ""); }
    // pretty print expression at requested indentation
    virtual std::string str(int depth = 0) { return "(UNIMPLEMENTED)"; }
    virtual bool isNameExpr() { return false; }
    virtual ~Expr();
    std::string tabs(int depth) {
        std::string tabs;
        for (int i = 0; i < depth; i++)
            tabs.append("  ");
        return tabs;
    }
};
Expr::~Expr() {}

/////////////////////////////////////////////////////////////////////////
// Expression Types
/////////////////////////////////////////////////////////////////////////

struct EmptyExpr : Expr {
    std::string str(int depth) { return tabs(depth); }
};
struct NameExpr : Expr {
    NameExpr(std::string name) : name(std::move(name)) {}
    bool isNameExpr() { return true; }
    std::string str(int depth) { return name; }
    std::string name;
};
struct NumExpr : Expr {
    NumExpr(double num) : num(num) {}
    std::string str(int depth) {
        char buf[200];
        sprintf(buf, "%g", num);
        return tabs(depth) + buf;
    }
    double num;
};
struct UnaryOpExpr : Expr {
    UnaryOpExpr(TokenType type, Expr* right) : type(type), right(right) {}
    std::string str(int depth) { return token_to_repr[type] + right->str(); }
    TokenType type;
    Expr* right;
};
struct BinaryOpExpr : Expr {
    BinaryOpExpr() = delete;
    BinaryOpExpr(Expr* left, TokenType type, Expr* right) : left(left), type(type), right(right) {}
    std::string str(int depth) { return tabs(depth) + "(" + left->str() + " " + token_to_repr[type] + " " + right->str() + ")" ; }
    Expr* left;
    TokenType type;
    Expr* right;
};

struct CallExpr : Expr {
    CallExpr() = delete;
    CallExpr(NameExpr* fn_name, Expr* args) : fn_name(fn_name), args(args) {}
    std::string str(int depth) { return tabs(depth) + fn_name->name + "(" + args->str() + ")"; }
    NameExpr* fn_name;
    Expr* args;
};

struct ReturnExpr : Expr {
    ReturnExpr() = delete;
    ReturnExpr(Expr* value) : value(value) {}
    std::string str(int depth) { return tabs(depth) + "ret " + value->str(); }
    Expr* value;
};

struct SubscriptExpr : Expr {
    SubscriptExpr() = delete;
    SubscriptExpr(Expr* array_name, Expr* index) : array_name(array_name), index(index) {}
    std::string str(int depth) { return tabs(depth) + array_name->str() + "[" + index->str() + "]"; }
    Expr* array_name;
    Expr* index;
};

struct CommaListExpr : Expr {
    CommaListExpr() = delete;
    CommaListExpr(std::vector<Expr*> exprs) : exprs(exprs) {}
    std::string str(int depth) {
        std::string str = tabs(depth);
        for (auto expr = exprs.begin(); expr != exprs.end(); expr++) {
            str += (*expr)->str();
            if (std::next(expr) != exprs.end())
                str += ", ";
        }
        return str;
    }
    std::vector<Expr*> exprs;
};

struct BlockExpr : Expr {
    BlockExpr() = delete;
    BlockExpr(std::vector<Expr*> stmts) : stmts(stmts) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + "{";
        std::string joinstr = "\n"; // depth >= 1 ? "\n" : " ";
        for (const auto expr : stmts) {
            if (expr->str(depth) != "") {
                str += joinstr + expr->str(depth + 1) + ";";
            }
        }
        return str + joinstr + tabs(depth) + "}";
    }
    std::vector<Expr*> stmts;
};

struct ForExpr : Expr {
    ForExpr() = delete;
    ForExpr(Expr* loop_var, Expr* range_expr, Expr* loop_body)
        : loop_var(loop_var), range_expr(range_expr), loop_body(loop_body) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + "for ";
        str += loop_var->str();
        str += " : " + range_expr->str() + "\n";
        str += loop_body->str(depth + 1);
        return str;
    }

    Expr* loop_var;
    Expr* range_expr;
    Expr* loop_body;
};

struct FnDefExpr : Expr {
    FnDefExpr() = delete;
    FnDefExpr(NameExpr* fn_name, Expr* args, Expr* body) : fn_name(fn_name), args(args), body(body) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + "fn ";
        str += fn_name->str(0);
        str += "(" + args->str() + ")\n";
        str += body->str(depth + 1);
        return str;
    }

    NameExpr* fn_name;
    Expr* args;
    Expr* body;
};

// indexed by token type
typedef int Prec;
typedef std::vector<Token>::const_iterator TokIter;
typedef std::function<Expr*(Parser&)> PrefixFn;
typedef std::vector<std::pair<PrefixFn, Prec>> PrefixTable;
typedef std::function<Expr*(Parser&, Expr* left)> InfixFn;
typedef std::vector<std::pair<InfixFn, Prec>> InfixTable;

struct Parser {
    Parser() = delete;
    Parser(const std::vector<Token> tokens) : _tokens(std::move(tokens)) {
        tokit = _tokens.begin();

        initPrefixTable(_prefix_func_table);
        _prefix_func_table[LEFT_BRACE] = std::make_pair(&Parser::parseBlock, 1);
        _prefix_func_table[LEFT_PAREN] = std::make_pair(&Parser::parseGrouping, 1);
        _prefix_func_table[RET] = std::make_pair(&Parser::parseReturn, 1);
        _prefix_func_table[ID] = std::make_pair(&Parser::parseID, 5);
        _prefix_func_table[NUM] = std::make_pair(&Parser::parseNum, 5);
        _prefix_func_table[BANG] = std::make_pair(&Parser::parseUnaryOp, 30);
        _prefix_func_table[FOR] = std::make_pair(&Parser::parseFor, 100);
        _prefix_func_table[FN] = std::make_pair(&Parser::parseFnDef, 100);

        initInfixTable(_infix_func_table);
        _infix_func_table[EQUALS] = std::make_pair(&Parser::parseBinaryOp, 10);
        _infix_func_table[COMMA] = std::make_pair(&Parser::parseCommaList, 20);
        _infix_func_table[COLON] = std::make_pair(&Parser::parseBinaryOp, 22);
        _infix_func_table[TO] = std::make_pair(&Parser::parseBinaryOp, 23);
        _infix_func_table[CMP] = std::make_pair(&Parser::parseBinaryOp, 24);
        _infix_func_table[OR] = std::make_pair(&Parser::parseBinaryOp, 25);
        _infix_func_table[AND] = std::make_pair(&Parser::parseBinaryOp, 26);
        _infix_func_table[PLUS] = std::make_pair(&Parser::parseBinaryOp, 30);
        _infix_func_table[MINUS] = std::make_pair(&Parser::parseBinaryOp, 30);
        _infix_func_table[DIV] = std::make_pair(&Parser::parseBinaryOp, 40);
        _infix_func_table[MULT] = std::make_pair(&Parser::parseBinaryOp, 40);
        _infix_func_table[BANG] = std::make_pair(&Parser::parseBinaryOp, 80);
        _infix_func_table[LEFT_PAREN] = std::make_pair(&Parser::parseCall, 100);
        _infix_func_table[LEFT_BRACKET] = std::make_pair(&Parser::parseSubscript, 100);
    }

    // core Pratt parsing routine
    Expr* ParseExpr(int precedence = 0) {
        if (endoftokens())
            return new EmptyExpr;
        auto prefix_tok = *tokit;
        auto token_pos = getTokenPos();
        if (parseVerbose)
            printf("CALL prefix %s:%d\n", tokit->str.c_str(), token_pos);
        Expr* expr = getPrefixFunc(tokit->type)(*this);

        while (precedence < getInfixPrecedence()) {
            if (parseVerbose)
                printf("CALL infix %s:%d\n", tokit->str.c_str(), getTokenPos());
            expr = getInfixFunc(tokit->type)(*this, expr);
        }
        if (parseVerbose)
            printf("END prefix %s:%d\n", prefix_tok.str.c_str(), token_pos);

        return expr;
    }

    std::vector<Expr*> ParseStatements(int precedence = 0) {
        std::vector<Expr*> statements;
        while (not endoftokens() and precedence < getPrefixPrecedence()) {
            // printf("\nparsing statement at %s on LINE %d POS %d\n",
            //       token_to_typestr[tokit->type],
            //       tokit->lineno,
            //       tokit->linepos);

            auto expr = ParseExpr();
            statements.push_back(expr);

            // detect erorrs with statement termination
            if (endoftokens() and lasttype() != RIGHT_BRACE) {
                fprintf(stderr, RED "Hit EOF without finding statement terminator (; or }) \n" RESET);
                exit(1);
            } else if (currtype() != SEMICOLON and lasttype() != RIGHT_BRACE) {
                fprintf(stderr,
                        RED "Expected stmt terminator *before* token on line %d, pos %d\n" RESET,
                        currtoken().lineno,
                        currtoken().linepos);
                exit(1);
            } else if (currtype() == SEMICOLON) {
                consume(); // get rid of semicolon
            } else if (lasttype() == RIGHT_BRACE) {
                if (parseVerbose)
                    fprintf(stderr, RED "accepting right brace as closing statement \n" RESET);
                // accept a right brace as implictly terminating statement
            }
            if (not endoftokens() and parseVerbose)
                fprintf(stderr, GREEN "token starting next stmt is '%s'\n" RESET, currtoken().str.c_str());
        }
        return statements;
    }

    // prefix functions
    // NOTE: parsing functions must consume what they use!
    static NameExpr* parseID(Parser& parser) { return new NameExpr((parser.consume()).str); }
    static NumExpr* parseNum(Parser& parser) { return new NumExpr(atof(parser.consume().str.c_str())); }
    static UnaryOpExpr* parseUnaryOp(Parser& parser) {
        TokenType type = parser.consume().type;
        auto right = parser.ParseExpr(parser.getPrefixPrec(type));
        return new UnaryOpExpr(type, right);
    }
    static Expr* parseGrouping(Parser& parser) {
        parser.consume(); // consume left paren
        if (parser.currtype() == RIGHT_PAREN) {
            parser.consume();
            return new EmptyExpr;
        }
        auto expr = parser.ParseExpr(parser.getPrefixPrec(LEFT_PAREN));
        TokenType right_paren = parser.consume().type; // consume right paren
        assert(right_paren && "expected paren when parsing call expr");
        return expr;
    }
    static ReturnExpr* parseReturn(Parser& parser) {
        parser.consume();
        auto expr = parser.ParseExpr(parser.getPrefixPrec(RET));
        return new ReturnExpr(expr);
    }
    static BlockExpr* parseBlock(Parser& parser) {
        parser.consume(); // consume brace
        std::vector<Expr*> statements;
        if (parser.currtype() == RIGHT_BRACE) {
            // handle empty block
            statements = {new EmptyExpr};
        } else {
            if (parseVerbose)
                printf(MAGENTA "start parsing block\n" RESET);
            statements = parser.ParseStatements();
            if (parseVerbose)
                printf(MAGENTA "done parsing block\n" RESET);
        }
        auto last_token_type = parser.consume().type; // consume brace
        assert(last_token_type == RIGHT_BRACE && "expected closing right-brace when parsing block expr");
        return new BlockExpr(statements);
    }
    static ForExpr* parseFor(Parser& parser) {
        parser.consume();
        // parse id
        assert(parser.currtype() == ID);
        auto loop_var = new NameExpr(parser.consume().str);
        // parse colon
        assert(parser.currtype() == COLON);
        parser.consume();

        auto range_expr = parser.ParseExpr(0);
        auto loop_body = parser.ParseExpr(0);

        return new ForExpr(loop_var, range_expr, loop_body);
    }

    // infix functions
    // NOTE: parsing functions must consume what they use!
    static BinaryOpExpr* parseBinaryOp(Parser& parser, Expr* left) {
        TokenType type = parser.consume().type;
        // get right hand side
        auto right = parser.ParseExpr(parser.getInfixPrec(type));
        return new BinaryOpExpr(left, type, right);
    }

    static CallExpr* parseCall(Parser& parser, Expr* left) {
        assert(left->isNameExpr());
        auto fn_name = dynamic_cast<NameExpr*>(left);
        parser.consume(); // consume paren
        Expr* args;
        if (parser.currtype() != RIGHT_PAREN) {
            // use low precedence for rhs; call should bind tightly on LHS but
            // weakly on RHS
            args = parser.ParseExpr(0);
        } else {
            args = new EmptyExpr;
        }
        TokenType right_paren = parser.consume().type; // consume paren
        assert(right_paren && "expected paren when parsing call expr");
        return new CallExpr(fn_name, args);
    }

    static FnDefExpr* parseFnDef(Parser& parser) {
        // FORM : fn id (args) {blockexpr}

        // parse fn
        assert(parser.currtype() == FN);
        parser.consume();

        // parse id
        assert(parser.currtype() == ID);
        auto fn_name = new NameExpr(parser.currtoken().str);
        parser.consume();

        // parse LEFT_PAREN
        assert(parser.consume().type == LEFT_PAREN);

        // parse args
        Expr* args;
        if (parser.currtype() != RIGHT_PAREN) {
            // use low precedence for rhs; call should bind tightly on LHS but
            // weakly on RHS
            args = parser.ParseExpr(0);
        } else {
            args = new EmptyExpr;
        }

        // parse RIGHT_PAREN
        assert(parser.consume().type && "expected right-paren when parsing FnDefExpr");

        // parse body
        assert(parser.currtype() && "expected BlockExpr starting with '{' as body of function");
        Expr* body = parser.ParseExpr(0);

        return new FnDefExpr(fn_name, args, body);
    }

    static SubscriptExpr* parseSubscript(Parser& parser, Expr* array_name) {
        parser.consume(); // consume bracket
        Expr* index_expr;
        if (parser.currtype() != RIGHT_BRACKET) {
            // use low precedence for rhs; call should bind tightly on LHS but
            // weakly on RHS
            index_expr = parser.ParseExpr(0);
        } else {
            assert(0 && "expected expression for array subscript operator");
        }
        TokenType right_bracket = parser.consume().type; // consume bracket
        assert(right_bracket && "expected closing right-bracket when parsing subscript expr");
        return new SubscriptExpr(array_name, index_expr);
    }

    static CommaListExpr* parseCommaList(Parser& parser, Expr* first_elem) {
        parser.consume(); // consume comma
        std::vector<Expr*> list_elems = {first_elem};
        list_elems.push_back(parser.ParseExpr(parser.getInfixPrec(COMMA)));

        // add items to list as long as they are available
        while (parser.currtype() == COMMA) {
            parser.consume(); // consume comma
            list_elems.push_back(parser.ParseExpr(0));
        }

        return new CommaListExpr(list_elems);
    }

    static Expr* prefixboom(Parser& parser) {
        fprintf(stderr,
                RED "prefixFunc for token type %s unimplemented.\n" RESET,
                token_to_typestr[parser.tokit->type]);
        exit(1);
        return new Expr();
    }
    static Expr* infixboom(Parser& parser, Expr*) {
        fprintf(stderr, RED "infixFunc for token type %s unimplemented.\n" RESET, token_to_typestr[parser.tokit->type]);
        exit(1);
        return new Expr();
    }

    // Helper functions
    int getTokenPos() { return tokit - _tokens.begin(); }

    Prec getInfixPrecedence() { return endoftokens() ? -9999 : getInfixPrec(tokit->type); }
    Prec getPrefixPrecedence() { return endoftokens() ? -9999 : getPrefixPrec(tokit->type); }

    // initialize function tables
    void initPrefixTable(PrefixTable& functable) {
        functable.resize(NUM_TOKEN_TYPES);
        for (int i = 0; i < NUM_TOKEN_TYPES; i++) {
            _prefix_func_table[i] = std::make_pair(&prefixboom, -100);
        }
    }
    void initInfixTable(InfixTable& functable) {
        functable.resize(NUM_TOKEN_TYPES);
        std::fill(functable.begin(), functable.end(), std::make_pair(&infixboom, -100));
    }

    // accessors for prefix and infix function tables
    PrefixFn getPrefixFunc(TokenType toktype) {
        assert(toktype < _prefix_func_table.size());
        return _prefix_func_table[toktype].first;
    }
    InfixFn getInfixFunc(TokenType toktype) {
        assert(toktype < _infix_func_table.size());
        return _infix_func_table[toktype].first;
    }
    Prec getPrefixPrec(TokenType toktype) {
        assert(toktype < _prefix_func_table.size());
        return _prefix_func_table[toktype].second;
    }
    Prec getInfixPrec(TokenType toktype) {
        assert(toktype < _infix_func_table.size());
        return _infix_func_table[toktype].second;
    }

    // token stream manipulation
    const Token& consume() { return *(tokit++); };
    const Token& currtoken() { return *tokit; };
    TokenType currtype() { return endoftokens() ? NONE : tokit->type; };
    TokenType lasttype() { return std::prev(tokit)->type; };
    bool endoftokens() { return tokit == _tokens.end(); };

    // variables
    PrefixTable _prefix_func_table;
    InfixTable _infix_func_table;
    std::vector<Token> _tokens;
    TokIter tokit;
};
