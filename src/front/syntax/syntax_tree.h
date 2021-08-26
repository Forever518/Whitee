#ifndef COMPILER_SYNTAX_TREE_H
#define COMPILER_SYNTAX_TREE_H

#include <string>
#include <memory>
#include <utility>
#include <vector>
#include <unordered_map>

#include "../../basic/std/compile_std.h"

using namespace std;

/**
 * Declaration of syntax elements.
 */
// Basic tree node class.
class SyntaxNode;

// Derived classes of SyntaxNode.
class CompUnitNode;

class DeclNode;

class ConstDeclNode;

class ConstDefNode;

class ConstInitValNode;

class VarDeclNode;

class VarDefNode;

class InitValNode;

class FuncDefNode;

class FuncFParamsNode;

class FuncFParamNode;

class BlockNode;

class BlockItemNode;

class StmtNode;

class ExpNode;

class CondNode;

class LValNode;

class PrimaryExpNode;

class UnaryExpNode;

class FuncRParamsNode;

class MulExpNode;

class AddExpNode;

class RelExpNode;

class EqExpNode;

class LAndExpNode;

class LOrExpNode;

class IdentNode;

// The non-array init value of const definition.
class ConstInitValValNode;

// The array init value of const definition.
class ConstInitValArrNode;

// The non-array init value of variable definition.
class InitValValNode;

// The array init value of variable definition.
class InitValArrNode;

// String node.
class StringNode;

enum SymbolType {
    CONST_VAR,
    CONST_ARRAY,
    VAR,
    ARRAY,
    VOID_FUNC,
    RET_FUNC
};

/**
 * @brief: Symbol Item
 * Record detail info about a symbol.
 * usageName is for distinguishing symbols which may have same name in IR.
 * a(int, function) in Block <2, 1> is renamed to F*2_1$a.
 * b(int, VAR) in Block <3, 2> is renamed to V*3_2$b.
 * uniqueName is set to be the key distinguish F & V in the same Block.
 * a(int, function) in Block <2, 1> is renamed to F*a.
 * b(int, VAR) in Block <3, 2> is renamed to V*b.
 * @param globalVarInitVal For all the global var's val is 0 or can be calculate,
 * Use ConstInitValNode to calculate it.
 */
class SymbolTableItem {
public:
    SymbolType symbolType;
    int dimension;
    std::vector<int> numOfEachDimension;
    std::vector<std::shared_ptr<ExpNode>> expressionOfEachDimension;
    std::string name;
    std::string uniqueName;
    std::string usageName;
    std::pair<int, int> blockId;
    shared_ptr<ConstInitValNode> constInitVal;
    shared_ptr<InitValNode> initVal;
    shared_ptr<ConstInitValNode> globalVarInitVal;
    bool isRecursion = false;
    std::unordered_map<std::string, int> eachFuncUseNum;
    std::vector<std::shared_ptr<SymbolTableItem>> eachFunc;

    /**
     *
     * @brief Use for usage.
     * When use, all values in the bracket [] is not constExp for sure,
     * use Exp instead.
     */
    SymbolTableItem(SymbolType &symbolType, int &dimension,
                    std::vector<std::shared_ptr<ExpNode>> &expressionOfEachDimension,
                    std::string &name,
                    std::pair<int, int> &blockId);

    /**
     *
     * @brief Use for define.
     * When define, all values in the bracket [] is constExp,
     * can get a int value.
     * @note All the variable item in SymbolTable should be construct by this,
     * For only define a variable or a function need to record it into the SymbolTable.
     * In DefineVar Node->SymbolTableItem, should use numOfEachDimension.
     * Meanwhile, ident in FuncFParam should use this, for now assume all the exp are constExp.
     * Other case should use expressionOfEachDimension.
     */
    SymbolTableItem(SymbolType &symbolType, int &dimension,
                    std::vector<int> &numOfEachDimension,
                    std::string &name,
                    std::pair<int, int> &blockId);

    /**
     * @brief only for non ary variable.
     */
    SymbolTableItem(SymbolType &symbolType, std::string &name, std::pair<int, int> &blockId);

    bool isVarSingleUseInUnRecursionFunction();
};

