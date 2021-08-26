#include "syntax_analyze.h"
#include "symbol_table.h"
#include "../lexer/lexer.h"
#include "../../basic/std/compile_std.h"

#include <iostream>
#include <memory>
#include <unordered_set>

/**
 * Declaration of analyzeFunctions:
 * Note: ConstExp -> AddExp (Ident should be Const).
 * That Const value is calculated while analyzing.
 */

/**
 * @brief get const value(int).
 * @details store the value get in symbol table,
 * for next replace all the const var to a single int value.
 */
int getConstExp();

int getMulExp();

int getUnaryExp();

int getConstVarExp();

shared_ptr<DeclNode> analyzeDecl();

shared_ptr<ConstDeclNode> analyzeConstDecl();

shared_ptr<ConstDefNode> analyzeConstDef();

shared_ptr<ConstInitValNode> analyzeConstInitVal(const std::shared_ptr<SymbolTableItem> &ident);

shared_ptr<ConstInitValArrNode> analyzeConstInitValArr(int dimension, std::vector<int> &numOfEachDimension);

shared_ptr<VarDeclNode> analyzeVarDecl();

shared_ptr<VarDefNode> analyzeVarDef();

void analyzeInitVal(const std::shared_ptr<SymbolTableItem> &ident);

std::shared_ptr<InitValArrNode> analyzeInitValArr(int dimension, std::vector<int> &numOfEachDimension);

shared_ptr<FuncDefNode> analyzeFuncDef();

shared_ptr<FuncFParamsNode> analyzeFuncFParams();

shared_ptr<FuncFParamNode> analyzeFuncFParam();

shared_ptr<BlockNode> analyzeBlock(bool isFuncBlock, bool isVoid, bool isInWhileFirstBlock);

shared_ptr<BlockItemNode> analyzeBlockItem(bool isInWhileFirstBlock);

shared_ptr<StmtNode> analyzeStmt(bool isInWhileFirstBlock);

shared_ptr<ExpNode> analyzeExp();

shared_ptr<CondNode> analyzeCond();

shared_ptr<LValNode> analyzeLVal();

shared_ptr<PrimaryExpNode> analyzePrimaryExp();

shared_ptr<UnaryExpNode> analyzeUnaryExp();

shared_ptr<FuncRParamsNode> analyzeFuncRParams();

shared_ptr<MulExpNode> analyzeMulExp();

shared_ptr<AddExpNode> analyzeAddExp();

shared_ptr<RelExpNode> analyzeRelExp();

shared_ptr<EqExpNode> analyzeEqExp();

shared_ptr<LAndExpNode> analyzeLAndExp();

shared_ptr<LOrExpNode> analyzeLOrExp();

/**
 * Functions about Lexer.
 */
static int lexerPosition = 0;

/// Initialize after lexer analyze.
static TokenInfo *nowPointerToken;

void popNextLexer() {
    if (nowPointerToken->getSym() == TokenType::END) {
        std::cerr << "Out of lexer vector range." << endl;
    }
    nowPointerToken = &tokenInfoList[++lexerPosition];
    if (_debugSyntax) {
        if (tokenInfoList[lexerPosition - 1].getSym() == TokenType::INTCONST) {
            std::cout << tokenInfoList[lexerPosition - 1].getValue() << '\n';
        } else {
            std::cout << tokenInfoList[lexerPosition - 1].getName() << '\n';
        }
    }
}

TokenInfo *peekNextLexer(int num) {
    if (lexerPosition + num > tokenInfoList.size()) {
        std::cerr << "Out of lexer vector range." << endl;
    }
    return &tokenInfoList[lexerPosition + num];
}

static shared_ptr<SymbolTableItem> nowFuncSymbol;
unordered_map<std::string, std::string> usageNameListOfVarSingleUseInUnRecursionFunction;
static bool isInWhileLockedCond = false;
static int step = 0;
static bool folderShouldBeOpen = false;
static int folderOpenNum = 0;
static bool isInDirectWhile = false;
static shared_ptr<StmtNode> whileBlockAssignStmt;
static bool lockingOpenFolder = false;
static bool isAssignLValWhileControlVar = false;
static bool judgeWhetherAssignLValWhileControlVar = false;
static int whileControlVarAssignNum = 0;
static shared_ptr<SymbolTableItem> lastAssignOrDeclVar;
static bool isLastValueGetableAssignOrDecl = false;
static bool getAssignLVal = false;
static int lastValue = 0;

bool isExpConstOrIMM(std::shared_ptr<ExpNode> exp) {
    auto addExp = s_p_c<AddExpNode>(exp);
    if (addExp->addExp) {
        return false;
    }
    auto mulExp = addExp->mulExp;
    if (mulExp->mulExp) {
        return false;
    }
    auto unaryExp = mulExp->unaryExp;
    bool isOpPlus = true;
    while (unaryExp->type == UnaryExpType::UNARY_DEEP) {
        if (unaryExp->op == "-") {
            isOpPlus = !isOpPlus;
        }
        unaryExp = unaryExp->unaryExp;
    }
    if (unaryExp->type == UnaryExpType::UNARY_PRIMARY) {
        auto primaryExp = unaryExp->primaryExp;
        if (primaryExp->type == PrimaryExpType::PRIMARY_NUMBER) {
            lastValue = isOpPlus ? primaryExp->number : -primaryExp->number;
            return true;
        }
        if (primaryExp->type == PrimaryExpType::PRIMARY_L_VAL) {
            auto lValExp = primaryExp->lVal;
            if (lValExp->ident->ident->symbolType == SymbolType::CONST_VAR) {
                lastValue = s_p_c<ConstInitValValNode>(lValExp->ident->ident->constInitVal)->value;
                lastValue = isOpPlus ? lastValue : -lastValue;
                return true;
            }
        }
    }
    return false;
}

/**
 * Symbol Table:
 */
unordered_map<std::pair<int, int>, std::shared_ptr<SymbolTablePerBlock>, pair_hash> symbolTable; // NOLINT

std::unordered_map<int, int> blockLayerId2LayerNum; // NOLINT

static int nowLayer = 0;

static pair<int, int> nowLayId = distributeBlockId(0, {0, 0});// NOLINT
static pair<int, int> layIdInFuncFParams; // NOLINT
static pair<int, int> globalLayId = {0, 1}; // NOLINT

bool isGlobalBlock(pair<int, int> layId) {
    return globalLayId == layId;
}

/**
 *
 * @brief get a ident
 * @details when the ident is for a var:
 *  1) var is normal var
 *   - var is non ary var.
 *   - var is ary var.
 *  2) var is const
 *   - var is const non ary var.
 *   - var is const ary var.
 * All these type do:
 *  1. insert the symbolTableItem created into symbolTable.
 *  2. ret IdentNode {
 *      symbolTableItem {
 *          all the details.
 *      }
 *  }
 *
 * when the ident is for a function:
 *  1. insert the symbolTableItem created into symbolTable.
 *  2. ret IdentNode {
 *      symbolTableItem {
 *          name.
 *          symbolType.
 *          blockId.
 *      }
 *  }
 *  3. the IdentNode ret will be used in a FuncNode, the FuncNode include a identNode.
 * @param isF whether the ident is for a function or a var
 * @param isVoid when ident is for a function, distinguish the function's type: RET & VOID
 * @param isConst when ident is for a var, distinguish the var type: CONST & VAR
 * @return A identNode.
 */
