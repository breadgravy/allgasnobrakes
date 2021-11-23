#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cfg.hpp"
#include "re.hpp"

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

void printDiv(const char* str) {
    static int phaseno = 1;
    printf("\n-----------------------------------------------------------------"
           "---------------\n");
    printf(" Phase %d : %s\n", phaseno++, str);
    printf("-------------------------------------------------------------------"
           "-------------\n");
}

#define RED "\u001b[31m"
#define GREEN "\u001b[32m"
#define YELLOW "\u001b[33m"
#define BLUE "\u001b[34m"
#define MAGENTA "\u001b[35m"
#define CYAN "\u001b[36m"
#define WHITE "\u001b[37m"
#define RESET "\u001b[0m"

///////////////////////////////////////////////////////////////////////////////
#define DECL_TOKEN_TYPE(type) type,
enum TokenType {
#include "token_types.inc"
};
#undef DECL_TOKEN_TYPE

#define DECL_TOKEN_TYPE(type) #type,
const char* token_to_str[] = {
#include "token_types.inc"
};
#undef DECL_TOKEN_TYPE

struct Token {
    TokenType type = NUM_TOKEN_TYPES;
    const std::string str = "";
    int lineno = 0;
    int linepos = 0;
};

#define SINGLE_CHAR_TOKEN(__ch__, __token_type__)                                                                      \
    case __ch__: {                                                                                                     \
        tok(__token_type__, str(__ch__));                                                                              \
        break;                                                                                                         \
    }

