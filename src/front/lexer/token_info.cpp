#include "lexer.h"

TokenInfo::TokenInfo(TokenType sym) {
    this->symbol = sym;
    this->value = 0;
}

TokenType TokenInfo::getSym() {
    return symbol;
}

string TokenInfo::getName() {
    return name;
}

void TokenInfo::setName(string na) {
    this->name = std::move(na);
}

int TokenInfo::getValue() const {
    return this->value;
}

void TokenInfo::setValue(int i) {
    this->value = i;
}
