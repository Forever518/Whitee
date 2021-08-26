#include "ir.h"

#include <iostream>

int global_bbCnt = 0;
int global_ssaCnt = 0;
unordered_map<unsigned int, string> valueSsaMap;
unordered_map<unsigned int, string> blockLabelMap;

/**
 * This function helps generate the written name of a Value.
 * @param v is the Value to be written.
 * @return the written name, which can be an ssa-name, a local pointer,
 *         a global pointer or a parameter name.
 */
string getSsaName(const shared_ptr<Value> &v) {
    string s;
    if (dynamic_cast<AllocInstruction *>(v.get())) {
        s = "%" + s_p_c<AllocInstruction>(v)->name + "*";
    } else if (dynamic_cast<BaseValue *>(v.get())) {
        s = s_p_c<BaseValue>(v)->getIdent();
        if (v->valueType == ValueType::PARAMETER
            && s_p_c<ParameterValue>(v)->variableType == VariableType::INT) {
            valueSsaMap[v->id] = s;
        }
    } else if (valueSsaMap.count(v->id) == 0) {
        s = "%" + to_string(global_ssaCnt++);
        valueSsaMap[v->id] = s;
    } else if (valueSsaMap.count(v->id) != 0) {
        s = valueSsaMap.at(v->id);
    } else {
        cerr << "Error occurs in process getSsaName." << endl;
    }
    return s;
}

/**
 * This function generates the basic block' name.
 * @param bb is the basic block.
 * @return the basic block start label.
 */
string getBasicBlockId(const shared_ptr<BasicBlock> &bb) {
    if (blockLabelMap.count(bb->id) == 0) {
        blockLabelMap[bb->id] = "<block> " + to_string(global_bbCnt++);
    }
    return blockLabelMap.at(bb->id);
}

string Module::toString() {
    global_bbCnt = 0;
    global_ssaCnt = 0;
    valueSsaMap.clear();
    blockLabelMap.clear();
    string s = "COMPILE IR:\n\n";
    if (!globalConstants.empty()) {
        for (const auto &c : globalConstants) {
            s += c->toString();
        }
        s += "\n";
    }
    if (!globalVariables.empty()) {
        for (const auto &v : globalVariables) {
            s += v->toString();
        }
        s += "\n";
    }
    if (!functions.empty()) {
        for (const auto &f : functions) {
            s += f->toString();
            s += "\n";
        }
    }
    return s;
}

string ConstantValue::toString() {
    string s = "@" + name + " = constant array ";
    for (const auto &d : dimensions) {
        s += "[" + to_string(d) + "]";
    }
    s += " [size " + to_string(values.size()) + "] (id " + to_string(id) + ") with values:\n   ";
    int temp = 0;
    if (values.empty()) {
        return s + " default 0\n";
    }
    for (const auto &i : values) {
        ++temp;
        if (temp > 50) {
            temp = 1;
            s += "\n   ";
        }
        s += " [" + to_string(i.first) + "]" + to_string(i.second);
    }
    return s + "\n";
}

string ConstantValue::getIdent() {
    return "@" + name + "*";
}

string ParameterValue::toString() {
    string s = "%" + name;
    if (variableType == VariableType::INT) {
        s += " = parameter int";
    } else {
        s += "* = parameter pointer []";
        for (int i = 1; i < dimensions.size(); ++i) {
            s += "[" + to_string(dimensions[i]) + "]";
        }
    }
    return s + " (id " + to_string(id) + ")\n";
}

string ParameterValue::getIdent() {
    if (variableType == VariableType::INT) return "%" + name;
    return "%" + name + "*";
}

string GlobalValue::toString() {
    string s = "@" + name + " = global ";
    if (variableType == VariableType::INT) {
        s += "int with init value " + to_string(initValues[0]);
    } else {
        s += "array ";
        for (const auto &d : dimensions) {
            s += "[" + to_string(d) + "]";
        }
        s += " [size " + to_string(size) + "] with init values (id " + to_string(id) + "):\n   ";
        int temp = 0;
        if (initValues.empty()) s += " default 0";
        for (const auto &i : initValues) {
            ++temp;
            if (temp > 50) {
                temp = 1;
                s += "\n   ";
            }
            s += " [" + to_string(i.first) + "]" + to_string(i.second);
        }
    }
    return s + "\n";
}

string GlobalValue::getIdent() {
    return "@" + name + "*";
}

string UndefinedValue::toString() {
    return "%[undefined value " + originName + "] (id " + to_string(id) + ")";
}

string UndefinedValue::getIdent() {
    return toString();
}

string NumberValue::toString() {
    return to_string(number);
}

string NumberValue::getIdent() {
    return toString();
}

string StringValue::toString() {
    return str;
}

string StringValue::getIdent() {
    return toString();
}

string Function::toString() {
    string s = "@" + name + " global function";
    if (funcType == FuncType::FUNC_INT) {
        s += " [return int]:\n";
    } else s += " [return void]:\n";
    s += "    " + getBasicBlockId(entryBlock) + " (id " + to_string(entryBlock->id) + "):\n";
    for (const auto &p : params) {
        s += "        " + p->toString();
    }
    if (!entryBlock->instructions.empty()) {
        s += entryBlock->toString();
    }
    for (int i = 1; i < blocks.size(); ++i) {
        const auto &bb = blocks.at(i);
        s += "    " + getBasicBlockId(bb) + " [depth " + to_string(bb->loopDepth)
             + "] (id " + to_string(bb->id) + "):\n";
        s += bb->toString();
    }
    return s;
}