shared_ptr<IdentNode> getIdentDefine(bool isF, bool isVoid, bool isConst) {
    if (_debugSyntax) {
        std::cout << "--------getIdentDefine--------\n";
    }
    std::string name = nowPointerToken->getName();
    popNextLexer();
    if (isF) {
        SymbolType symbolType = isVoid ? SymbolType::VOID_FUNC : SymbolType::RET_FUNC;
        auto symbolTableItem = std::make_shared<SymbolTableItem>(symbolType, name, nowLayId);
        nowFuncSymbol = symbolTableItem;
        insertSymbol(nowLayId, symbolTableItem);
        return std::make_shared<IdentNode>(symbolTableItem);
    }
    int dimension = 0;
    std::vector<int> numOfEachDimension;
    while (nowPointerToken->getSym() == TokenType::LBRACKET) {
        popNextLexer(); // LBRACKET
        dimension++;
        numOfEachDimension.push_back(getConstExp());
        popNextLexer(); // RBRACKET
    }
    if (dimension == 0) {
        SymbolType symbolType = (isConst ? SymbolType::CONST_VAR : SymbolType::VAR);
        auto symbolTableItem = std::make_shared<SymbolTableItem>(symbolType, name, nowLayId);
        if (openFolder && !lockingOpenFolder) {
            lastAssignOrDeclVar = symbolTableItem;
        }
        insertSymbol(nowLayId, symbolTableItem);
        return std::make_shared<IdentNode>(symbolTableItem);
    } else {
        SymbolType symbolType = (isConst ? SymbolType::CONST_ARRAY : SymbolType::ARRAY);
        auto symbolTableItem = std::make_shared<SymbolTableItem>(symbolType, dimension, numOfEachDimension, name,
                                                                 nowLayId);
        insertSymbol(nowLayId, symbolTableItem);
        return std::make_shared<IdentNode>(symbolTableItem);
    }
}

/**
 *
 * @details Use for ident in function
 * For dimension > 0 ident is a[][*][*]
 * Should consider the first [].
 * @return
 */
shared_ptr<IdentNode> getIdentDefineInFunction() {
    if (_debugSyntax) {
        std::cout << "--------getIdentDefine--------\n";
    }
    std::string name = nowPointerToken->getName();
    popNextLexer();
    int dimension = 0;
    std::vector<int> numOfEachDimension;
    if (nowPointerToken->getSym() == TokenType::LBRACKET) {
        popNextLexer();
        dimension++;
        numOfEachDimension.push_back(0);
        popNextLexer();
    }
    while (nowPointerToken->getSym() == TokenType::LBRACKET) {
        popNextLexer(); // LBRACKET
        dimension++;
        numOfEachDimension.push_back(getConstExp());
        popNextLexer(); // RBRACKET
    }
    if (dimension == 0) {
        SymbolType symbolType = (SymbolType::VAR);
        auto symbolTableItem = std::make_shared<SymbolTableItem>(symbolType, name, nowLayId);
        insertSymbol(nowLayId, symbolTableItem);
        return std::make_shared<IdentNode>(symbolTableItem);
    } else {
        SymbolType symbolType = (SymbolType::ARRAY);
        auto symbolTableItem = std::make_shared<SymbolTableItem>(symbolType, dimension, numOfEachDimension, name,
                                                                 nowLayId);
        insertSymbol(nowLayId, symbolTableItem);
        return std::make_shared<IdentNode>(symbolTableItem);
    }
}

/**
 * @details
 * !!! when !isF, should copy symbolTableItem, For expressionOfPerDimension is in symbolTableItem.
 * And every usage & decl is not the same. when use shared_ptr, the real one is shared.
 * Change in one place, change in another place, cause error.
 * Should copy a new one.
 * No need for function.
 * @param isF
 * @return
 */
shared_ptr<IdentNode> getIdentUsage(bool isF) {
    if (_debugSyntax) {
        std::cout << "--------getIdentUsage--------\n";
    }
    std::string name = nowPointerToken->getName();
    popNextLexer();
    auto symbolInTable = findSymbol(nowLayId, name, isF);
    if (symbolInTable->eachFuncUseNum.find(nowFuncSymbol->usageName) == symbolInTable->eachFuncUseNum.end()) {
        symbolInTable->eachFuncUseNum[nowFuncSymbol->usageName] = 0;
        symbolInTable->eachFunc.push_back(nowFuncSymbol);
    }
    symbolInTable->eachFuncUseNum[nowFuncSymbol->usageName]++;
    if (openFolder && getAssignLVal && !lockingOpenFolder) {
        lastAssignOrDeclVar = symbolInTable;
        getAssignLVal = false;
    }
    if (openFolder && lockingOpenFolder && judgeWhetherAssignLValWhileControlVar) {
        isAssignLValWhileControlVar = (symbolInTable == lastAssignOrDeclVar);
        judgeWhetherAssignLValWhileControlVar = false;
    }
    if (isF) {
        unordered_set<string> sysFunc(
                {"getint", "getch", "getarray", "putint", "putch", "putarray", "putf", "starttime", "stoptime"});

        if (nowFuncSymbol == symbolInTable) {
            symbolInTable->isRecursion = true;
        }
        if (openFolder) {
            if (sysFunc.find(nowFuncSymbol->name) == sysFunc.end()) {
                whileControlVarAssignNum += 2;
            }
        }
        return std::make_shared<IdentNode>(
                std::make_shared<SymbolTableItem>(symbolInTable->symbolType, name, symbolInTable->blockId));
    } else {
        std::vector<std::shared_ptr<ExpNode>> expressionOfEachDimension;
        while (nowPointerToken->getSym() == TokenType::LBRACKET) {
            popNextLexer(); // LBRACKET
            expressionOfEachDimension.push_back(analyzeExp());
            popNextLexer(); // RBRACKET
        }
        std::shared_ptr<SymbolTableItem> symbolInIdent(
                new SymbolTableItem(symbolInTable->symbolType, symbolInTable->dimension,
                                    expressionOfEachDimension, name,
                                    symbolInTable->blockId));
        if (symbolInTable->symbolType == SymbolType::CONST_ARRAY ||
            symbolInTable->symbolType == SymbolType::CONST_VAR) {
            symbolInIdent->constInitVal = symbolInTable->constInitVal;
        }
        symbolInIdent->numOfEachDimension = symbolInTable->numOfEachDimension;
        return std::make_shared<IdentNode>(symbolInIdent);
    }
}

/**
 * Definition of analyzeFunctions.
 */
