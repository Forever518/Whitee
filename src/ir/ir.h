#ifndef COMPILER_IR_H
#define COMPILER_IR_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include "../front/syntax/syntax_tree.h"

using namespace std;

class Value;

class BasicBlock;

class Function;

class Instruction;

class Module;

class BaseValue;

class ConstantValue;

class StringValue;

class GlobalValue;

class ParameterValue;

class NumberValue;

class UndefinedValue;

class ReturnInstruction;

class BranchInstruction;

class JumpInstruction;

class InvokeInstruction;

class UnaryInstruction;

class BinaryInstruction;

class AllocInstruction;

class LoadInstruction;

class StoreInstruction;

class PhiInstruction;

class PhiMoveInstruction;

enum ValueType {
    CONSTANT,
    NUMBER,
    STRING,
    GLOBAL,
    PARAMETER,
    UNDEFINED,
    INSTRUCTION,
    MODULE,
    FUNCTION,
    BASIC_BLOCK
};

enum VariableType {
    INT,
    POINTER
};

enum InstructionType {
    RET,
    BR,
    JMP,
    INVOKE,
    UNARY,
    BINARY,
    CMP,
    ALLOC,
    LOAD,
    STORE,
    PHI,
    PHI_MOV
};

enum InvokeType {
    COMMON,
    GET_INT,
    GET_CHAR,
    GET_ARRAY,
    PUT_INT,
    PUT_CHAR,
    PUT_ARRAY,
    PUT_F,
    START_TIME,
    STOP_TIME
};

enum ResultType {
    R_VAL_RESULT,
    L_VAL_RESULT,
    OTHER_RESULT
};

class Value : public enable_shared_from_this<Value> {
private:
    static unsigned int valueId;
public:

    unsigned int id;
    ValueType valueType;
    unordered_set<shared_ptr<Value>> users;

    bool valid = true;

    static unsigned int getValueId();

    explicit Value(ValueType valueType) : valueType(valueType), id(valueId++) {};

    virtual string toString() = 0;

    virtual void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) = 0;

    virtual void abandonUse() = 0;

    virtual unsigned long long hashCode() = 0;

    virtual bool equals(shared_ptr<Value> &value) = 0;
};

class Module : public Value {
public:
    vector<shared_ptr<Value>> globalStrings;
    vector<shared_ptr<Value>> globalConstants;
    vector<shared_ptr<Value>> globalVariables;
    vector<shared_ptr<Function>> functions;

    Module() : Value(ValueType::MODULE) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override {};

    void abandonUse() override {};

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override { return false; }
};

class Function : public Value {
public:
    string name;
    FuncType funcType;
    vector<shared_ptr<Value>> params;
    vector<shared_ptr<BasicBlock>> blocks;
    shared_ptr<BasicBlock> entryBlock; // the entryBlock is the first block of a function.

    unordered_set<shared_ptr<Function>> callees;
    unordered_set<shared_ptr<Function>> callers;

    unordered_map<shared_ptr<Value>, unsigned int> variableWeight; // value <--> weight.
    unordered_map<shared_ptr<Value>, string> variableRegs; // value <--> registers.
    unordered_set<shared_ptr<Value>> variableWithoutReg;
    unsigned int requiredStackSize = 0; // required size in bytes.

    bool hasSideEffect = true;

    unordered_map<string, VariableType> variables; // @Deprecated

    Function() : Value(ValueType::FUNCTION), funcType(FuncType::FUNC_VOID) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override {};

    void abandonUse() override;

    bool fitInline(unsigned int maxInsCnt, unsigned int maxPointerSituationCnt);

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override { return false; }
};

class BasicBlock : public Value {
public:
    shared_ptr<Function> function;

    unordered_set<shared_ptr<BasicBlock>> predecessors;
    unordered_set<shared_ptr<BasicBlock>> successors;
    vector<shared_ptr<Instruction>> instructions;
    unordered_set<shared_ptr<PhiInstruction>> phis;

    unsigned int loopDepth = 1; // used for register weight counting.
    unordered_set<shared_ptr<Value>> aliveValues; // the values which are alive in this basic block.

    unordered_map<string, shared_ptr<Value>> localVarSsaMap;

    bool sealed = true; // used to mark if this block is sealed.
    unordered_map<string, shared_ptr<PhiInstruction>> incompletePhis; // store incomplete phis.

    BasicBlock() : Value(ValueType::BASIC_BLOCK) {};

    BasicBlock(shared_ptr<Function> &function, bool sealed, unsigned int loopDepth)
            : Value(ValueType::BASIC_BLOCK), function(function), sealed(sealed), loopDepth(loopDepth) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override { return false; }
};

class Instruction : public Value {
public:
    InstructionType type;
    shared_ptr<BasicBlock> block;

    ResultType resultType;
    string caughtVarName; // the l-value local variable's name.
    unordered_set<shared_ptr<Value>> aliveValues; // the values which are alive at this instruction.

