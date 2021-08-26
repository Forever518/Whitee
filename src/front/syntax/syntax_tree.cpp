#include "syntax_tree.h"

// Definition of StmtNode constructors.
StmtNode StmtNode::assignStmt(shared_ptr<LValNode> &lVal, shared_ptr<ExpNode> &exp) {
    shared_ptr<BlockNode> block(nullptr);
    shared_ptr<CondNode> cond(nullptr);
    shared_ptr<StmtNode> stmt(nullptr);
    return StmtNode(StmtType::STMT_ASSIGN, lVal, exp, block, cond, stmt, stmt);
}

StmtNode StmtNode::emptyStmt() {
    return StmtNode();
}

StmtNode StmtNode::blockStmt(shared_ptr<BlockNode> &block) {
    shared_ptr<LValNode> lVal(nullptr);
    shared_ptr<ExpNode> exp(nullptr);
    shared_ptr<CondNode> cond(nullptr);
    shared_ptr<StmtNode> stmt(nullptr);
    return StmtNode(StmtType::STMT_BLOCK, lVal, exp, block, cond, stmt, stmt);
}

StmtNode StmtNode::expStmt(shared_ptr<ExpNode> &exp) {
    shared_ptr<LValNode> lVal(nullptr);
    shared_ptr<BlockNode> block(nullptr);
    shared_ptr<CondNode> cond(nullptr);
    shared_ptr<StmtNode> stmt(nullptr);
    return StmtNode(StmtType::STMT_EXP, lVal, exp, block, cond, stmt, stmt);
}

StmtNode StmtNode::ifStmt(shared_ptr<CondNode> &cond, shared_ptr<StmtNode> &stmt, shared_ptr<StmtNode> &elseStmt) {
    shared_ptr<LValNode> lVal(nullptr);
    shared_ptr<ExpNode> exp(nullptr);
    shared_ptr<BlockNode> block(nullptr);
    return StmtNode(StmtType::STMT_IF_ELSE, lVal, exp, block, cond, stmt, elseStmt);
}

StmtNode StmtNode::ifStmt(shared_ptr<CondNode> &cond, shared_ptr<StmtNode> &stmt) {
    shared_ptr<LValNode> lVal(nullptr);
    shared_ptr<ExpNode> exp(nullptr);
    shared_ptr<BlockNode> block(nullptr);
    shared_ptr<StmtNode> elseStmt(nullptr);
    return StmtNode(StmtType::STMT_IF, lVal, exp, block, cond, stmt, elseStmt);
}

StmtNode StmtNode::whileStmt(shared_ptr<CondNode> &cond, shared_ptr<StmtNode> &stmt) {
    shared_ptr<LValNode> lVal(nullptr);
    shared_ptr<ExpNode> exp(nullptr);
    shared_ptr<BlockNode> block(nullptr);
    shared_ptr<StmtNode> elseStmt(nullptr);
    return StmtNode(StmtType::STMT_WHILE, lVal, exp, block, cond, stmt, elseStmt);
}

StmtNode StmtNode::breakStmt() {
    shared_ptr<LValNode> lVal(nullptr);
    shared_ptr<ExpNode> exp(nullptr);
    shared_ptr<BlockNode> block(nullptr);
    shared_ptr<CondNode> cond(nullptr);
    shared_ptr<StmtNode> stmt(nullptr);
    return StmtNode(StmtType::STMT_BREAK, lVal, exp, block, cond, stmt, stmt);
}

StmtNode StmtNode::continueStmt() {
    shared_ptr<LValNode> lVal(nullptr);
    shared_ptr<ExpNode> exp(nullptr);
    shared_ptr<BlockNode> block(nullptr);
    shared_ptr<CondNode> cond(nullptr);
    shared_ptr<StmtNode> stmt(nullptr);
    return StmtNode(StmtType::STMT_CONTINUE, lVal, exp, block, cond, stmt, stmt);
}

StmtNode StmtNode::returnStmt(shared_ptr<ExpNode> &exp) {
    shared_ptr<LValNode> lVal(nullptr);
    shared_ptr<BlockNode> block(nullptr);
    shared_ptr<CondNode> cond(nullptr);
    shared_ptr<StmtNode> stmt(nullptr);
    return StmtNode(StmtType::STMT_RETURN, lVal, exp, block, cond, stmt, stmt);
}