shared_ptr<DeclNode> analyzeDecl() {
    if (_debugSyntax) {
        std::cout << "--------analyzeDecl--------\n";
    }
    shared_ptr<DeclNode> declNode(nullptr);
    if (openFolder) {
        whileControlVarAssignNum += 2;
    }
    if (nowPointerToken->getSym() == TokenType::CONST_TK) {
        declNode = analyzeConstDecl();
    } else {
        declNode = analyzeVarDecl();
    }
    return declNode;
}

shared_ptr<ConstDeclNode> analyzeConstDecl() {
    if (_debugSyntax) {
        std::cout << "--------analyzeConstDecl--------\n";
    }
    popNextLexer(); // CONST_TK
    popNextLexer(); // INT_TK
    vector<shared_ptr<ConstDefNode>> constDefList;
    constDefList.push_back(analyzeConstDef());
    while (nowPointerToken->getSym() == TokenType::COMMA) {
        popNextLexer(); // COMMA
        constDefList.push_back(analyzeConstDef());
    }
    popNextLexer(); // SEMICOLON
    return std::make_shared<ConstDeclNode>(constDefList);
}

shared_ptr<ConstDefNode> analyzeConstDef() {
    if (_debugSyntax) {
        std::cout << "--------analyzeConstDef--------\n";
    }
    auto ident = getIdentDefine(false, false, true);
    popNextLexer(); // ASSIGN
    auto constInitVal = analyzeConstInitVal(ident->ident);
    return std::make_shared<ConstDefNode>(ident, constInitVal);
}

/**
 *
 * @param ident is a SymbolTableItem, use it to get the dimension of this const var,
 * to judge the type of the constInitVal:
 * 1. ConstInitValValNode
 * 2. ConstInitValAryNode
 * @return
 */
shared_ptr<ConstInitValNode> analyzeConstInitVal(const std::shared_ptr<SymbolTableItem> &ident) {
    if (_debugSyntax) {
        std::cout << "--------analyzeInitVal--------\n";
    }
    if (ident->dimension == 0) {
        int value = getConstExp();
        ident->constInitVal = std::make_shared<ConstInitValValNode>(value);
    } else {
        ident->constInitVal = analyzeConstInitValArr(0, ident->numOfEachDimension);
    }
    return ident->constInitVal;
}

/**
 * @deprecated
 * for case: const int a[10000000] = {1};
 * Time usage is not bearable.
 */
shared_ptr<ConstInitValArrNode> constZeroDefaultArr(int dimension, std::vector<int> &numOfEachDimension) {
    int thisDimensionNum = numOfEachDimension[dimension];
    vector<shared_ptr<ConstInitValNode>> valueList;
    if (dimension + 1 == numOfEachDimension.size()) {
        for (int i = 0; i < thisDimensionNum; i++) {
            int exp = 0;
            valueList.push_back(std::make_shared<ConstInitValValNode>(exp));
        }
    } else {
        for (int i = 0; i < thisDimensionNum; i++) {
            valueList.push_back(constZeroDefaultArr(dimension + 1, numOfEachDimension));
        }
    }
    return std::make_shared<ConstInitValArrNode>(thisDimensionNum, valueList);
}

/**
 *
 * @brief Get Const Arr Init Value.
 * @details Dimension is a important param,
 * For this function is used to be Recursive.
 * Dimension = 1 means the arr is like [1, 2, 3, ...].
 * The item in the vector should be ConstInitValValNode instead of ConstInitValArrNode.
 */
shared_ptr<ConstInitValArrNode> analyzeConstInitValArr(int dimension, std::vector<int> &numOfEachDimension) {
    if (_debugSyntax) {
        std::cout << "--------analyzeConstInitValArr--------\n";
    }
    int thisDimensionNum = numOfEachDimension[dimension];
    vector<shared_ptr<ConstInitValNode>> valueList;
    if (dimension + 1 == numOfEachDimension.size()) {
        if (nowPointerToken->getSym() == TokenType::LBRACE) {
            popNextLexer(); // LBRACE
            for (int i = 0; i < thisDimensionNum; i++) {
                if (nowPointerToken->getSym() == TokenType::RBRACE) {
                    break;
                } else {
                    auto exp = getConstExp();
                    valueList.push_back(std::make_shared<ConstInitValValNode>(exp));
                    if (nowPointerToken->getSym() == TokenType::COMMA) {
                        popNextLexer(); // COMMA
                    }
                }
            }
            popNextLexer(); // RBRACE
        } else {
            for (int i = 0; i < thisDimensionNum; i++) {
                if (nowPointerToken->getSym() == TokenType::LBRACE || nowPointerToken->getSym() == TokenType::RBRACE) {
                    break;
                }
                auto exp = getConstExp();
                valueList.push_back(std::make_shared<ConstInitValValNode>(exp));
                if (nowPointerToken->getSym() == TokenType::COMMA) {
                    popNextLexer(); // COMMA
                }
            }
        }
        return std::make_shared<ConstInitValArrNode>(thisDimensionNum, valueList);
    }
    if (nowPointerToken->getSym() == TokenType::LBRACE) {
        popNextLexer(); // LBRACE
        for (int i = 0; i < thisDimensionNum; i++) {
            if (nowPointerToken->getSym() == TokenType::RBRACE) {
                break;
            } else {
                valueList.push_back(analyzeConstInitValArr(dimension + 1, numOfEachDimension));
                if (nowPointerToken->getSym() == TokenType::COMMA) {
                    popNextLexer(); // COMMA
                }
            }
        }
        popNextLexer(); // RBRACE
    } else {
        for (int i = 0; i < thisDimensionNum; i++) {
            valueList.push_back(analyzeConstInitValArr(dimension + 1, numOfEachDimension));
            if (nowPointerToken->getSym() == TokenType::COMMA) {
                popNextLexer(); // COMMA
            }
        }
    }
    return std::make_shared<ConstInitValArrNode>(thisDimensionNum, valueList);
}

int getConstExp() {
    if (_debugSyntax) {
        std::cout << "--------getConstExp--------\n";
    }
    int value = getMulExp();
    while (nowPointerToken->getSym() == TokenType::PLUS || nowPointerToken->getSym() == TokenType::MINUS) {
        if (nowPointerToken->getSym() == TokenType::PLUS) {
            popNextLexer();
            value += getMulExp();
        } else {
            popNextLexer();
            value -= getMulExp();
        }
    }
    return value;
}

int getMulExp() {
    if (_debugSyntax) {
        std::cout << "--------getMulExp--------\n";
    }
    int value = getUnaryExp();
    while (nowPointerToken->getSym() == TokenType::MULT || nowPointerToken->getSym() == TokenType::DIV ||
           nowPointerToken->getSym() == TokenType::REMAIN) {
        if (nowPointerToken->getSym() == TokenType::MULT) {
            popNextLexer();
            value *= getUnaryExp();
        } else if (nowPointerToken->getSym() == TokenType::DIV) {
            popNextLexer();
            value /= getUnaryExp();
        } else {
            popNextLexer();
            value %= getUnaryExp();
        }
    }
    return value;
}