    Instruction(InstructionType type, shared_ptr<BasicBlock> &block, ResultType resultType)
            : Value(ValueType::INSTRUCTION), type(type), resultType(resultType), block(block) {};

    string toString() override = 0;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override = 0;

    void abandonUse() override = 0;

    unsigned long long hashCode() override = 0;

    bool equals(shared_ptr<Value> &value) override = 0;
};

class BaseValue : public Value {
public:
    explicit BaseValue(ValueType type) : Value(type) {};

    virtual string getIdent() = 0;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override {};

    void abandonUse() override {};

    unsigned long long hashCode() override = 0;

    bool equals(shared_ptr<Value> &value) override = 0;
};

/**
 * Stand for undefined SSA.
 */
class UndefinedValue : public BaseValue {
public:
    string originName;

    explicit UndefinedValue(string &name) : BaseValue(ValueType::UNDEFINED), originName(name) {};

    string toString() override;

    string getIdent() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override { return false; }
};

/**
 * Stand for immediate number.
 */
class NumberValue : public BaseValue {
public:
    int number;

    explicit NumberValue(int number) : BaseValue(ValueType::NUMBER), number(number) {};

    string toString() override;

    string getIdent() override;

    unsigned long long hashCode() override { return number; }

    bool equals(shared_ptr<Value> &value) override;
};

/**
 * Only used to keep the CONST ARRAYS.
 */
class ConstantValue : public BaseValue {
public:
    string name;
    vector<int> dimensions;
    map<int, int> values;
    int size = 0;

    ConstantValue() : BaseValue(ValueType::CONSTANT) {};

    explicit ConstantValue(shared_ptr<ConstDefNode> &constDef);

    explicit ConstantValue(shared_ptr<GlobalValue> &globalVar);

    string toString() override;

    string getIdent() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override;
};

/**
 * Used to keep a function parameter.
 */
class ParameterValue : public BaseValue {
public:
    string name;
    VariableType variableType;
    vector<int> dimensions;
    shared_ptr<Function> function;

    ParameterValue(shared_ptr<Function> &function, shared_ptr<FuncFParamNode> &funcFParam);

    string toString() override;

    string getIdent() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override;
};

/**
 * Used to keep global variables, including array or common variable.
 */
class GlobalValue : public BaseValue {
public:
    string name;
    InitType initType;
    VariableType variableType;
    vector<int> dimensions;
    map<int, int> initValues; // index <--> value, default 0.
    int size;

    explicit GlobalValue(shared_ptr<VarDefNode> &varDef);

    string toString() override;

    string getIdent() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override;
};

class StringValue : public BaseValue {
public:
    string str;

    explicit StringValue(string &str) : BaseValue(ValueType::STRING), str(str) {};

    string toString() override;

    string getIdent() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override;
};

/**
 * Return IR.
 */
class ReturnInstruction : public Instruction {
public:
    FuncType funcType;
    shared_ptr<Value> value;

    ReturnInstruction(FuncType funcType, shared_ptr<Value> &value, shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::RET, bb, OTHER_RESULT),
              funcType(funcType), value(value) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &val) override { return false; }
};

/**
 * Branch IR.
 */
class BranchInstruction : public Instruction {
public:
    shared_ptr<Value> condition;
    shared_ptr<BasicBlock> trueBlock;
    shared_ptr<BasicBlock> falseBlock;

    BranchInstruction(shared_ptr<Value> &condition, shared_ptr<BasicBlock> &trueBlock,
                      shared_ptr<BasicBlock> &falseBlock, shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::BR, bb, OTHER_RESULT), condition(condition),
              trueBlock(trueBlock), falseBlock(falseBlock) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override { return false; }
};

/**
 * Jump IR.
 */
class JumpInstruction : public Instruction {
public:
    shared_ptr<BasicBlock> targetBlock;

    JumpInstruction(shared_ptr<BasicBlock> &targetBlock, shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::JMP, bb, OTHER_RESULT), targetBlock(targetBlock) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override { return false; }
};

/**
 * Invoke IR.
 */
class InvokeInstruction : public Instruction {
public:
    static unordered_map<string, InvokeType> sysFuncMap;
    shared_ptr<Function> targetFunction;
    vector<shared_ptr<Value>> params;
    InvokeType invokeType;
    string targetName; // only used for system call.

    InvokeInstruction(shared_ptr<Function> &targetFunction, vector<shared_ptr<Value>> &params,
                      shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::INVOKE, bb,
                          targetFunction->funcType == FuncType::FUNC_INT ? R_VAL_RESULT
                                                                         : OTHER_RESULT),
              params(params), invokeType(InvokeType::COMMON), targetFunction(targetFunction) {};

    InvokeInstruction(string &sysFuncName, vector<shared_ptr<Value>> &params, shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::INVOKE, bb, sysFuncName == "getint"
                                                       || sysFuncName == "getch" || sysFuncName == "getarray" ?
                                                       R_VAL_RESULT : OTHER_RESULT),
              params(params),
              invokeType(sysFuncMap.at(sysFuncName)), targetName(
                    sysFuncName == "starttime" ? "_sysy_starttime" :
                    sysFuncName == "stoptime" ? "_sysy_stoptime" : sysFuncName) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override;
};