StmtNode StmtNode::returnStmt() {
    shared_ptr<LValNode> lVal(nullptr);
    shared_ptr<BlockNode> block(nullptr);
    shared_ptr<CondNode> cond(nullptr);
    shared_ptr<StmtNode> stmt(nullptr);
    shared_ptr<ExpNode> exp(nullptr);
    return StmtNode(StmtType::STMT_RETURN_VOID, lVal, exp, block, cond, stmt, stmt);
}

string StmtNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "Stmt ";
    switch (type) {
        case StmtType::STMT_EMPTY:
            s += "Empty\n";
            break;
        case StmtType::STMT_RETURN_VOID:
            s += "Return Void\n";
            break;
        case StmtType::STMT_RETURN:
            s += "Return\n" + exp->toString(tabCnt + 1);
            break;
        case StmtType::STMT_CONTINUE:
            s += "Continue\n";
            break;
        case StmtType::STMT_BREAK:
            s += "Break\n";
            break;
        case StmtType::STMT_WHILE:
            s += "While\n" + cond->toString(tabCnt + 1) + stmt->toString(tabCnt + 1);
            break;
        case StmtType::STMT_IF:
            s += "If\n" + cond->toString(tabCnt + 1) + stmt->toString(tabCnt + 1);
            break;
        case StmtType::STMT_IF_ELSE:
            s += "If Else\n" + cond->toString(tabCnt + 1) + stmt->toString(tabCnt + 1) + elseStmt->toString(tabCnt + 1);
            break;
        case StmtType::STMT_EXP:
            s += "Exp\n" + exp->toString(tabCnt + 1);
            break;
        case StmtType::STMT_BLOCK:
            s += "Block\n" + block->toString(tabCnt + 1);
            break;
        default:
            s += "Assign\n" + lVal->toString(tabCnt + 1) + exp->toString(tabCnt + 1);
    }
    return s;
}

// Definitions of UnaryExp constructors.
UnaryExpNode UnaryExpNode::primaryUnaryExp(shared_ptr<PrimaryExpNode> &primaryExp, string &op) {
    shared_ptr<IdentNode> ident(nullptr);
    shared_ptr<FuncRParamsNode> funcRParams(nullptr);
    shared_ptr<UnaryExpNode> unaryExp(nullptr);
    return UnaryExpNode(UnaryExpType::UNARY_PRIMARY, primaryExp, ident, funcRParams, op, unaryExp);
}

UnaryExpNode UnaryExpNode::funcCallUnaryExp(shared_ptr<IdentNode> &ident,
                                            shared_ptr<FuncRParamsNode> &funcRParams, string &op) {
    shared_ptr<PrimaryExpNode> primaryExp(nullptr);
    shared_ptr<UnaryExpNode> unaryExp(nullptr);
    return UnaryExpNode(UnaryExpType::UNARY_FUNC, primaryExp, ident, funcRParams, op, unaryExp);
}

UnaryExpNode UnaryExpNode::funcCallUnaryExp(shared_ptr<IdentNode> &ident, string &op) {
    shared_ptr<PrimaryExpNode> primaryExp(nullptr);
    shared_ptr<UnaryExpNode> unaryExp(nullptr);
    shared_ptr<FuncRParamsNode> funcRParams(nullptr);
    return UnaryExpNode(UnaryExpType::UNARY_FUNC_NON_PARAM, primaryExp, ident, funcRParams, op, unaryExp);
}

UnaryExpNode UnaryExpNode::unaryUnaryExp(shared_ptr<UnaryExpNode> &unaryExp, string &op) {
    shared_ptr<IdentNode> ident(nullptr);
    shared_ptr<FuncRParamsNode> funcRParams(nullptr);
    shared_ptr<PrimaryExpNode> primaryExp(nullptr);
    return UnaryExpNode(UnaryExpType::UNARY_DEEP, primaryExp, ident, funcRParams, op, unaryExp);
}

string UnaryExpNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "UnaryExp ";
    switch (type) {
        case UnaryExpType::UNARY_PRIMARY:
            s += "Primary\n" + primaryExp->toString(tabCnt + 1);
            break;
        case UnaryExpType::UNARY_FUNC:
            s += "Func Call\n" + ident->toString(tabCnt + 1) + funcRParams->toString(tabCnt + 1);
            break;
        case UnaryExpType::UNARY_FUNC_NON_PARAM:
            s += "Func Call Non-Param\n" + ident->toString(tabCnt + 1);
            break;
        default:
            s += "Deep Exp " + op + "\n" + unaryExp->toString(tabCnt + 1);
    }
    return s;
}