int getUnaryExp() {
    if (_debugSyntax) {
        std::cout << "--------getUnaryExp--------\n";
    }
    int value;
    if (nowPointerToken->getSym() == TokenType::LPAREN) {
        popNextLexer();
        value = getConstExp();
        popNextLexer();
    } else if (nowPointerToken->getSym() == TokenType::INTCONST) {
        value = nowPointerToken->getValue();
        popNextLexer();
    } else if (nowPointerToken->getSym() == TokenType::PLUS) {
        popNextLexer();
        value = getUnaryExp();
    } else if (nowPointerToken->getSym() == TokenType::MINUS) {
        popNextLexer();
        value = -getUnaryExp();
    } else {
        value = getConstVarExp();
    }
    return value;
}

int getConstVarExp() {
    if (_debugSyntax) {
        std::cout << "--------getConstVarExp--------\n";
    }
    std::string name = nowPointerToken->getName();
    popNextLexer(); // IDENT
    auto symbolInTable = findSymbol(nowLayId, name, false);
    int dimension = symbolInTable->dimension;
    vector<shared_ptr<ConstInitValNode>> valList;
    if (dimension > 0) {
        if (symbolInTable->symbolType == SymbolType::CONST_ARRAY) {
            valList = (s_p_c<ConstInitValArrNode>(symbolInTable->constInitVal))->valList;
        } else {
            valList = (s_p_c<ConstInitValArrNode>(symbolInTable->globalVarInitVal))->valList;
        }
    } else {
        if (symbolInTable->symbolType == SymbolType::CONST_VAR) {
            return (s_p_c<ConstInitValValNode>(symbolInTable->constInitVal))->value;
        }
        return (s_p_c<ConstInitValValNode>(symbolInTable->globalVarInitVal))->value;
    }
    while (dimension > 0) {
        dimension--;
        popNextLexer(); // LBRACKET
        int offset = getConstExp();
        popNextLexer(); // RBRACKET
        if (offset >= valList.size()) {
            return 0;
        }
        if (dimension == 0) {
            return (s_p_c<ConstInitValValNode>(valList[offset]))->value;
        } else {
            valList = (s_p_c<ConstInitValArrNode>(valList[offset]))->valList;
        }
    }
    cerr << "getConstVarExp: Source code may have errors." << endl;
    return -1;
} // NOLINT

std::shared_ptr<VarDeclNode> analyzeVarDecl() {
    if (_debugSyntax) {
        std::cout << "--------analyzeVarDecl--------\n";
    }
    popNextLexer(); // INT_TK
    vector<shared_ptr<VarDefNode>> varDefList;
    varDefList.push_back(analyzeVarDef());
    while (nowPointerToken->getSym() == TokenType::COMMA) {
        popNextLexer(); // INT_TK
        varDefList.push_back(analyzeVarDef());
    }
    popNextLexer(); // SEMICOLON
    return std::make_shared<VarDeclNode>(varDefList);
}

std::shared_ptr<VarDefNode> analyzeVarDef() {
    if (_debugSyntax) {
        std::cout << "--------analyzeVarDef--------\n";
    }
    auto ident = getIdentDefine(false, false, false);
    bool hasAssigned = false;
    if (nowPointerToken->getSym() == TokenType::ASSIGN) {
        hasAssigned = true;
        popNextLexer(); // ASSIGN
        analyzeInitVal(ident->ident);
        std::shared_ptr<InitValNode> initVal;
        if (!isGlobalBlock(ident->ident->blockId)) {
            initVal = ident->ident->initVal;
            if (ident->ident->dimension == 0) {
                return std::make_shared<VarDefNode>(ident, initVal);
            } else {
                return std::make_shared<VarDefNode>(ident, ident->ident->dimension, ident->ident->numOfEachDimension,
                                                    initVal);
            }
        }
    }
    if (!hasAssigned && isGlobalBlock(ident->ident->blockId)) {
        if (ident->ident->dimension == 0) {
            int value = 0;
            ident->ident->globalVarInitVal = std::make_shared<ConstInitValValNode>(value);
        }
    }
    if (ident->ident->dimension == 0) {
        return std::make_shared<VarDefNode>(ident);
    } else {
        return std::make_shared<VarDefNode>(ident, ident->ident->dimension, ident->ident->numOfEachDimension);
    }
}

void analyzeInitVal(const std::shared_ptr<SymbolTableItem> &ident) {
    if (_debugSyntax) {
        std::cout << "--------analyzeInitVal--------\n";
    }
    if (ident->dimension == 0) {
        if (isGlobalBlock(ident->blockId)) {
            int value = getConstExp();
            if (openFolder && !lockingOpenFolder) {
                lastValue = value;
                isLastValueGetableAssignOrDecl = true;
            }
            ident->globalVarInitVal = std::make_shared<ConstInitValValNode>(value);
        } else {
            auto expNode = analyzeExp();
            if (openFolder && isExpConstOrIMM(expNode) && !lockingOpenFolder) {
                isLastValueGetableAssignOrDecl = true;
            }
            ident->initVal = std::make_shared<InitValValNode>(expNode);
        }
    } else {
        if (isGlobalBlock(ident->blockId)) {
            ident->globalVarInitVal = analyzeConstInitValArr(0, ident->numOfEachDimension);
        } else {
            ident->initVal = analyzeInitValArr(0, ident->numOfEachDimension);
        }
    }
}

/**
 * @deprecated
 */
std::shared_ptr<InitValArrNode> zeroDefaultArr(int dimension, std::vector<int> &numOfEachDimension) {
    int thisDimensionNum = numOfEachDimension[dimension];
    vector<shared_ptr<InitValNode>> valueList;
    if (dimension + 1 == numOfEachDimension.size()) {
        for (int i = 0; i < thisDimensionNum; i++) {
            int value = 0;
            auto exp = std::make_shared<PrimaryExpNode>(PrimaryExpNode::numberExp(value));
            auto expExp = s_p_c<ExpNode>(exp);
            valueList.push_back(std::make_shared<InitValValNode>(expExp));
        }
    } else {
        for (int i = 0; i < thisDimensionNum; i++) {
            valueList.push_back(zeroDefaultArr(dimension + 1, numOfEachDimension));
        }
    }
    return std::make_shared<InitValArrNode>(thisDimensionNum, valueList);
}