/**
 * Unary IR.
 */
class UnaryInstruction : public Instruction {
public:
    string op;
    shared_ptr<Value> value;

    UnaryInstruction(string &op, shared_ptr<Value> &value, shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::UNARY, bb, R_VAL_RESULT), op(op), value(value) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override {
        return (unsigned long long) value.get() * (op.empty() ? 0 : op.at(0));
    }

    bool equals(shared_ptr<Value> &val) override;
};

/**
 * Binary IR.
 */
class BinaryInstruction : public Instruction {
public:
    string op;
    shared_ptr<Value> lhs;
    shared_ptr<Value> rhs;

    BinaryInstruction(string &op, shared_ptr<Value> &lhs, shared_ptr<Value> &rhs, shared_ptr<BasicBlock> &bb)
            : Instruction(swapOp(op) != op ? InstructionType::CMP : InstructionType::BINARY, bb, R_VAL_RESULT),
              op(op), lhs(lhs), rhs(rhs) {};

    string toString() override;

    inline static string swapOp(const string &op) {
        return op == "==" ? "!=" :
               op == "!=" ? "==" :
               op == ">" ? "<=" :
               op == "<" ? ">=" :
               op == "<=" ? ">" :
               op == ">=" ? "<" : op;
    }

    inline static string swapOpConst(const string &op) {
        return op == ">" ? "<" :
               op == "<" ? ">" :
               op == "<=" ? ">=" :
               op == ">=" ? "<=" : op;
    }

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override {
        unsigned long long x = 0;
        if (!op.empty()) x = x * 5 + op.at(0);
        if (op.size() > 1) x = x * 5 + op.at(1);
        return x * ((unsigned long long) lhs.get() + (unsigned long long) rhs.get());
    }

    bool equals(shared_ptr<Value> &value) override;
};

/**
 * Memory Alloc IR.
 */
class AllocInstruction : public Instruction {
public:
    string name;
    int bytes;
    int units;

    AllocInstruction(string &name, int bytes, int units, shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::ALLOC, bb, OTHER_RESULT), name(name), bytes(bytes), units(units) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override;
};

/**
 * Memory store IR.
 */
class StoreInstruction : public Instruction {
public:
    shared_ptr<Value> value;
    shared_ptr<Value> address;
    shared_ptr<Value> offset;

    StoreInstruction(shared_ptr<Value> &value, shared_ptr<Value> &address,
                     shared_ptr<Value> &offset, shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::STORE, bb, OTHER_RESULT), value(value), address(address), offset(offset) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &val) override { return false; }
};

/**
 * Memory load IR.
 */
class LoadInstruction : public Instruction {
public:
    shared_ptr<Value> address;
    shared_ptr<Value> offset;

    LoadInstruction(shared_ptr<Value> &address, shared_ptr<Value> &offset, shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::LOAD, bb, R_VAL_RESULT), address(address), offset(offset) {};

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void abandonUse() override;

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override;
};

/**
 * Phi instruction used in SSA IR.
 */
class PhiInstruction : public Instruction {
public:
    string localVarName;
    unordered_map<shared_ptr<BasicBlock>, shared_ptr<Value>> operands;

    shared_ptr<PhiMoveInstruction> phiMove; // used after phi elimination.

    PhiInstruction(string &localVarName, shared_ptr<BasicBlock> &bb)
            : Instruction(InstructionType::PHI, bb, L_VAL_RESULT), localVarName(localVarName) {
        caughtVarName = localVarName;
    };

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override;

    void replaceUse(shared_ptr<BasicBlock> &toBeReplaced, shared_ptr<BasicBlock> &replaceBlock);

    void abandonUse() override;

    int getOperandValueCount(const shared_ptr<Value> &value);

    bool onlyHasBlockUserOrUserEmpty();

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override;
};

/**
 * Copy phi's operand at the end of each block predecessors.
 */
class PhiMoveInstruction : public Instruction {
public:
    shared_ptr<PhiInstruction> phi;

    unordered_map<shared_ptr<BasicBlock>, unordered_set<shared_ptr<Value>>> blockALiveValues;

    explicit PhiMoveInstruction(shared_ptr<PhiInstruction> &phi);

    string toString() override;

    void replaceUse(shared_ptr<Value> &toBeReplaced, shared_ptr<Value> &replaceValue) override {};

    void abandonUse() override {};

    unsigned long long hashCode() override { return 0; }

    bool equals(shared_ptr<Value> &value) override { return false; }
};

shared_ptr<NumberValue> getNumberValue(int);

string generateArgumentLeftValueName(const string &);

string generatePhiLeftValueName(const string &);

string generateTempLeftValueName();

unsigned int countWeight(unsigned int, unsigned int);

#endif
