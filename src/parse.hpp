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
#include "expr.hpp"
#include "time.hpp"

// indexed by token type
typedef int Prec;
static constexpr Prec PREC_NONE = -99999; // indicates prefix/infix not done 

typedef std::vector<Token>::const_iterator TokIter;
typedef std::function<Expr*(Parser&)> PrefixFn;
typedef std::vector<std::pair<PrefixFn, Prec>> PrefixTable;
typedef std::function<Expr*(Parser&, Expr* left)> InfixFn;
typedef std::vector<std::pair<InfixFn, Prec>> InfixTable;

struct Parser {
    Parser() = delete;
    Parser(const std::vector<Token>& tokens) : tokens(tokens), tokit(tokens.begin()) {

        initPrefixTable(prefix_func_table);
        prefix_func_table[LEFT_BRACE] = std::make_pair(&Parser::parseBlock, 1);
        prefix_func_table[LEFT_PAREN] = std::make_pair(&Parser::parseGrouping, 1);
        prefix_func_table[RET] = std::make_pair(&Parser::parseReturn, 1);
        prefix_func_table[ID] = std::make_pair(&Parser::parseID, 5);
        prefix_func_table[NUM] = std::make_pair(&Parser::parseNum, 5);
        prefix_func_table[TRUE] = std::make_pair(&Parser::parseBool, 5);
        prefix_func_table[FALSE] = std::make_pair(&Parser::parseBool, 5);
        prefix_func_table[BANG] = std::make_pair(&Parser::parseUnaryOp, 100);
        prefix_func_table[MINUS] = std::make_pair(&Parser::parseUnaryOp, 100);
        prefix_func_table[FOR] = std::make_pair(&Parser::parseFor, 100);
        prefix_func_table[FN] = std::make_pair(&Parser::parseFnDef, 100);
        prefix_func_table[IF] = std::make_pair(&Parser::parseIf, 100);
        prefix_func_table[VAR] = std::make_pair(&Parser::parseVar, 100);
        prefix_func_table[PRINT] = std::make_pair(&Parser::parsePrint, 100);

        initInfixTable(infix_func_table);
        infix_func_table[EQUALS] = std::make_pair(&Parser::parseBinaryOp, 10);
        infix_func_table[COMMA] = std::make_pair(&Parser::parseCommaList, 20);
        infix_func_table[COLON] = std::make_pair(&Parser::parseBinaryOp, 22);
        infix_func_table[TO] = std::make_pair(&Parser::parseBinaryOp, 23);
        infix_func_table[CMP] = std::make_pair(&Parser::parseBinaryOp, 24);
        infix_func_table[OR] = std::make_pair(&Parser::parseBinaryOp, 25);
        infix_func_table[AND] = std::make_pair(&Parser::parseBinaryOp, 26);
        infix_func_table[PLUS] = std::make_pair(&Parser::parseBinaryOp, 30);
        infix_func_table[MINUS] = std::make_pair(&Parser::parseBinaryOp, 30);
        infix_func_table[DIV] = std::make_pair(&Parser::parseBinaryOp, 40);
        infix_func_table[MULT] = std::make_pair(&Parser::parseBinaryOp, 40);
        infix_func_table[BANG] = std::make_pair(&Parser::parseBinaryOp, 80);
        infix_func_table[LEFT_PAREN] = std::make_pair(&Parser::parseCall, 100);
        infix_func_table[LEFT_BRACKET] = std::make_pair(&Parser::parseSubscript, 100);
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

        printf("Finding infix expr wih precedence > %d\n", precedence);
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
                fprintf(stderr,
                        RED "Hit EOF without finding statement terminator (; or }) \n" RESET);
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
                fprintf(stderr,
                        GREEN "token starting next stmt is '%s'\n" RESET,
                        currtoken().str.c_str());
        }
        return statements;
    }

    ///////////////////////////////////////////////////////////////////////////
    // prefix functions
    // NOTE: parsing functions must consume what they use!
    static NameExpr* parseID(Parser& parser) {
        assert(parser.currtype() == ID);
        return new NameExpr((parser.consume()).str);
    }
    static NumExpr* parseNum(Parser& parser) {
        assert(parser.currtype() == NUM);
        return new NumExpr(atof(parser.consume().str.c_str()));
    }
    static BoolExpr* parseBool(Parser& parser) {
        assert(parser.currtype() == TRUE or parser.currtype() == FALSE);
        return new BoolExpr(parser.consume().type == TRUE);
    }
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
    static PrintExpr* parsePrint(Parser& parser) {
        parser.consume();
        return new PrintExpr(parser.ParseExpr(0));
    }
    static ReturnExpr* parseReturn(Parser& parser) {
        parser.consume();
        auto expr = parser.ParseExpr(parser.getPrefixPrec(RET));
        return new ReturnExpr(expr);
    }
    static VarExpr* parseVar(Parser& parser) {
        parser.consume();
        auto expr = parser.ParseExpr(0);
        return new VarExpr(expr);
    }
    static BlockExpr* parseBlock(Parser& parser) {
        parser.consume(); // consume brace
        std::vector<Expr*> statements;
        if (parser.currtype() == RIGHT_BRACE) {
            // handle empty block
            statements = {};
        } else {
            if (parseVerbose)
                printf(MAGENTA "start parsing block\n" RESET);
            statements = parser.ParseStatements();
            if (parseVerbose)
                printf(MAGENTA "done parsing block\n" RESET);
        }
        auto last_token_type = parser.consume().type; // consume brace
        assert(last_token_type == RIGHT_BRACE &&
               "expected closing right-brace when parsing block expr");
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
    static IfExpr* parseIf(Parser& parser) {

        // consume IF
        assert(parser.currtype() == IF);
        parser.consume();

        // get if cond
        auto if_cond = parser.ParseExpr(0);
        auto if_body = parseBlock(parser);

        // parse else clause if it exists
        bool has_else = false;
        Expr* else_body = new EmptyExpr();
        if (parser.currtype() == ELSE) {
            // consume else
            parser.consume();
            // expect this to be a BlockExpr
            else_body = parseBlock(parser);
            has_else = true;
        }

        return new IfExpr(has_else, if_cond, if_body, else_body);
    }

    ///////////////////////////////////////////////////////////////////////////
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
        fprintf(stderr,
                RED "infixFunc for token type %s unimplemented.\n" RESET,
                token_to_typestr[parser.tokit->type]);
        exit(1);
        return new Expr();
    }

    // Helper functions
    int getTokenPos() { return tokit - tokens.begin(); }

    Prec getInfixPrecedence() { return endoftokens() ? PREC_NONE : getInfixPrec(tokit->type); }
    Prec getPrefixPrecedence() { return endoftokens() ? PREC_NONE : getPrefixPrec(tokit->type); }

    // initialize function tables
    void initPrefixTable(PrefixTable& functable) {
        functable.resize(NUM_TOKEN_TYPES);
        for (int i = 0; i < NUM_TOKEN_TYPES; i++) {
            prefix_func_table[i] = std::make_pair(&prefixboom, PREC_NONE);
        }
    }
    void initInfixTable(InfixTable& functable) {
        functable.resize(NUM_TOKEN_TYPES);
        std::fill(functable.begin(), functable.end(), std::make_pair(&infixboom, PREC_NONE));
    }

    // accessors for prefix and infix function tables
    PrefixFn getPrefixFunc(TokenType toktype) {
        assert(toktype < prefix_func_table.size());
        return prefix_func_table[toktype].first;
    }
    InfixFn getInfixFunc(TokenType toktype) {
        assert(toktype < infix_func_table.size());
        return infix_func_table[toktype].first;
    }
    Prec getPrefixPrec(TokenType toktype) {
        assert(toktype < prefix_func_table.size());
        return prefix_func_table[toktype].second;
    }
    Prec getInfixPrec(TokenType toktype) {
        assert(toktype < infix_func_table.size());
        return infix_func_table[toktype].second;
    }

    // token stream manipulation
    const Token& consume() { return *(tokit++); };
    const Token& currtoken() { return *tokit; };
    TokenType currtype() { return endoftokens() ? NONE : tokit->type; };
    TokenType lasttype() { return std::prev(tokit)->type; };
    bool endoftokens() { return tokit == tokens.end(); };

    // variables
    PrefixTable prefix_func_table;
    InfixTable infix_func_table;
    const std::vector<Token>& tokens;
    TokIter tokit;
};