std::shared_ptr<InitValArrNode> analyzeInitValArr(int dimension, std::vector<int> &numOfEachDimension) {
    if (_debugSyntax) {
        std::cout << "--------analyzeInitValArr--------\n";
    }
    int thisDimensionNum = numOfEachDimension[dimension];
    vector<shared_ptr<InitValNode>> valueList;
    if (dimension + 1 == numOfEachDimension.size()) {
        if (nowPointerToken->getSym() == TokenType::LBRACE) {
            popNextLexer(); // LBRACE
            for (int i = 0; i < thisDimensionNum; i++) {
                if (nowPointerToken->getSym() == TokenType::RBRACE) {
                    break;
                } else {
                    auto exp = analyzeExp();
                    valueList.push_back(std::make_shared<InitValValNode>(exp));
                    if (nowPointerToken->getSym() == TokenType::COMMA) {
                        popNextLexer(); // COMMA
                    }
                }
            }
            popNextLexer(); // RBRACE
        } else {
            for (int i = 0; i < thisDimensionNum; i++) {
                if (nowPointerToken->getSym() == TokenType::LBRACE || nowPointerToken->getSym() == TokenType::RBRACE) {
                    break;
                }
                auto exp = analyzeExp();
                valueList.push_back(std::make_shared<InitValValNode>(exp));
                if (nowPointerToken->getSym() == TokenType::COMMA) {
                    popNextLexer(); // COMMA
                }
            }
        }
        return std::make_shared<InitValArrNode>(thisDimensionNum, valueList);
    }
    if (nowPointerToken->getSym() == TokenType::LBRACE) {
        popNextLexer(); // LBRACE
        for (int i = 0; i < thisDimensionNum; i++) {
            if (nowPointerToken->getSym() == TokenType::RBRACE) {
                break;
            } else {
                valueList.push_back(analyzeInitValArr(dimension + 1, numOfEachDimension));
                if (nowPointerToken->getSym() == TokenType::COMMA) {
                    popNextLexer(); // COMMA
                }
            }
        }
        popNextLexer(); // RBRACE
    } else {
        for (int i = 0; i < thisDimensionNum; i++) {
            valueList.push_back(analyzeInitValArr(dimension + 1, numOfEachDimension));
            if (nowPointerToken->getSym() == TokenType::COMMA) {
                popNextLexer(); // COMMA
            }
        }
    }
    return std::make_shared<InitValArrNode>(thisDimensionNum, valueList);
}

shared_ptr<FuncDefNode> analyzeFuncDef() {
    if (_debugSyntax) {
        std::cout << "--------analyzeFuncDef--------\n";
    }
    bool isVoid = nowPointerToken->getSym() == TokenType::VOID_TK;
    popNextLexer(); // VOID_TK || INT_TK
    FuncType funcType = isVoid ? FuncType::FUNC_VOID : FuncType::FUNC_INT;
    bool hasParams = false;
    auto ident = getIdentDefine(true, isVoid, false);
    popNextLexer(); // LPAREN
    shared_ptr<FuncFParamsNode> funcFParams;
    pair<int, int> fatherLayId = nowLayId;
    nowLayer++;
    nowLayId = distributeBlockId(nowLayer, fatherLayId);
    layIdInFuncFParams = nowLayId;
    if (nowPointerToken->getSym() != TokenType::RPAREN) {
        funcFParams = analyzeFuncFParams();
        hasParams = true;
    }
    popNextLexer(); // RPAREN
    nowLayer--;
    nowLayId = fatherLayId;
    auto block = analyzeBlock(true, isVoid, false);
    if (hasParams) {
        return std::make_shared<FuncDefNode>(funcType, ident, funcFParams, block);
    }
    return std::make_shared<FuncDefNode>(funcType, ident, block);
}

shared_ptr<FuncFParamsNode> analyzeFuncFParams() {
    if (_debugSyntax) {
        std::cout << "--------analyzeFuncFParams--------\n";
    }
    vector<shared_ptr<FuncFParamNode>> funcParamList;
    funcParamList.push_back(analyzeFuncFParam());
    while (nowPointerToken->getSym() == TokenType::COMMA) {
        popNextLexer(); // COMMA
        funcParamList.push_back(analyzeFuncFParam());
    }
    return std::make_shared<FuncFParamsNode>(funcParamList);
}

shared_ptr<FuncFParamNode> analyzeFuncFParam() {
    if (_debugSyntax) {
        std::cout << "--------analyzeFuncFParam--------\n";
    }
    popNextLexer(); // INT_TK
    auto ident = getIdentDefineInFunction();
    return std::make_shared<FuncFParamNode>(ident, ident->ident->dimension, ident->ident->numOfEachDimension);
}

/**
 *
 * @brief Get a block code
 * @details when get in a block, should notice blockId.
 * What need to do is add layer, and get a new layerId.
 * When get out of a block, set layerId to now layer's father.
 * @param isFuncBlock if isFuncBlock, then use layIdInFuncParam,
 * for it's in the same block. Don't generate a block again.
 * @return
 */
shared_ptr<BlockNode> analyzeBlock(bool isFuncBlock, bool isVoid, bool isInWhileFirstBlock) {
    if (_debugSyntax) {
        std::cout << "--------analyzeBlock--------\n";
    }
    pair<int, int> fatherLayId = nowLayId;
    nowLayer++;
    if (isFuncBlock) {
        nowLayId = layIdInFuncFParams;
    } else {
        nowLayId = distributeBlockId(nowLayer, fatherLayId);
    }
    int itemCnt = 0;
    vector<shared_ptr<BlockItemNode>> blockItems;
    popNextLexer(); // LBRACE
    while (nowPointerToken->getSym() != TokenType::RBRACE) {
        auto blockItem = analyzeBlockItem(isInWhileFirstBlock);
        if (openFolder && folderShouldBeOpen) {
            for (int i = 0; i < folderOpenNum; i++) {
                blockItems.push_back(blockItem);
                ++itemCnt;
            }
            folderShouldBeOpen = false;
        } else {
            blockItems.push_back(blockItem);
            ++itemCnt;
        }
    }
    if (isVoid) {
        auto defaultReturn = std::make_shared<StmtNode>(StmtNode::returnStmt());
        blockItems.push_back(defaultReturn);
    }
    popNextLexer(); // RBRACE
    nowLayer--;
    nowLayId = fatherLayId;
    return std::make_shared<BlockNode>(itemCnt, blockItems);
}

shared_ptr<BlockItemNode> analyzeBlockItem(bool isInWhileFirstBlock) {
    if (_debugSyntax) {
        std::cout << "--------analyzeBlockItem--------\n";
    }
    if (nowPointerToken->getSym() == TokenType::INT_TK || nowPointerToken->getSym() == TokenType::CONST_TK) {
        return analyzeDecl();
    } else {
        return analyzeStmt(isInWhileFirstBlock);
    }
}

bool isAssign() {
    int peekNum = 0;
    while (peekNextLexer(peekNum)->getSym() != TokenType::SEMICOLON) {
        if (peekNextLexer(peekNum)->getSym() == TokenType::ASSIGN) {
            return true;
        }
        peekNum++;
    }
    return false;
}

bool isSingleCondAddExpThisSymbol(std::shared_ptr<AddExpNode> addExp, std::shared_ptr<SymbolTableItem> symbol) {
    if (addExp->addExp) {
        return false;
    }
    auto mulExp = addExp->mulExp;
    if (mulExp->mulExp) {
        return false;
    }
    auto unaryExp = mulExp->unaryExp;
    if (unaryExp->type == UnaryExpType::UNARY_PRIMARY) {
        auto primaryExp = unaryExp->primaryExp;
        if (primaryExp->type == PrimaryExpType::PRIMARY_L_VAL) {
            auto lValExp = primaryExp->lVal;
            if (lValExp->ident->ident->symbolType == SymbolType::VAR) {
                if (lValExp->ident->ident->name == symbol->name) {
                    return true;
                }
            }
        }
    }
    return false;
}

