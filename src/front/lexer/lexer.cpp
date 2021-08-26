#include "lexer.h"
#include "../../basic/std/compile_std.h"

#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace std;

int c;
string token;

std::vector<TokenInfo> tokenInfoList;

extern string debugMessageDirectory;

std::unordered_map<string, TokenType>
        reverseTable{ // NOLINT
        {"int",      INT_TK},
        {"const",    CONST_TK},
        {"void",     VOID_TK},
        {"if",       IF_TK},
        {"else",     ELSE_TK},
        {"while",    WHILE_TK},
        {"break",    BREAK_TK},
        {"continue", CONTINUE_TK},
        {"return",   RETURN_TK},
        {",",        COMMA},
        {";",        SEMICOLON},
        {"[",        LBRACKET},
        {"]",        RBRACKET},
        {"{",        LBRACE},
        {"}",        RBRACE},
        {"(",        LPAREN},
        {")",        RPAREN},
        {"=",        ASSIGN},
        {"+",        PLUS},
        {"-",        MINUS},
        {"!",        NOT},
        {"*",        MULT},
        {"/",        DIV},
        {"%",        REMAIN},
        {"<",        LESS},
        {">",        LARGE},
        {"<=",       LEQ},
        {">=",       LAQ},
        {"==",       EQUAL},
        {"!=",       NEQUAL},
        {"&&",       AND},
        {"||",       OR}
};

// type1:/* */ type2:// //
void skipComment(int type, FILE *in) {
    if (type == 1) {
        while (true) {
            c = fgetc(in);
            if (c == '*') {
                c = fgetc(in);
                if (c == '/') {
                    break;
                }
            }
        }
    } else {
        while (true) {
            c = fgetc(in);
            if (c == '\n') {
                break;
            }
        }
    }
}

void skipSpaceOrNewlineOrTab(FILE *in) {
    while (isSpace() || isNewline() || isTab()) {
        c = fgetc(in);
    }
}

void dealWithKeywordOrIdent(FILE *in) {
    while ((isLetter() || isDigit())) {
        catToken();
        c = fgetc(in);  //if c is not a whitespace, then jump to error
    }
    fseek(in, -1L, 1);
    string name(token);
    if (reverseTable.find(token) != reverseTable.end()) {
        TokenInfo tmp(reverseTable.at(token));
        tmp.setName(name);
        tokenInfoList.push_back(tmp);
    } else {
        TokenInfo tmp(IDENT);
        tmp.setName(token);
        tokenInfoList.push_back(tmp);
    }
}

void dealWithConstDigit(FILE *in) {
    // deal with 0Xxxx
    bool isHex = false;
    bool isOct = false;
    if (c == '0') {
        c = fgetc(in);
        if (c == 'x' || c == 'X') {
            isHex = true;
            c = fgetc(in);
        } else {
            isOct = true;
            fseek(in, -1L, 1);
            c = '0';
        }
    }
    while (isDigit()) {
        catToken();
        c = fgetc(in);
    }
    long long integer = strToInt(isHex, isOct);
    fseek(in, -1L, 1);
    TokenInfo tmp(INTCONST);
    tmp.setName(token);
    if (integer == 2147483648 && tokenInfoList[tokenInfoList.size() - 1].getSym() == MINUS) {
        tokenInfoList.pop_back();
        tmp.setValue(-integer);
    } else {
        tmp.setValue(integer);
    }
    tokenInfoList.push_back(tmp);
}

void dealWithStr(FILE *in) {
    while (true) {
        c = fgetc(in);
        catToken();
        if (c == '"') {
            break;
        }
    }
    TokenInfo tmp(STRCONST);
    tmp.setName(token);
    tokenInfoList.push_back(tmp);
}

void dealWithOtherTk(FILE *in) {
    switch (c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case ':':
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case ',':
        case ';': {
            TokenInfo tmp(reverseTable.at(string(1, c)));
            tmp.setName(string(1, c));
            tokenInfoList.push_back(tmp);
            break;
        }
        case '|':
        case '&': {
            fgetc(in);
            TokenInfo tmp(reverseTable.at(string(2, c)));
            tmp.setName(string(2, c));
            tokenInfoList.push_back(tmp);
            break;
        }
        case '<':
        case '>':
        case '!':
        case '=': {
            catToken();
            c = fgetc(in);
            if (c == '=') {
                catToken();
            } else {
                fseek(in, -1L, 1);
            }
            TokenInfo tmp(reverseTable.at(token));
            tmp.setName(token);
            tokenInfoList.push_back(tmp);
            break;
        }
        default:
            break;
    }
}

void skipIllegalChar(FILE *in) {
    while (!isNewline() && c < 32) {
        c = fgetc(in);
        if (c == EOF) {
            break;
        }
    }
}

bool lexicalAnalyze(const string &file) {
    FILE *in = fopen(file.c_str(), "r");
    if (!in) {
        return false;
    }
    parseSym(in);
    if (_debugLexer) {
        FILE *out = fopen((debugMessageDirectory + "lexer.txt").c_str(), "w+");
        fprintf(out, "[Lexer]\n");
        for (TokenInfo tokenInfo : tokenInfoList) {
            fprintf(out, "%s", tokenInfo.getName().c_str());
            if (tokenInfo.getSym() == INTCONST) {
                fprintf(out, " %d", tokenInfo.getValue());
            }
            fprintf(out, "\n");
        }
        fclose(out);
    }
    fclose(in);
    return true;
}

void parseSym(FILE *in) {
    while (c != EOF) {
        clearToken();
        c = fgetc(in);

        skipIllegalChar(in);
        skipSpaceOrNewlineOrTab(in);

        // perhaps comment
        if (c == '/') {
            c = fgetc(in);
            if (c == '/') {
                skipComment(2, in);
                continue;
            } else if (c == '*') {
                skipComment(1, in);
                continue;
            } else {
                fseek(in, -1L, 1);
                c = '/';
            }
        }

        if (isLetter()) {
            dealWithKeywordOrIdent(in);
        } else if (isDigit()) {
            dealWithConstDigit(in);
        } else if (c == '"') {// turn '/' to '/'
            dealWithStr(in);
        } else {
            dealWithOtherTk(in);
        }

    }
    tokenInfoList.emplace_back(TokenType::END);
}

bool isSpace() {
    return c == ' ';
}

bool isLetter() {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_');
}

bool isDigit() {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

bool isTab() {
    return c == '\t';
}

bool isNewline() {
    return c == '\n';
}

void clearToken() {
    token.clear();
}

void catToken() {
    token.push_back(c);
}

long long strToInt(bool isHex, bool isOct) {
    long long integer;
    if (isHex) {
        sscanf(token.c_str(), "%llx", &integer);
    } else if (isOct) {
        sscanf(token.c_str(), "%llo", &integer);
    } else {
        sscanf(token.c_str(), "%lld", &integer);
    }
    return integer;
}