/**
 * Definition of enums.
 */
enum FuncType {
    FUNC_INT,
    FUNC_VOID
};

enum StmtType {
    STMT_ASSIGN,
    STMT_EXP,
    STMT_BLOCK,
    STMT_IF,
    STMT_IF_ELSE,
    STMT_WHILE,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_RETURN,
    STMT_RETURN_VOID,
    STMT_EMPTY
};

enum UnaryExpType {
    UNARY_PRIMARY,
    UNARY_FUNC,
    UNARY_FUNC_NON_PARAM,
    UNARY_DEEP
};

enum PrimaryExpType {
    PRIMARY_PARENT_EXP,
    PRIMARY_L_VAL,
    PRIMARY_NUMBER,
    PRIMARY_STRING
};

enum InitType {
    INIT,
    NON_INIT
};

/**
 * Definition of syntax elements.
 */
class SyntaxNode {
public:
    virtual ~SyntaxNode() = default;

    virtual string toString(int tabCnt) = 0;
};

class CompUnitNode : public SyntaxNode {
public:
    vector<shared_ptr<DeclNode>> declList;
    vector<shared_ptr<FuncDefNode>> funcDefList;

    explicit CompUnitNode(vector<shared_ptr<DeclNode>> &declList, vector<shared_ptr<FuncDefNode>> &funcDefList)
            : declList(declList), funcDefList(funcDefList) {};

    string toString(int tabCnt) override;
};

class BlockNode : public SyntaxNode {
public:
    int itemCnt;
    vector<shared_ptr<BlockItemNode>> blockItems;

    BlockNode(int itemCnt, vector<shared_ptr<BlockItemNode>> &blockItems) : itemCnt(itemCnt), blockItems(blockItems) {};

    BlockNode() : itemCnt(0) {};

    string toString(int tabCnt) override;
};

class BlockItemNode : public SyntaxNode {
public:
    string toString(int tabCnt) override = 0;
};

class DeclNode : public BlockItemNode {
public:
    string toString(int tabCnt) override = 0;
};

class ConstDeclNode : public DeclNode {
public:
    vector<shared_ptr<ConstDefNode>> constDefList;

    explicit ConstDeclNode(vector<shared_ptr<ConstDefNode>> &constDefList)
            : constDefList(constDefList) {};

    string toString(int tabCnt) override;
};

class VarDeclNode : public DeclNode {
public:
    vector<shared_ptr<VarDefNode>> varDefList;

    explicit VarDeclNode(vector<shared_ptr<VarDefNode>> &varDefList)
            : varDefList(varDefList) {};

    string toString(int tabCnt) override;
};

class ConstDefNode : public SyntaxNode {
public:
    shared_ptr<IdentNode> ident;
    shared_ptr<ConstInitValNode> constInitVal;

    ConstDefNode(shared_ptr<IdentNode> &ident, shared_ptr<ConstInitValNode> &constInitVal)
            : ident(ident), constInitVal(constInitVal) {};

    string toString(int tabCnt) override;
};

class ConstInitValNode : public SyntaxNode {
public:
    string toString(int tabCnt) override = 0;

    virtual vector<pair<int, int>> toOneDimensionArray(int start, int size) = 0;
};

class ConstInitValValNode : public ConstInitValNode {
public:
    int value;

    explicit ConstInitValValNode(int &val) : value(val) {};

    string toString(int tabCnt) override;

    vector<pair<int, int>> toOneDimensionArray(int start, int size) override;
};

class ConstInitValArrNode : public ConstInitValNode {
public:
    int expectedSize;
    vector<shared_ptr<ConstInitValNode>> valList;

    explicit ConstInitValArrNode(int expectedSize, vector<shared_ptr<ConstInitValNode>> &valList)
            : expectedSize(expectedSize), valList(valList) {};

    string toString(int tabCnt) override;

    vector<pair<int, int>> toOneDimensionArray(int start, int size) override;
};

class VarDefNode : public SyntaxNode {
public:
    shared_ptr<IdentNode> ident;
    InitType type;
    int dimension;
    vector<int> dimensions;
    shared_ptr<InitValNode> initVal;