static int whileControlVarEndValue = 0;
static TokenType relation = TokenType::EQUAL;

bool isCondSingleAndUseLastInitOrDeclVar(std::shared_ptr<CondNode> cond, std::shared_ptr<SymbolTableItem> symbol) {
    auto lOrExp = cond->lOrExp;
    if (lOrExp->lOrExp) {
        return false;
    }
    auto lAndExp = lOrExp->lAndExp;
    if (lAndExp->lAndExp) {
        return false;
    }
    auto eqExp = lAndExp->eqExp;
    if (eqExp->eqExp) {
        return false;
    }
    auto relExp = eqExp->relExp;
    if (relExp->relExp && relExp->addExp) {
        if (relExp->relExp->relExp) {
            return false;
        }
        if (!isSingleCondAddExpThisSymbol(relExp->relExp->addExp, symbol)) {
            return false;
        }
        if (!isExpConstOrIMM(relExp->addExp)) {
            return false;
        }
        whileControlVarEndValue = lastValue;
        return true;
    }
    return false;
}

/**
 *
 * @brief lVal has been judged
 * only whileControlVar lVal assignment should be record and use this judgement.
 * mulExp should be a "num".
 */
bool isAssignStepStyle(shared_ptr<StmtNode> assignStmt) {
    auto exp = assignStmt->exp;
    auto addExp = dynamic_pointer_cast<AddExpNode>(exp);
    bool isStepPlus = true;
    if (!addExp->addExp) {
        return false;
    }
    if (addExp->op == "-") {
        isStepPlus = !isStepPlus;
    }
    auto mulExp = addExp->mulExp;
    addExp = addExp->addExp;
    if (mulExp->mulExp) {
        return false;
    }
    auto unaryExp = mulExp->unaryExp;
    while (unaryExp->type == UnaryExpType::UNARY_DEEP) {
        if (unaryExp->op == "-") {
            isStepPlus = !isStepPlus;
            unaryExp = unaryExp->unaryExp;
        }
    }
    if (unaryExp->type != UnaryExpType::UNARY_PRIMARY) {
        return false;
    }
    auto primaryExp = unaryExp->primaryExp;
    if (primaryExp->type == PrimaryExpType::PRIMARY_NUMBER) {
        step = isStepPlus ? primaryExp->number : -primaryExp->number;
    } else {
        return false;
    }
    if (addExp->addExp) {
        return false;
    }
    mulExp = addExp->mulExp;
    if (mulExp->mulExp) {
        return false;
    }
    unaryExp = mulExp->unaryExp;
    if (unaryExp->type != UnaryExpType::UNARY_PRIMARY) {
        return false;
    }
    primaryExp = unaryExp->primaryExp;
    return primaryExp->type == PrimaryExpType::PRIMARY_L_VAL
           && primaryExp->lVal->ident->ident->usageName == lastAssignOrDeclVar->usageName;
}

shared_ptr<StmtNode> analyzeWhile() {
    int whileControlStartValue = lastValue;
    popNextLexer(); // While
    popNextLexer();
    if (openFolder) {
        isInWhileLockedCond = true;
    }
    auto cond = analyzeCond();
    popNextLexer();
    shared_ptr<StmtNode> whileInsideStmt;
    if (openFolder && !lockingOpenFolder && isLastValueGetableAssignOrDecl) {
        isLastValueGetableAssignOrDecl = false;
        auto whileControlVar = lastAssignOrDeclVar;
        lockingOpenFolder = true;
        if (isCondSingleAndUseLastInitOrDeclVar(cond, whileControlVar)) {
            whileControlVarAssignNum = 0;
            isInDirectWhile = true;
            whileInsideStmt = analyzeStmt(true);
            if (whileControlVarAssignNum == 1) {
                if (isAssignStepStyle(whileBlockAssignStmt)) {
                    if (relation == TokenType::LARGE || relation == TokenType::LEQ || relation == TokenType::LESS ||
                        relation == TokenType::LAQ) {
                        if (relation == TokenType::LARGE) {
                            if (whileControlStartValue <= whileControlVarEndValue) {
                                folderOpenNum = 0;
                                return whileInsideStmt;
                            } else {
                                folderOpenNum = (whileControlVarEndValue - whileControlStartValue) / step;
                            }
                        }
                        if (relation == TokenType::LAQ) {
                            if (whileControlStartValue < whileControlVarEndValue) {
                                folderOpenNum = 0;
                                return whileInsideStmt;
                            } else {
                                folderOpenNum = (whileControlVarEndValue - whileControlStartValue - 1) / step;
                            }
                        }
                        if (relation == TokenType::LESS) {
                            if (whileControlStartValue > whileControlVarEndValue) {
                                folderOpenNum = 0;
                                return whileInsideStmt;
                            } else {
                                folderOpenNum = (whileControlVarEndValue - whileControlStartValue) / step;
                            }
                        }
                        if (relation == TokenType::LEQ) {
                            if (whileControlStartValue >= whileControlVarEndValue) {
                                folderOpenNum = 0;
                                return whileInsideStmt;
                            } else {
                                folderOpenNum = (whileControlVarEndValue - whileControlStartValue + 1) / step;
                            }
                        }
                        isInDirectWhile = false;
                        lockingOpenFolder = false;
                        if (folderOpenNum >= 0 && folderOpenNum < 1000) {
                            folderShouldBeOpen = true;
                            return whileInsideStmt;
                        }
                    }
                }
            }
            isInDirectWhile = false;
            lockingOpenFolder = false;
            return std::make_shared<StmtNode>(StmtNode::whileStmt(cond, whileInsideStmt));
        }
        lockingOpenFolder = false;
    }
    auto tmp = isInDirectWhile;
    if (openFolder) {
        isLastValueGetableAssignOrDecl = false;
        isInDirectWhile = false;
    }
    whileInsideStmt = analyzeStmt(false);
    if (openFolder) {
        isInDirectWhile = tmp;
    }
    return std::make_shared<StmtNode>(StmtNode::whileStmt(cond, whileInsideStmt));
}


