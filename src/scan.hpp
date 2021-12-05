#pragma once

#include <cassert>
#include <cstring>
#include <stdlib.h>
#include <string>
#include <vector>

#include "cfg.hpp"
#include "color.hpp"
#include "re.hpp"

#define DECL_TOKEN_TYPE(type, _) type,
enum TokenType {
#include "token_types.inc"
};
#undef DECL_TOKEN_TYPE

// array map from TokenType to type string
// (i.e. FOR --> "FOR", EQUALS --> "EQUALS")
#define DECL_TOKEN_TYPE(type, _) #type,
const char* token_to_typestr[] = {
#include "token_types.inc"
};
#undef DECL_TOKEN_TYPE

// array map from TokenType to its source file representation
// (i.e. FOR --> 'for', EQUALS --> '=')
#define DECL_TOKEN_TYPE(_, repr) repr,
const char* token_to_repr[] = {
#include "token_types.inc"
};
#undef DECL_TOKEN_TYPE

// Token type used in Scanner and parser
struct Token {
    TokenType type = NUM_TOKEN_TYPES;
    const std::string str = "";
    int lineno = 0;
    int linepos = 0;
};

#define SINGLE_CHAR_TOKEN(__ch__, __token_type__)                                                  \
    case __ch__: {                                                                                 \
        tok(__token_type__, str(__ch__));                                                          \
        break;                                                                                     \
    }

#define KW_MATCH(__token_type__, __token_str__)                                                    \
    else if (re_match(idstr, "^" #__token_str__ "$")) {                                            \
        return __token_type__;                                                                     \
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
            case '"': {
                int start_lineno = _lineno;
                if (scanVerbose)
                    printf("Capturing String Literal " BRIGHTBLUE "\"");
                ch = advance();
                std::string str;
                for (; ch != '"' and ch != '\0'; ch = advance()) {
                    if (scanVerbose)
                        printf("%c", ch);
                    if (ch == '\n')
                        _lineno++;
                    str.push_back(ch);
                }
                if (scanVerbose)
                    printf("\"" RESET " on LINE %d\n",start_lineno);

                tok(STRING, str);

                break;
            }
            case '#': {
                if (scanVerbose)
                    printf("Commented " CYAN);
                for (; ch != '\n' and ch != '\0'; ch = advance()) {
                    if (scanVerbose)
                        printf("%c", ch);
                }
                if (scanVerbose)
                    printf(RESET " on LINE %d.\n", _lineno);
                _lineno++;
                break;
            }
            case 'a' ... 'z':
            case 'A' ... 'Z':
            case '_': {
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
                printf("Found unimpl char '%c' (%d) at lineno %d, pos %d\n",
                       ch,
                       ch,
                       _lineno,
                       _linepos);
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
        KW_MATCH(ELSE, else)
        KW_MATCH(CMP, cmp)
        KW_MATCH(FN, fn)
        KW_MATCH(FOR,for)
        KW_MATCH(VAR,var)
        KW_MATCH(IF, if)
        KW_MATCH(OR, or)
        KW_MATCH(PRINT, print)
        KW_MATCH(RET, ret)
        KW_MATCH(TO, to)
        KW_MATCH(TRUE, True)
        KW_MATCH(FALSE, False)
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
        _tokens.push_back({.type = type,
                           .str = std::move(str),
                           .lineno = _lineno,
                           .linepos = _linepos});
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
                printf(CYAN "LINE %d: \n" RESET, curr_lineno);
            }
            printf("\t%-12s = %-10s at %d,%d  \n",
                   token_to_typestr[tok.type],
                   tok.str.c_str(),
                   tok.lineno,
                   tok.linepos);
        }
    }

    std::vector<Token> _tokens;
    int _linepos = 0;
    int _lineno = 1;
    const char* _srcbuf = nullptr;
    const size_t _sz = 0;
};