    VarDefNode(shared_ptr<IdentNode> &ident, int dimension, vector<int> &dimensions, shared_ptr<InitValNode> &initVal)
            : ident(ident), type(InitType::INIT), dimension(dimension), dimensions(dimensions), initVal(initVal) {};

    VarDefNode(shared_ptr<IdentNode> &ident, int dimension, vector<int> &dimensions)
            : ident(ident), type(InitType::NON_INIT), dimension(dimension), dimensions(dimensions) {};

    VarDefNode(shared_ptr<IdentNode> &ident, shared_ptr<InitValNode> &initVal)
            : ident(ident), type(InitType::INIT), dimension(0), initVal(initVal) {};

    explicit VarDefNode(shared_ptr<IdentNode> &ident)
            : ident(ident), type(InitType::NON_INIT), dimension(0) {};

    string toString(int tabCnt) override;
};

class InitValNode : public SyntaxNode {
public:
    string toString(int tabCnt) override = 0;

    virtual vector<pair<int, shared_ptr<ExpNode>>> toOneDimensionArray(int start, int size) = 0;
};

class InitValValNode : public InitValNode {
public:
    shared_ptr<ExpNode> exp;

    explicit InitValValNode(shared_ptr<ExpNode> &exp) : exp(exp) {};

    string toString(int tabCnt) override;

    vector<pair<int, shared_ptr<ExpNode>>> toOneDimensionArray(int start, int size) override;
};

class InitValArrNode : public InitValNode {
public:
    int expectedSize;
    vector<shared_ptr<InitValNode>> valList;

    InitValArrNode(int expectedSize, vector<shared_ptr<InitValNode>> &valList)
            : expectedSize(expectedSize), valList(valList) {};

    string toString(int tabCnt) override;

    vector<pair<int, shared_ptr<ExpNode>>> toOneDimensionArray(int start, int size) override;
};

class FuncDefNode : public SyntaxNode {
public:
    FuncType funcType;
    shared_ptr<IdentNode> ident;
    shared_ptr<FuncFParamsNode> funcFParams;
    shared_ptr<BlockNode> block;

    FuncDefNode(FuncType funcType, shared_ptr<IdentNode> &ident, shared_ptr<FuncFParamsNode> &funcFParams,
                shared_ptr<BlockNode> &block)
            : funcType(funcType), ident(ident), funcFParams(funcFParams), block(block) {};

    FuncDefNode(FuncType funcType, shared_ptr<IdentNode> &ident, shared_ptr<BlockNode> &block)
            : funcType(funcType), ident(ident), block(block) {};

    string toString(int tabCnt) override;
};

class FuncFParamsNode : public SyntaxNode {
public:
    vector<shared_ptr<FuncFParamNode>> funcParamList;

    explicit FuncFParamsNode(vector<shared_ptr<FuncFParamNode>> &funcParamList)
            : funcParamList(funcParamList) {};

    string toString(int tabCnt) override;
};

class FuncFParamNode : public SyntaxNode {
public:
    shared_ptr<IdentNode> ident;
    int dimension;
    /*
     * NOTE:
     * 1. The first dimensions[0] can be any number.
     * 2. The rest dimensions must be a constant.
     */
    vector<int> dimensions;

    FuncFParamNode(shared_ptr<IdentNode> &ident, int dimension, vector<int> &dimensions)
            : ident(ident), dimension(dimension), dimensions(dimensions) {};

    explicit FuncFParamNode(shared_ptr<IdentNode> &ident) : ident(ident), dimension(0) {};

    string toString(int tabCnt) override;
};

class StmtNode : public BlockItemNode {
public:
    /*
     * NOTE:
     * 1. 'break', 'continue' and 'empty' does not use any members.
     * 2. 'return' may use 0 member.
     * 3. Other type use at least 1 member.
     */
    StmtType type;
    shared_ptr<LValNode> lVal;
    shared_ptr<ExpNode> exp;
    shared_ptr<BlockNode> block;
    shared_ptr<CondNode> cond;
    shared_ptr<StmtNode> stmt;
    shared_ptr<StmtNode> elseStmt;

    StmtNode() : type(StmtType::STMT_EMPTY) {};