shared_ptr<StmtNode> analyzeStmt(bool isInWhileFirstBlock) {
    if (_debugSyntax) {
        std::cout << "--------analyzeStmt--------\n";
    }
    if (nowPointerToken->getSym() == TokenType::LBRACE) {
        if (openFolder) {
            isLastValueGetableAssignOrDecl = false;
        }
        auto blockNode = analyzeBlock(false, false, isInWhileFirstBlock);
        return std::make_shared<StmtNode>(StmtNode::blockStmt(blockNode));
    } else if (nowPointerToken->getSym() == TokenType::BREAK_TK) {
        if (openFolder) {
            isLastValueGetableAssignOrDecl = false;
        }
        if (openFolder && isInWhileFirstBlock) {
            whileControlVarAssignNum = 2;
        }
        popNextLexer();
        popNextLexer();
        return std::make_shared<StmtNode>(StmtNode::breakStmt());
    } else if (nowPointerToken->getSym() == TokenType::CONTINUE_TK) {
        if (openFolder) {
            isLastValueGetableAssignOrDecl = false;
        }
        if (openFolder && isInWhileFirstBlock) {
            whileControlVarAssignNum = 2;
        }
        popNextLexer();
        popNextLexer();
        return std::make_shared<StmtNode>(StmtNode::continueStmt());
    } else if (nowPointerToken->getSym() == TokenType::IF_TK) {
        if (openFolder) {
            isLastValueGetableAssignOrDecl = false;
        }
        popNextLexer();
        popNextLexer(); // LPAREN
        auto cond = analyzeCond();
        popNextLexer(); // RPAREN
        auto tmp = isInDirectWhile;
        if (openFolder) {
            isInDirectWhile = false;
        }
        auto ifBranchStmt = analyzeStmt(isInWhileFirstBlock);
        if (nowPointerToken->getSym() == TokenType::ELSE_TK) {
            popNextLexer();
            auto elseBranchStmt = analyzeStmt(isInWhileFirstBlock);
            if (openFolder) {
                isInDirectWhile = tmp;
            }
            return std::make_shared<StmtNode>(StmtNode::ifStmt(cond, ifBranchStmt, elseBranchStmt));
        }
        if (openFolder) {
            isInDirectWhile = tmp;
        }
        return std::make_shared<StmtNode>(StmtNode::ifStmt(cond, ifBranchStmt));
    } else if (nowPointerToken->getSym() == TokenType::WHILE_TK) {
        return analyzeWhile();
    } else if (nowPointerToken->getSym() == TokenType::RETURN_TK) {
        if (openFolder) {
            isLastValueGetableAssignOrDecl = false;
        }
        popNextLexer();
        if (nowPointerToken->getSym() == TokenType::SEMICOLON) {
            popNextLexer();
            return std::make_shared<StmtNode>(StmtNode::returnStmt());
        }
        auto exp = analyzeExp();
        popNextLexer();
        return std::make_shared<StmtNode>(StmtNode::returnStmt(exp));
    } else if (nowPointerToken->getSym() == TokenType::SEMICOLON) {
        popNextLexer();
        return std::make_shared<StmtNode>(StmtNode::emptyStmt());
    } else if (isAssign()) {
        if (openFolder && !lockingOpenFolder) {
            getAssignLVal = true;
            isLastValueGetableAssignOrDecl = true;
        }
        if (openFolder && lockingOpenFolder) {
            judgeWhetherAssignLValWhileControlVar = true;
        }
        auto lVal = analyzeLVal();
        if (openFolder && lockingOpenFolder) {
            judgeWhetherAssignLValWhileControlVar = false;
            if (isAssignLValWhileControlVar) {
                whileControlVarAssignNum += 1;
                if (!isInDirectWhile) {
                    whileControlVarAssignNum += 1;
                }
            }
        }
        popNextLexer(); // ASSIGN
        auto exp = analyzeExp();
        if (openFolder && !lockingOpenFolder) {
            if (!isExpConstOrIMM(exp)) {
                isLastValueGetableAssignOrDecl = false;
            }
        }
        popNextLexer();
        auto assignStmt = std::make_shared<StmtNode>(StmtNode::assignStmt(lVal, exp));
        if (openFolder && lockingOpenFolder && isAssignLValWhileControlVar) {
            whileBlockAssignStmt = assignStmt;
        }
        return assignStmt;
    } else {
        if (openFolder) {
            isLastValueGetableAssignOrDecl = false;
        }
        auto exp = analyzeExp();
        popNextLexer();
        return std::make_shared<StmtNode>(StmtNode::expStmt(exp));
    }
}

/**
 * @brief Exp -> AddExp
 * @details Calling Exp, return AddExp.
 * @return
 */
std::shared_ptr<ExpNode> analyzeExp() {
    if (_debugSyntax) {
        std::cout << "--------analyzeExp--------\n";
    }
    return analyzeAddExp();
}

std::shared_ptr<AddExpNode> analyzeAddExp() {
    if (_debugSyntax) {
        std::cout << "--------analyzeAddExp--------\n";
    }
    auto mulExp = analyzeMulExp();
    auto addExp = std::make_shared<AddExpNode>(mulExp);
    while (nowPointerToken->getSym() == TokenType::MINUS || nowPointerToken->getSym() == TokenType::PLUS) {
        string op = nowPointerToken->getSym() == TokenType::MINUS ? "-" : "+";
        popNextLexer();
        mulExp = analyzeMulExp();
        addExp = std::make_shared<AddExpNode>(mulExp, addExp, op);
    }
    return addExp;
}

std::shared_ptr<MulExpNode> analyzeMulExp() {
    if (_debugSyntax) {
        std::cout << "--------analyzeMulExp--------\n";
    }
    auto unaryExp = analyzeUnaryExp();
    auto relExp = std::make_shared<MulExpNode>(unaryExp);
    while (nowPointerToken->getSym() == TokenType::MULT || nowPointerToken->getSym() == TokenType::DIV ||
           nowPointerToken->getSym() == TokenType::REMAIN) {
        string op = nowPointerToken->getSym() == TokenType::MULT ? "*" :
                    nowPointerToken->getSym() == TokenType::DIV ? "/" : "%";
        popNextLexer();
        unaryExp = analyzeUnaryExp();
        relExp = std::make_shared<MulExpNode>(unaryExp, relExp, op);
    }
    return relExp;
}

std::shared_ptr<UnaryExpNode> analyzeUnaryExp() {
    if (_debugSyntax) {
        std::cout << "--------analyzeUnaryExp--------\n";
    }
    if (nowPointerToken->getSym() == TokenType::IDENT && peekNextLexer(1)->getSym() == TokenType::LPAREN) {
        auto ident = getIdentUsage(true);
        string op = "+";
        popNextLexer(); // LPAREN
        if (nowPointerToken->getSym() == TokenType::RPAREN) {
            popNextLexer();
            return std::make_shared<UnaryExpNode>(UnaryExpNode::funcCallUnaryExp(ident, op));
        }
        auto funcRParams = analyzeFuncRParams();
        popNextLexer(); // RPAREN
        return std::make_shared<UnaryExpNode>(UnaryExpNode::funcCallUnaryExp(ident, funcRParams, op));
    } else if (nowPointerToken->getSym() == TokenType::PLUS || nowPointerToken->getSym() == TokenType::MINUS ||
               nowPointerToken->getSym() == TokenType::NOT) {
        string op = nowPointerToken->getSym() == TokenType::PLUS ? "+" :
                    nowPointerToken->getSym() == TokenType::MINUS ? "-" : "!";
        popNextLexer();
        auto unaryExp = analyzeUnaryExp();
        return std::make_shared<UnaryExpNode>(UnaryExpNode::unaryUnaryExp(unaryExp, op));
    } else {
        string op = "+";
        auto primaryExp = analyzePrimaryExp();
        return std::make_shared<UnaryExpNode>(UnaryExpNode::primaryUnaryExp(primaryExp, op));
    }
}