#define KW_MATCH(__token_type__, __token_str__)                                                                        \
    else if (re_match(idstr, "^" #__token_str__ "$")) {                                                                \
        return __token_type__;                                                                                         \
    }

struct Scanner {
    Scanner() = delete;
    Scanner(const char* buf) : _srcbuf(buf), _sz(strlen(buf)) {}
    std::vector<Token> scan() {
        char ch = *_srcbuf;
        _lineno = 0;
        _linepos = 0;
        while (ch != '\0') {
            switch (ch) {
            case '#': {
                printf("Commented " CYAN);
                for (; ch != '\n' and ch != '\0'; ch = advance()) {
                    printf("%c", ch);
                }
                printf(RESET " on LINE %d.\n", _lineno);
                _lineno++;
                break;
            }
            case 'a' ... 'z':
            case 'A' ... 'Z':
            case '_': {
                // printf("%3ld: found alpha  char '%c'\n", ch);

                // TODO: implement keyword check
                // TokenType kw = parseKeyword();
                std::string idstr = consumeId();
                TokenType kw = getKeywordTokenType(idstr);
                tok(kw, idstr);
                break;
            }
            case '0' ... '9': {
                // printf("%3ld: found digit  char '%c'\n", ch);
                tok(NUM, consumeNum());
                break;
            }
            case '\n':
            case '\r':
            case '\t':
            case ' ': {
                if (ch == '\n') {
                    _lineno++;
                    _linepos = 0;
                }
                // printf("%3ld: found wspace char '%c'\n", i, ch);
                break;
            }
                SINGLE_CHAR_TOKEN('+', PLUS)
                SINGLE_CHAR_TOKEN('-', MINUS)
                SINGLE_CHAR_TOKEN('/', DIV)
                SINGLE_CHAR_TOKEN('*', MULT)
                SINGLE_CHAR_TOKEN('=', EQUALS)
                SINGLE_CHAR_TOKEN('!', BANG)
                SINGLE_CHAR_TOKEN(',', COMMA)
                SINGLE_CHAR_TOKEN(':', COLON)
                SINGLE_CHAR_TOKEN(';', SEMICOLON)
                SINGLE_CHAR_TOKEN('(', LEFT_PAREN)
                SINGLE_CHAR_TOKEN(')', RIGHT_PAREN)
                SINGLE_CHAR_TOKEN('{', LEFT_BRACE)
                SINGLE_CHAR_TOKEN('}', RIGHT_BRACE)
                SINGLE_CHAR_TOKEN('[', LEFT_BRACKET)
                SINGLE_CHAR_TOKEN(']', RIGHT_BRACKET)
            default: {
                printf("Found unimpl char '%c' (%d) at lineno %d, pos %d\n", ch, ch, _lineno, _linepos);
                exit(0);
                break;
            }
            }
            ch = advance();
        }

        if (dump_token_stream)
            dumpTokenStream();

        std::vector<Token> tmp;
        std::swap(tmp, _tokens);
        return tmp;
    }
    TokenType getKeywordTokenType(const std::string& idstr) {
        if (false) {
        }
        KW_MATCH(AND, and)
        KW_MATCH(ELIF, elif)
        KW_MATCH(ELSE, else)
        KW_MATCH(CMP, cmp)
        KW_MATCH(FN, fn)
        KW_MATCH(FOR,for)
        KW_MATCH(IF, if)
        KW_MATCH(OR, or)
        KW_MATCH(RET, ret)
        KW_MATCH(TO, to)
        else {
            return ID;
        }
    }
    std::string consumeId() {
        std::string s;
        char ch = *_srcbuf;
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
        char ch = *_srcbuf;
        assert(isdigit(ch));
        while (isdigit(ch)) {
            s.push_back(ch);
            ch = advance();
        }
        stepback();
        return s;
    }
    std::string str(char c) { return std::string(1, c); }
    void tok(TokenType type, std::string str) {
        _tokens.push_back({.type = type, .str = std::move(str), .lineno = _lineno, .linepos = _linepos});
    }
    char advance() {
        _linepos++;
        return *(++_srcbuf);
    }
    char stepback() {
        _linepos--;
        return *(--_srcbuf);
    }

    void dumpTokenStream() {
        int curr_lineno = -1;

        for (auto tok : _tokens) {
            if (tok.lineno > curr_lineno) {
                curr_lineno = tok.lineno;
                printf("LINE %d: \n", curr_lineno);
            }
            printf("\t%-12s = %-10s at %d,%d  \n", token_to_str[tok.type], tok.str.c_str(), tok.lineno, tok.linepos);
        }
    }

    std::vector<Token> _tokens;
    int _linepos = 0;
    int _lineno = 0;
    const char* _srcbuf = nullptr;
    const size_t _sz = 0;
};

#if 0

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

#endif

// fwd decl
struct Parser;

struct Expr {
    void print(int depth = 0) { printf("%s\n", str().c_str()); }
    virtual std::string str(int depth = 0) { return "Expr()"; }
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
    std::string str(int depth) { return "(EMPTY)"; }
};
struct NameExpr : Expr {
    NameExpr(std::string name) : name(std::move(name)) {}
    bool isNameExpr() { return true; }
    std::string str(int depth) { return tabs(depth) + "Name[\"" + name + "\"]"; }
    void print(int depth) { printf("%s\n", str(depth).c_str()); }
    std::string name;
};
struct NumExpr : Expr {
    NumExpr(double num) : num(num) {}
    std::string str(int depth) { return tabs(depth) + "Num[" + std::to_string(num) + "]"; }
    double num;
};
struct UnaryOpExpr : Expr {
    UnaryOpExpr(TokenType type, Expr* right) : type(type), right(right) {}
    std::string str(int depth) {
        return tabs(depth) + "(UnaryOp[" + token_to_str[type] + "]\n" + right->str(depth + 1) + ")";
    }
    TokenType type;
    Expr* right;
};
struct BinaryOpExpr : Expr {
    BinaryOpExpr() = delete;
    BinaryOpExpr(Expr* left, TokenType type, Expr* right) : left(left), type(type), right(right) {}
    std::string str(int depth) {
        return tabs(depth) + "(BinaryOp[" + token_to_str[type] + "]\n" + left->str(depth + 1) + "\n" +
               right->str(depth + 1)  + ")";
    }
    Expr* left;
    TokenType type;
    Expr* right;
};

struct CallExpr : Expr {
    CallExpr() = delete;
    CallExpr(NameExpr* fn_name, Expr* args) : fn_name(fn_name), args(args) {}
    std::string str(int depth) {
        return tabs(depth) + "(Call[" + fn_name->name + "]\n" 
            + args->str(depth + 1) + ")";
    }
    NameExpr* fn_name;
    Expr* args;
};

struct ReturnExpr : Expr {
    ReturnExpr() = delete;
    ReturnExpr(Expr* value) : value(value) {}
    std::string str(int depth) {
        return tabs(depth) + "(Return "  "\n" + value->str() + "\n" + tabs(depth) + ")";
    }
    Expr* value;
};

struct SubscriptExpr : Expr {
    SubscriptExpr() = delete;
    SubscriptExpr(Expr* array_name, Expr* index) : array_name(array_name), index(index) {}
    std::string str(int depth) {
        return tabs(depth) + "(ArrIndex \n" + array_name->str(depth+1) + "\n" + index->str(depth+1) +")";
    }
    Expr* array_name;
    Expr* index;
};

struct CommaListExpr : Expr {
    CommaListExpr() = delete;
    CommaListExpr(std::vector<Expr*> exprs) : exprs(exprs) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + "(CommaList \n";
        for (const auto expr : exprs){
            expr->str(depth+1) + "\n";
        }
        return str + ")";
    }
    std::vector<Expr*> exprs;
};

struct BlockExpr : Expr {
    BlockExpr() = delete;
    BlockExpr(std::vector<Expr*> stmts) : stmts(stmts) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + "{BlockExpr ";
        for (const auto expr : stmts){
            str += "\n" + expr->str(depth+1);
        }
        return str + "}";
    }
    std::vector<Expr*> stmts;
};

struct ForExpr : Expr {
    ForExpr() = delete;
    ForExpr(Expr* loop_var, Expr* range_expr, Expr* loop_body)
        : loop_var(loop_var), range_expr(range_expr), loop_body(loop_body) {}
    std::string str(int depth) {
        std::string str = tabs(depth) + "{ForExpr \n";
        str += loop_var->str(depth+1) + "\n";
        str += range_expr->str(depth+1) + "\n";
        str += loop_body->str(depth+1) + "\n";
        return str + "}";
    }

