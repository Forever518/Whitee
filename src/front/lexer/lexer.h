#ifndef COMPILER_LEXER_H
#define COMPILER_LEXER_H

#include <cwchar>
#include <string>
#include <vector>

using namespace std;

enum TokenType {
    IDENT,
    INTCONST,
    STRCONST,
    CONST_TK,
    INT_TK,
    VOID_TK,
    IF_TK,
    ELSE_TK,
    WHILE_TK,
    BREAK_TK,
    CONTINUE_TK,
    RETURN_TK,
    COMMA,
    SEMICOLON,
    LBRACKET,
    RBRACKET,
    LBRACE,
    RBRACE,
    LPAREN,
    RPAREN,
    ASSIGN,
    PLUS,
    MINUS,
    NOT,
    MULT,
    DIV,
    REMAIN,
    LESS,
    LARGE,
    LEQ,
    LAQ,
    EQUAL,
    NEQUAL,
    AND,
    OR,
    END
};

class TokenInfo {
private:
    TokenType symbol;
    string name;
    int value;

    // How to deal with different kinds of value?
    // for each type,
    // set a unique type_token class
    // which can save message specially and set interface for symbolTable
public:
    explicit TokenInfo(TokenType sym);

    TokenType getSym();

    string getName();

    [[nodiscard]] int getValue() const;

    void setName(string na);

    void setValue(int i);
};

extern std::vector<TokenInfo> tokenInfoList;

void parseSym(FILE *in);

bool isSpace();

bool isLetter();

bool isDigit();

bool isTab();

bool isNewline();

void clearToken();

void catToken();

long long strToInt(bool isHex, bool isOct);

bool lexicalAnalyze(const string &file);

#endif