// Definitions of PrimaryExp constructors.
PrimaryExpNode PrimaryExpNode::parentExp(shared_ptr<ExpNode> &exp) {
    shared_ptr<LValNode> lVal(nullptr);
    int number = 0;
    string str;
    return PrimaryExpNode(PrimaryExpType::PRIMARY_PARENT_EXP, exp, lVal, number, str);
}

PrimaryExpNode PrimaryExpNode::lValExp(shared_ptr<LValNode> &lVal) {
    shared_ptr<ExpNode> exp(nullptr);
    int number = 0;
    string str;
    if (lVal->ident->ident->symbolType == SymbolType::CONST_VAR) {
        shared_ptr<ConstInitValValNode> val = s_p_c<ConstInitValValNode>(
                lVal->ident->ident->constInitVal);
        number = val->value;
        return PrimaryExpNode(PrimaryExpType::PRIMARY_NUMBER, exp, lVal, number, str);
    }
    return PrimaryExpNode(PrimaryExpType::PRIMARY_L_VAL, exp, lVal, number, str);
}

PrimaryExpNode PrimaryExpNode::numberExp(int &number) {
    shared_ptr<ExpNode> exp(nullptr);
    shared_ptr<LValNode> lVal(nullptr);
    string str;
    return PrimaryExpNode(PrimaryExpType::PRIMARY_NUMBER, exp, lVal, number, str);
}

PrimaryExpNode PrimaryExpNode::stringExp(string &str) {
    shared_ptr<ExpNode> exp(nullptr);
    shared_ptr<LValNode> lVal(nullptr);
    int number = 0;
    return PrimaryExpNode(PrimaryExpType::PRIMARY_STRING, exp, lVal, number, str);
}

string PrimaryExpNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "PrimaryExp\n";
    switch (type) {
        case PrimaryExpType::PRIMARY_L_VAL:
            s += lVal->toString(tabCnt + 1);
            break;
        case PrimaryExpType::PRIMARY_PARENT_EXP:
            s += exp->toString(tabCnt + 1);
            break;
        default:
            s += string((tabCnt + 1) * _W_LEN, ' ') + to_string(number) + "\n";
    }
    return s;
}

// Definition of symbol table item.
SymbolTableItem::SymbolTableItem(SymbolType &symbolType, int &dimension,
                                 std::vector<std::shared_ptr<ExpNode>> &expressionOfEachDimension, std::string &name,
                                 std::pair<int, int> &blockId) : symbolType(symbolType), dimension(dimension),
                                                                 expressionOfEachDimension(expressionOfEachDimension),
                                                                 name(name),
                                                                 blockId(blockId) {
    this->usageName =
            ((this->symbolType == SymbolType::VOID_FUNC || this->symbolType == SymbolType::RET_FUNC) ? "F_" :
             "V_") + std::to_string(blockId.first) + '_' + std::to_string(blockId.second) + '_' + name;
    this->uniqueName =
            ((this->symbolType == SymbolType::VOID_FUNC || this->symbolType == SymbolType::RET_FUNC) ? "F$" :
             "V$") + name;
}

SymbolTableItem::SymbolTableItem(SymbolType &symbolType, std::string &name, std::pair<int, int> &blockId) :
        symbolType(symbolType), name(name), blockId(blockId) {
    this->dimension = 0;
    this->usageName =
            ((this->symbolType == SymbolType::VOID_FUNC || this->symbolType == SymbolType::RET_FUNC) ? "F_" :
             "V_") + std::to_string(blockId.first) + "_" + std::to_string(blockId.second) + '_' + name;
    this->uniqueName =
            ((this->symbolType == SymbolType::VOID_FUNC || this->symbolType == SymbolType::RET_FUNC) ? "F*" :
             "V*") + name;
    if (symbolType == SymbolType::RET_FUNC && name == "main") {
        this->usageName = "main";
        this->uniqueName = "main";
    }
};