std::shared_ptr<PrimaryExpNode> analyzePrimaryExp() {
    if (_debugSyntax) {
        std::cout << "--------analyzePrimaryExp--------\n";
    }
    if (nowPointerToken->getSym() == TokenType::LPAREN) {
        popNextLexer(); // LPAREN
        auto exp = analyzeExp();
        popNextLexer(); // RPAREN
        return std::make_shared<PrimaryExpNode>(PrimaryExpNode::parentExp(exp));
    } else if (nowPointerToken->getSym() == TokenType::INTCONST) {
        int value = nowPointerToken->getValue();
        popNextLexer();
        return std::make_shared<PrimaryExpNode>(PrimaryExpNode::numberExp(value));
    } else {
        auto lVal = analyzeLVal();
        return std::make_shared<PrimaryExpNode>(PrimaryExpNode::lValExp(lVal));
    }
}

std::shared_ptr<LValNode> analyzeLVal() {
    if (_debugSyntax) {
        std::cout << "--------analyzeLVal--------\n";
    }
    auto ident = getIdentUsage(false);
    if (ident->ident->dimension == 0) {
        return std::make_shared<LValNode>(ident);
    }
    return std::make_shared<LValNode>(ident, ident->ident->dimension, ident->ident->expressionOfEachDimension);
}

std::shared_ptr<CompUnitNode> analyzeCompUnit() {
    if (_debugSyntax) {
        std::cout << "--------analyzeCompUnit--------\n";
    }
    vector<shared_ptr<DeclNode>> declList;
    vector<shared_ptr<FuncDefNode>> funcDefList;
    while (nowPointerToken->getSym() != TokenType::END) {
        if (peekNextLexer(2)->getSym() == TokenType::LPAREN) {
            funcDefList.push_back(analyzeFuncDef());
        } else {
            declList.push_back(analyzeDecl());
        }
    }
    for (const auto &symbolTableItem : symbolTable[{0, 1}]->symbolTableThisBlock) {
        if (symbolTableItem.second->isVarSingleUseInUnRecursionFunction()) {
            usageNameListOfVarSingleUseInUnRecursionFunction[symbolTableItem.second->usageName] = symbolTableItem.second->eachFunc[0]->usageName;
        }
    }
    return std::make_shared<CompUnitNode>(declList, funcDefList);
}

std::shared_ptr<FuncRParamsNode> analyzeFuncRParams() {
    if (_debugSyntax) {
        std::cout << "--------analyzeFuncRParams--------\n";
    }
    vector<shared_ptr<ExpNode>> exps;
    auto exp = analyzeExp();
    exps.push_back(exp);
    while (nowPointerToken->getSym() == TokenType::COMMA) {
        popNextLexer(); // COMMA
        exp = analyzeExp();
        exps.push_back(exp);
    }
    return std::make_shared<FuncRParamsNode>(exps);
}

std::shared_ptr<CondNode> analyzeCond() {
    if (_debugSyntax) {
        std::cout << "--------analyzeCond--------\n";
    }
    auto lOrExp = analyzeLOrExp();
    auto condExp = std::make_shared<CondNode>(lOrExp);
    if (openFolder) {
        changeCondDivideIntoMul(condExp);
    }
    return condExp;
}

std::shared_ptr<LOrExpNode> analyzeLOrExp() {
    if (_debugSyntax) {
        std::cout << "--------analyzeLOrExp--------\n";
    }
    auto lAndExp = analyzeLAndExp();
    auto lOrExp = std::make_shared<LOrExpNode>(lAndExp);
    while (nowPointerToken->getSym() == TokenType::OR) {
        string op = "||";
        popNextLexer();
        lAndExp = analyzeLAndExp();
        lOrExp = std::make_shared<LOrExpNode>(lAndExp, lOrExp, op);
    }
    return lOrExp;
}

std::shared_ptr<LAndExpNode> analyzeLAndExp() {
    if (_debugSyntax) {
        std::cout << "--------analyzeLAndExp--------\n";
    }
    auto eqExp = analyzeEqExp();
    auto lAndExp = std::make_shared<LAndExpNode>(eqExp);
    while (nowPointerToken->getSym() == TokenType::AND) {
        string op = "&&";
        popNextLexer();
        eqExp = analyzeEqExp();
        lAndExp = std::make_shared<LAndExpNode>(eqExp, lAndExp, op);
    }
    return lAndExp;
}

std::shared_ptr<EqExpNode> analyzeEqExp() {
    if (_debugSyntax) {
        std::cout << "--------analyzeEqExp--------\n";
    }
    auto relExp = analyzeRelExp();
    auto eqExp = std::make_shared<EqExpNode>(relExp);
    while (nowPointerToken->getSym() == TokenType::EQUAL || nowPointerToken->getSym() == TokenType::NEQUAL) {
        string op = nowPointerToken->getSym() == TokenType::EQUAL ? "==" : "!=";
        popNextLexer();
        relExp = analyzeRelExp();
        eqExp = std::make_shared<EqExpNode>(relExp, eqExp, op);
    }
    return eqExp;
}

std::shared_ptr<RelExpNode> analyzeRelExp() {
    if (_debugSyntax) {
        std::cout << "--------analyzeRelExp--------\n";
    }
    auto addExp = analyzeAddExp();
    auto relExp = std::make_shared<RelExpNode>(addExp);
    while (nowPointerToken->getSym() == TokenType::LARGE || nowPointerToken->getSym() == TokenType::LAQ ||
           nowPointerToken->getSym() == TokenType::LESS || nowPointerToken->getSym() == TokenType::LEQ) {
        if (openFolder && isInWhileLockedCond) {
            relation = nowPointerToken->getSym();
            isInWhileLockedCond = false;
        }
        string op = nowPointerToken->getSym() == TokenType::LARGE ? ">" :
                    nowPointerToken->getSym() == TokenType::LAQ ? ">=" :
                    nowPointerToken->getSym() == TokenType::LESS ? "<" : "<=";
        popNextLexer();
        addExp = analyzeAddExp();
        relExp = std::make_shared<RelExpNode>(addExp, relExp, op);
    }
    return relExp;
}

std::shared_ptr<CompUnitNode> syntaxAnalyze() {
    nowPointerToken = &tokenInfoList[0];
    auto retFuncType = SymbolType::RET_FUNC;
    auto voidFuncType = SymbolType::VOID_FUNC;
    string retNames[] = {"getint", "getch", "getarray"};
    string voidNames[] = {"putint", "putch", "putarray", "putf", "starttime", "stoptime"};
    for (string &retName : retNames) {
        auto retFunc = std::make_shared<SymbolTableItem>(retFuncType, retName, nowLayId);
        insertSymbol(nowLayId, retFunc);
    }
    for (string &voidName : voidNames) {
        auto voidFunc = std::make_shared<SymbolTableItem>(voidFuncType, voidName, nowLayId);
        insertSymbol(nowLayId, voidFunc);
    }
    return analyzeCompUnit();
}