    static StmtNode assignStmt(shared_ptr<LValNode> &lVal, shared_ptr<ExpNode> &exp);

    static StmtNode emptyStmt();

    static StmtNode blockStmt(shared_ptr<BlockNode> &block);

    static StmtNode expStmt(shared_ptr<ExpNode> &exp);

    static StmtNode ifStmt(shared_ptr<CondNode> &cond, shared_ptr<StmtNode> &stmt, shared_ptr<StmtNode> &elseStmt);

    static StmtNode ifStmt(shared_ptr<CondNode> &cond, shared_ptr<StmtNode> &stmt);

    static StmtNode whileStmt(shared_ptr<CondNode> &cond, shared_ptr<StmtNode> &stmt);

    static StmtNode breakStmt();

    static StmtNode continueStmt();

    static StmtNode returnStmt(shared_ptr<ExpNode> &exp);

    static StmtNode returnStmt();

    string toString(int tabCnt) override;

private:
    StmtNode(StmtType type, shared_ptr<LValNode> &lVal, shared_ptr<ExpNode> &exp, shared_ptr<BlockNode> &block,
             shared_ptr<CondNode> &cond, shared_ptr<StmtNode> &stmt, shared_ptr<StmtNode> &elseStmt)
            : type(type), lVal(lVal), exp(exp), block(block), cond(cond), stmt(stmt), elseStmt(elseStmt) {};
};

class ExpNode : public SyntaxNode {
public:
    string toString(int tabCnt) override;
};

class PrimaryExpNode : public ExpNode {
public:
    /*
     * NOTE: This primaryExpNode can be type of (Exp), LVal or Number.
     */
    PrimaryExpType type;
    shared_ptr<ExpNode> exp;
    shared_ptr<LValNode> lVal;
    int number;
    string str;

    static PrimaryExpNode parentExp(shared_ptr<ExpNode> &exp);

    static PrimaryExpNode lValExp(shared_ptr<LValNode> &lVal);

    static PrimaryExpNode numberExp(int &number);

    static PrimaryExpNode stringExp(string &str);

    string toString(int tabCnt) override;

private:
    PrimaryExpNode(PrimaryExpType type, shared_ptr<ExpNode> &exp, shared_ptr<LValNode> &lVal,
                   int &number, string &str) : type(type), exp(exp), lVal(lVal), number(number), str(str) {};
};

class CondNode : public ExpNode {
public:
    shared_ptr<LOrExpNode> lOrExp;

    explicit CondNode(shared_ptr<LOrExpNode> &lOrExp) : lOrExp(lOrExp) {};

    string toString(int tabCnt) override;
};

class LValNode : public ExpNode {
public:
    shared_ptr<IdentNode> ident;
    int dimension;
    vector<shared_ptr<ExpNode>> exps;

    LValNode(shared_ptr<IdentNode> &ident, int dimension, vector<shared_ptr<ExpNode>> &exps)
            : ident(ident), dimension(dimension), exps(exps) {};

    explicit LValNode(shared_ptr<IdentNode> &ident) : ident(ident), dimension(0) {};

    string toString(int tabCnt) override;
};

class UnaryExpNode : public ExpNode {
public:
    UnaryExpType type;
    shared_ptr<PrimaryExpNode> primaryExp;
    shared_ptr<IdentNode> ident;
    shared_ptr<FuncRParamsNode> funcRParams;
    /*
     * NOTE: Using this member to mark whether the exp is positive, negative or not.
     */
    string op;
    shared_ptr<UnaryExpNode> unaryExp;

    static UnaryExpNode primaryUnaryExp(shared_ptr<PrimaryExpNode> &primaryExp, string &op);

    static UnaryExpNode
    funcCallUnaryExp(shared_ptr<IdentNode> &ident, shared_ptr<FuncRParamsNode> &funcRParams, string &op);

    static UnaryExpNode funcCallUnaryExp(shared_ptr<IdentNode> &ident, string &op);

    static UnaryExpNode unaryUnaryExp(shared_ptr<UnaryExpNode> &unaryExp, string &op);