SymbolTableItem::SymbolTableItem(SymbolType &symbolType, int &dimension,
                                 std::vector<int> &numOfEachDimension, std::string &name,
                                 std::pair<int, int> &blockId) : symbolType(symbolType), dimension(dimension),
                                                                 numOfEachDimension(numOfEachDimension),
                                                                 name(name),
                                                                 blockId(blockId) {
    this->usageName =
            ((this->symbolType == SymbolType::VOID_FUNC || this->symbolType == SymbolType::RET_FUNC) ? "F_" :
             "V_") + std::to_string(blockId.first) + "_" + std::to_string(blockId.second) + '_' + name;
    this->uniqueName =
            ((this->symbolType == SymbolType::VOID_FUNC || this->symbolType == SymbolType::RET_FUNC) ? "F*" :
             "V*") + name;
}

bool SymbolTableItem::isVarSingleUseInUnRecursionFunction() {
    if (this->symbolType != SymbolType::VAR) {
        return false;
    }
    if (this->eachFunc.size() > 1 || this->eachFunc.empty()) {
        return false;
    }
    return !this->eachFunc[0]->isRecursion;
}

// Definition of toString.
string CompUnitNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "[AST]\nCompUnit\n";
    for (const auto &decl : declList) {
        s += decl->toString(tabCnt + 1);
    }
    for (const auto &func : funcDefList) {
        s += func->toString(tabCnt + 1);
    }
    return s;
}

string BlockNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "Block with item count " + to_string(itemCnt) + "\n";
    for (const auto &item : blockItems) {
        s += item->toString(tabCnt + 1);
    }
    return s;
}

string ConstDeclNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "ConstDecl\n";
    for (const auto &constant : constDefList) {
        s += constant->toString(tabCnt + 1);
    }
    return s;
}

string VarDeclNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "ValDecl\n";
    for (const auto &var : varDefList) {
        s += var->toString(tabCnt + 1);
    }
    return s;
}

string ConstDefNode::toString(int tabCnt) {
    return string(tabCnt * _W_LEN, ' ') + "ConstDef\n" + ident->toString(tabCnt + 1)
           + constInitVal->toString(tabCnt + 1);
}

string ConstInitValValNode::toString(int tabCnt) {
    return string(tabCnt * _W_LEN, ' ') + "ConstInitValVal\n" + string((tabCnt + 1) * _W_LEN, ' ') + to_string(value) +
           "\n";
}

vector<pair<int, int>> ConstInitValValNode::toOneDimensionArray(int start, int size) {
    return vector<pair<int, int>>(1, {start, value});
}

string ConstInitValArrNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "ConstInitValArr\n";
    for (const auto &node : valList) {
        s += node->toString(tabCnt + 1);
    }
    return s;
}

vector<pair<int, int>> ConstInitValArrNode::toOneDimensionArray(int start, int size) {
    vector<pair<int, int>> re;
    for (const auto &val : valList) {
        vector<pair<int, int>> tempVector = val->toOneDimensionArray(start, size / expectedSize);
        re.insert(re.end(), tempVector.begin(), tempVector.end());
        start += size / expectedSize;
    }
    return re;
}

string VarDefNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "VarDef\n" + ident->toString(tabCnt + 1);
    if (dimension != 0) {
        s += string((tabCnt + 1) * _W_LEN, ' ') + "dimensions: ";
        for (const int d : dimensions) {
            s += to_string(d) + " ";
        }
        s += "\n";
    }
    if (type == InitType::INIT) return s + initVal->toString(tabCnt + 1);
    if (ident->ident->blockId.first == 0) {
        if (ident->ident->globalVarInitVal) {
            return s + ident->ident->globalVarInitVal->toString(tabCnt + 1);
        } else return s + string((tabCnt + 1) * _W_LEN, ' ') + "default 0\n";
    }
    return s;
}

string InitValValNode::toString(int tabCnt) {
    return string(tabCnt * _W_LEN, ' ') + "InitValVal\n" + exp->toString(tabCnt + 1);
}

vector<pair<int, shared_ptr<ExpNode>>> InitValValNode::toOneDimensionArray(int start, int size) {
    return vector<pair<int, shared_ptr<ExpNode>>>(1, {start, exp});
}

string InitValArrNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "InitValArr\n";
    for (const auto &node : valList) {
        s += node->toString(tabCnt + 1);
    }
    return s;
}

vector<pair<int, shared_ptr<ExpNode>>> InitValArrNode::toOneDimensionArray(int start, int size) {
    vector<pair<int, shared_ptr<ExpNode>>> re;
    for (const auto &val : valList) {
        vector<pair<int, shared_ptr<ExpNode>>> tempVector = val->toOneDimensionArray(start, size / expectedSize);
        re.insert(re.end(), tempVector.begin(), tempVector.end());
        start += size / expectedSize;
    }
    return re;
}

string FuncDefNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "FuncDef ";
    switch (funcType) {
        case FuncType::FUNC_INT:
            s += "int\n";
            break;
        default:
            s += "void\n";
    }
    s += ident->toString(tabCnt + 1);
    if (funcFParams != nullptr) {
        s += funcFParams->toString(tabCnt + 1);
    }
    return s + block->toString(tabCnt + 1);
}

string FuncFParamsNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "FuncFParams\n";
    for (const shared_ptr<FuncFParamNode> &node : funcParamList) {
        s += node->toString(tabCnt + 1);
    }
    return s;
}

string FuncFParamNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "FuncFParam\n";
    s += ident->toString(tabCnt + 1);
    if (dimension != 0) {
        s += string((tabCnt + 1) * _W_LEN, ' ') + "dimensions: ";
        for (const int d : dimensions) {
            s += to_string(d) + " ";
        }
        s += "\n";
    }
    return s;
}

string CondNode::toString(int tabCnt) {
    return string(tabCnt * _W_LEN, ' ') + "Cond\n" + lOrExp->toString(tabCnt + 1);
}

string LValNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "LVal\n" + ident->toString(tabCnt + 1);
    for (const shared_ptr<ExpNode> &exp : exps) {
        s += exp->toString(tabCnt + 1);
    }
    return s;
}

string FuncRParamsNode::toString(int tabCnt) {
    string s = string(tabCnt * _W_LEN, ' ') + "FuncRParamsNode\n";
    for (const shared_ptr<ExpNode> &exp : exps) {
        s += exp->toString(tabCnt + 1);
    }
    return s;
}

string MulExpNode::toString(int tabCnt) {
    if (!op.empty()) {
        return string(tabCnt * _W_LEN, ' ') + "MulExp " + op + "\n" + mulExp->toString(tabCnt + 1) +
               unaryExp->toString(tabCnt + 1);
    }
    return string(tabCnt * _W_LEN, ' ') + "MulExp\n" + unaryExp->toString(tabCnt + 1);
}

string AddExpNode::toString(int tabCnt) {
    if (!op.empty()) {
        return string(tabCnt * _W_LEN, ' ') + "AddExp " + op + "\n" + addExp->toString(tabCnt + 1) +
               mulExp->toString(tabCnt + 1);
    }
    return string(tabCnt * _W_LEN, ' ') + "AddExp\n" + mulExp->toString(tabCnt + 1);
}

string RelExpNode::toString(int tabCnt) {
    if (!op.empty()) {
        return string(tabCnt * _W_LEN, ' ') + "RelExp " + op + "\n" + relExp->toString(tabCnt + 1) +
               addExp->toString(tabCnt + 1);
    }
    return string(tabCnt * _W_LEN, ' ') + "RelExp\n" + addExp->toString(tabCnt + 1);
}

string EqExpNode::toString(int tabCnt) {
    if (!op.empty()) {
        return string(tabCnt * _W_LEN, ' ') + "EqExp " + op + "\n" + eqExp->toString(tabCnt + 1) +
               relExp->toString(tabCnt + 1);
    }
    return string(tabCnt * _W_LEN, ' ') + "EqExp\n" + relExp->toString(tabCnt + 1);
}

string LAndExpNode::toString(int tabCnt) {
    if (!op.empty()) {
        return string(tabCnt * _W_LEN, ' ') + "LAndExp " + op + "\n" + lAndExp->toString(tabCnt + 1) +
               eqExp->toString(tabCnt + 1);
    }
    return string(tabCnt * _W_LEN, ' ') + "LAndExp\n" + eqExp->toString(tabCnt + 1);
}

string LOrExpNode::toString(int tabCnt) {
    if (!op.empty()) {
        return string(tabCnt * _W_LEN, ' ') + "LOrExp " + op + "\n" + lOrExp->toString(tabCnt + 1) +
               lAndExp->toString(tabCnt + 1);
    }
    return string(tabCnt * _W_LEN, ' ') + "LOrExp\n" + lAndExp->toString(tabCnt + 1);
}

string IdentNode::toString(int tabCnt) {
    return string(tabCnt * _W_LEN, ' ') + "Ident " + ident->usageName + "\n";
}

string ExpNode::toString(int tabCnt) {
    return "";
}