    Expr* loop_var;
    Expr* range_expr;
    Expr* loop_body;
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
        _prefix_func_table[LEFT_BRACE] = std::make_pair(&Parser::parseBlock, 0);
        _prefix_func_table[LEFT_PAREN] = std::make_pair(&Parser::parseGrouping, 0);
        _prefix_func_table[RET] = std::make_pair(&Parser::parseReturn, 0);
        _prefix_func_table[ID] = std::make_pair(&Parser::parseID, 5);
        _prefix_func_table[NUM] = std::make_pair(&Parser::parseNum, 5);
        _prefix_func_table[BANG] = std::make_pair(&Parser::parseUnaryOp, 30);
        _prefix_func_table[FOR] = std::make_pair(&Parser::parseFor, 100);
        _prefix_func_table[RIGHT_BRACE] = std::make_pair(&Parser::parseBlock, -77);

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
        // assume that finding these tokens in infix context implies an expression boundary
        _infix_func_table[ID] = std::make_pair(&Parser::infixboom, -77);
        _infix_func_table[NUM] = std::make_pair(&Parser::infixboom, -77);
        _infix_func_table[FOR] = std::make_pair(&Parser::infixboom, -77);
        _infix_func_table[IF] = std::make_pair(&Parser::infixboom, -77);
        _infix_func_table[RET] = std::make_pair(&Parser::infixboom, -77);
        _infix_func_table[SEMICOLON] = std::make_pair(&Parser::infixboom, -77);
        // if seen, should always stop parsing at RIGHT_PAREN, RIGHT_BRACKET
        _infix_func_table[RIGHT_PAREN] = std::make_pair(&Parser::infixboom, -777);
        _infix_func_table[RIGHT_BRACKET] = std::make_pair(&Parser::infixboom, -777);
        // BRACE should not ever be parsed as infix
        _infix_func_table[LEFT_BRACE] = std::make_pair(&Parser::infixboom, -777);
        _infix_func_table[RIGHT_BRACE] = std::make_pair(&Parser::infixboom, -777);
    }

    // core Pratt parsing routine
    Expr* ParseExpr(int precedence = 0) {
        if (endoftokens())
            return new EmptyExpr;
        auto prefix_type = tokit->type;
        auto token_pos = getTokenPos();
        printf("calling prefix fn for %dth tok %s\n", token_pos, token_to_str[prefix_type]);
        Expr* expr = getPrefixFunc(prefix_type)(*this);

        while (precedence < getInfixPrecedence()) {
            printf("calling infix fn for %dth tok %s\n", getTokenPos(), token_to_str[tokit->type]);
            expr = getInfixFunc(tokit->type)(*this, expr);
        }
        printf("ending prefix parse for %dth tok %s\n", token_pos, token_to_str[prefix_type]);

        return expr;
    }

    std::vector<Expr*> ParseStatements(int precedence = 0) {
        std::vector<Expr*> statements;
        // use these to check for forward progress
        while (not endoftokens() and precedence < getPrefixPrecedence()) {
            // printf("\nparsing statement at %s on LINE %d POS %d\n",
            //       token_to_str[tokit->type],
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
                // fprintf(stderr, RED "accepting right brace as closing statement \n" RESET);
                // accept a right brace as implictly terminating statement
            }
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
        printf("parsing block stmt starting here: %s\n", token_to_str[parser.currtype()]);
        parser.consume(); // consume brace
        std::vector<Expr*> statements;
        if (parser.currtype() == RIGHT_BRACE) {
            // handle empty block
            statements = {new EmptyExpr};
        } else {
            printf("\tparsing block stmt here: %s\n", token_to_str[parser.currtype()]);
            statements = parser.ParseStatements();
            printf("\tdone parsing block\n");
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
            list_elems.push_back(parser.ParseExpr(parser.getInfixPrec(COMMA)));
        }

        return new CommaListExpr(list_elems);
    }

    static Expr* prefixboom(Parser& parser) {
        fprintf(stderr, RED "prefixFunc for token type %s unimplemented.\n" RESET, token_to_str[parser.tokit->type]);
        exit(1);
        return new Expr();
    }
    static Expr* infixboom(Parser& parser, Expr*) {
        fprintf(stderr, RED "infixFunc for token type %s unimplemented.\n" RESET, token_to_str[parser.tokit->type]);
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
            _prefix_func_table[i] = std::make_pair(&prefixboom, 100);
        }
    }
    void initInfixTable(InfixTable& functable) {
        functable.resize(NUM_TOKEN_TYPES);
        std::fill(functable.begin(), functable.end(), std::make_pair(&infixboom, 100));
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
            printf(CYAN "%3d:" RESET, lineno);
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
    printDiv("Scanner");
    Scanner scanner(filebuf);
    auto tokens = scanner.scan();

    printDiv("Parser");
    Parser parser(tokens);
    auto statements = parser.ParseStatements();
    for (auto& stmt : statements) {
        stmt->print();
    }

    printDiv("cleanup");
}

void run_prompt() { printf("prompt goes here\n"); }

int main(int argc, char** argv) {

    if (argc == 2) {
        run_file(argv[1], false);
    } else if (argc == 1) {
        run_prompt();
    } else {
        printf("too many args!\n");
    }

    return 0;
}