    string toString(int tabCnt) override;

private:
    UnaryExpNode(UnaryExpType type, shared_ptr<PrimaryExpNode> &primaryExp, shared_ptr<IdentNode> &ident,
                 shared_ptr<FuncRParamsNode> &funcRParams, string &op, shared_ptr<UnaryExpNode> &unaryExp)
            : type(type), primaryExp(primaryExp), ident(ident), funcRParams(funcRParams), op(op), unaryExp(unaryExp) {};
};

class FuncRParamsNode : public SyntaxNode {
public:
    vector<shared_ptr<ExpNode>> exps;

    explicit FuncRParamsNode(vector<shared_ptr<ExpNode>> &exps) : exps(exps) {};

    string toString(int tabCnt) override;
};

class MulExpNode : public ExpNode {
public:
    shared_ptr<UnaryExpNode> unaryExp;
    shared_ptr<MulExpNode> mulExp;
    string op;

    MulExpNode(shared_ptr<UnaryExpNode> &unaryExp, shared_ptr<MulExpNode> &mulExp, string &op)
            : unaryExp(unaryExp), mulExp(mulExp), op(op) {};

    explicit MulExpNode(shared_ptr<UnaryExpNode> &unaryExp) : unaryExp(unaryExp) {};

    string toString(int tabCnt) override;
};

class AddExpNode : public ExpNode {
public:
    shared_ptr<MulExpNode> mulExp;
    shared_ptr<AddExpNode> addExp;
    string op;

    AddExpNode(shared_ptr<MulExpNode> &mulExp, shared_ptr<AddExpNode> &addExp, string &op)
            : mulExp(mulExp), addExp(addExp), op(op) {};

    explicit AddExpNode(shared_ptr<MulExpNode> &mulExp) : mulExp(mulExp) {};

    string toString(int tabCnt) override;
};

class RelExpNode : public ExpNode {
public:
    shared_ptr<AddExpNode> addExp;
    shared_ptr<RelExpNode> relExp;
    string op;

    RelExpNode(shared_ptr<AddExpNode> &addExp, shared_ptr<RelExpNode> &relExp, string &op)
            : addExp(addExp), relExp(relExp), op(op) {};

    explicit RelExpNode(shared_ptr<AddExpNode> &addExp) : addExp(addExp) {};

    string toString(int tabCnt) override;
};

class EqExpNode : public ExpNode {
public:
    shared_ptr<RelExpNode> relExp;
    shared_ptr<EqExpNode> eqExp;
    string op;

    EqExpNode(shared_ptr<RelExpNode> &relExp, shared_ptr<EqExpNode> &eqExp, string &op)
            : relExp(relExp), eqExp(eqExp), op(op) {};

    explicit EqExpNode(shared_ptr<RelExpNode> &relExp) : relExp(relExp) {};

    string toString(int tabCnt) override;
};

class LAndExpNode : public ExpNode {
public:
    shared_ptr<EqExpNode> eqExp;
    shared_ptr<LAndExpNode> lAndExp;
    string op;

    LAndExpNode(shared_ptr<EqExpNode> &eqExp, shared_ptr<LAndExpNode> &lAndExp, string &op)
            : eqExp(eqExp), lAndExp(lAndExp), op(op) {};

    explicit LAndExpNode(shared_ptr<EqExpNode> &eqExp) : eqExp(eqExp) {};

    string toString(int tabCnt) override;
};

class LOrExpNode : public ExpNode {
public:
    shared_ptr<LAndExpNode> lAndExp;
    shared_ptr<LOrExpNode> lOrExp;
    string op;

    LOrExpNode(shared_ptr<LAndExpNode> &lAndExp, shared_ptr<LOrExpNode> &lOrExp, string &op)
            : lAndExp(lAndExp), lOrExp(lOrExp), op(op) {};

    explicit LOrExpNode(shared_ptr<LAndExpNode> &lAndExp) : lAndExp(lAndExp) {};

    string toString(int tabCnt) override;
};

class IdentNode : public SyntaxNode {
public:
    std::shared_ptr<SymbolTableItem> ident;

    explicit IdentNode(shared_ptr<SymbolTableItem> ident) : ident(std::move(ident)) {};

    string toString(int tabCnt) override;
};

#endif