string BasicBlock::toString() {
    string s;
    shared_ptr<BasicBlock> bb = s_p_c<BasicBlock>(shared_from_this());
    for (const auto &phi : phis) {
        if (!phi->phiMove)
            s += "        " + phi->toString();
    }
    for (const auto &ins : instructions) {
        if (ins->type != PHI_MOV)
            s += "        " + ins->toString();
        else {
            if (s_p_c<PhiMoveInstruction>(ins)->phi->operands.count(bb) == 0) {
                cerr << "Error occurs in process basic block to string: wrong phi move." << endl;
            }
            string valueAtThis = getSsaName(s_p_c<PhiMoveInstruction>(ins)->phi->operands.at(bb));
            s += "        " + getSsaName(ins) + " = copy " + valueAtThis + " (" + ins->caughtVarName + ") (id "
                 + to_string(ins->id) + ")\n";
        }
    }
    return s;
}

string ReturnInstruction::toString() {
    string type = funcType == FuncType::FUNC_INT ? "[int] " : "[void] ";
    string returnValue = funcType == FuncType::FUNC_INT ? getSsaName(value) : "";
    return "return " + type + returnValue + " (id " + to_string(id) + ")\n";
}

string BranchInstruction::toString() {
    return "branch [condition " + getSsaName(condition) + "] [true to " + getBasicBlockId(trueBlock)
           + "] [false to " + getBasicBlockId(falseBlock) + "] (id " + to_string(id) + ")\n";
}

string JumpInstruction::toString() {
    return "jump " + getBasicBlockId(targetBlock) + " (id " + to_string(id) + ")\n";
}

string InvokeInstruction::toString() {
    string s;
    shared_ptr<Value> v = shared_from_this();
    if (invokeType == InvokeType::COMMON) {
        if (targetFunction->funcType == FuncType::FUNC_INT) {
            s += getSsaName(v) + " = ";
        }
        s += "call " + targetFunction->name + " [args";
    } else {
        if (invokeType == InvokeType::GET_INT
            || invokeType == InvokeType::GET_CHAR
            || invokeType == InvokeType::GET_ARRAY) {
            s += getSsaName(v) + " = ";
        }
        s += "call system function " + targetName + " [args";
    }
    if (params.empty()) {
        s += " None]";
        if (resultType == L_VAL_RESULT) s += " (" + caughtVarName + ")";
        return s + " (id " + to_string(id) + ")\n";
    }
    for (const auto &i : params) {
        s += " " + getSsaName(i);
    }
    s += "]";
    if (resultType == L_VAL_RESULT) s += " (" + caughtVarName + ")";
    return s + " (id " + to_string(id) + ")\n";
}

string UnaryInstruction::toString() {
    shared_ptr<Value> v = shared_from_this();
    string s = getSsaName(v) + " = " + op + " " + getSsaName(value);
    if (resultType == L_VAL_RESULT) return s + " (" + caughtVarName + ") (id " + to_string(id) + ")\n";
    return s + " (id " + to_string(id) + ")\n";
}

string BinaryInstruction::toString() {
    shared_ptr<Value> v = shared_from_this();
    string s = getSsaName(v) + " = ";
    if (type == InstructionType::CMP) s += "[cmp] ";
    s += getSsaName(lhs) + " " + op + " " + getSsaName(rhs);
    if (resultType == L_VAL_RESULT) s += " (" + caughtVarName + ")";
    return s + " (id " + to_string(id) + ")\n";
}

string AllocInstruction::toString() {
    shared_ptr<Value> v = shared_from_this();
    return getSsaName(v) + " = alloc [units " + to_string(units) + "] [bytes " + to_string(bytes)
           + "] (id " + to_string(id) + ")\n";
}

string StoreInstruction::toString() {
    return "store " + getSsaName(value) + " to " + getSsaName(address) + " [offset " + getSsaName(offset)
           + "] (id " + to_string(id) + ")\n";
}

string LoadInstruction::toString() {
    shared_ptr<Value> v = shared_from_this();
    string s = getSsaName(v) + " = load " + getSsaName(address) + " [offset " + getSsaName(offset) + "]";
    if (resultType == L_VAL_RESULT) return s + " (" + caughtVarName + ") (id " + to_string(id) + ")\n";
    return s + " (id " + to_string(id) + ")\n";
}

string PhiInstruction::toString() {
    shared_ptr<Value> v = shared_from_this();
    string s = getSsaName(v) + " = phi";
    if (phiMove != nullptr) {
        s += " " + getSsaName(phiMove);
    } else {
        for (const auto &item : operands) {
            s += " [" + getSsaName(item.second) + ", " + getBasicBlockId(item.first) + "]";
        }
    }
    return s + " (" + localVarName + ") (id " + to_string(id) + ")\n";
}

string PhiMoveInstruction::toString() {
    shared_ptr<Value> v = shared_from_this();
    string s = getSsaName(v) + " = phi move";
    for (const auto &item : phi->operands) {
        s += " [" + getSsaName(item.second) + ", " + getBasicBlockId(item.first) + "]";
    }
    return s + " (" + caughtVarName + ") (id " + to_string(id) + ")\n";
}
