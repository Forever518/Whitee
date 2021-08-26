#include <iostream>
#include <initializer_list>

#include "ir_build.h"

shared_ptr<Module> module;

unordered_map<string, shared_ptr<Function>> globalFunctionMap; // name <--> Function
unordered_map<string, shared_ptr<ConstantValue>> globalConstantMap; // name <--> Constant Array
unordered_map<string, shared_ptr<GlobalValue>> globalVariableMap; // name <--> Global Variable
unordered_map<string, shared_ptr<Value>> localArrayMap; // name <--> Local Array
unordered_map<string, shared_ptr<StringValue>> globalStringMap; // string <--> string pointer

string mulOp = "*"; // NOLINT
string addOp = "+"; // NOLINT

unsigned int loopDepth = 0;

void blockToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<BlockNode> &block,
               shared_ptr<BasicBlock> &loopJudge, shared_ptr<BasicBlock> &loopEnd, bool &afterJump);

void blockToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<BlockNode> &block);

void varDefToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb,
                const shared_ptr<VarDefNode> &varDef, bool &afterJump);

void stmtToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<StmtNode> &stmt,
              shared_ptr<BasicBlock> &loopJudge, shared_ptr<BasicBlock> &loopEnd, bool &afterJump);

shared_ptr<Value> expToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<ExpNode> &exp);

void conditionToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<ExpNode> &cond,
                   shared_ptr<BasicBlock> &trueBlock, shared_ptr<BasicBlock> &falseBlock);

void pointerToIr(const shared_ptr<LValNode> &lVal, shared_ptr<Value> &address, shared_ptr<Value> &offset,
                 shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb);

shared_ptr<Module> buildIrModule(shared_ptr<CompUnitNode> &compUnit) {
    module = make_shared<Module>(); // NOLINT
    for (const auto &decl : compUnit->declList) {
        if (dynamic_cast<ConstDeclNode *>(decl.get())) {
            for (auto &def : s_p_c<ConstDeclNode>(decl)->constDefList) {
                if (def->ident->ident->symbolType == SymbolType::CONST_ARRAY) {
                    shared_ptr<Value> value = make_shared<ConstantValue>(def);
                    module->globalConstants.push_back(value);
                    globalConstantMap.insert(
                            {def->ident->ident->usageName, s_p_c<ConstantValue>(value)});
                }
            }
        } else {
            for (auto &def : s_p_c<VarDeclNode>(decl)->varDefList) {
                shared_ptr<Value> value = make_shared<GlobalValue>(def);
                module->globalVariables.push_back(value);
                globalVariableMap.insert({def->ident->ident->usageName, s_p_c<GlobalValue>(value)});
            }
        }
    }
    for (auto &funcNode : compUnit->funcDefList) {
        shared_ptr<Function> function = make_shared<Function>();
        shared_ptr<BasicBlock> entryBlock = make_shared<BasicBlock>(function, true, loopDepth);
        module->functions.push_back(function);
        function->name = funcNode->ident->ident->usageName;
        function->funcType = funcNode->funcType;
        function->entryBlock = entryBlock;
        function->blocks.push_back(entryBlock);
        globalFunctionMap.insert({function->name, function});
        if (funcNode->funcFParams) {
            for (auto &param : funcNode->funcFParams->funcParamList) {
                shared_ptr<Value> paramValue = make_shared<ParameterValue>(function, param);
                function->params.push_back(paramValue);
                if (s_p_c<ParameterValue>(paramValue)->variableType == VariableType::INT) {
                    writeLocalVariable(entryBlock, s_p_c<ParameterValue>(paramValue)->name, paramValue);
                } else {
                    localArrayMap[param->ident->ident->usageName] = paramValue;
                }
            }
        }
        blockToIr(function, entryBlock, funcNode->block);
    }
    return module;
}

/**
 * Transform a block(this 'block' is the concept of AST, instead of SSA) to IR.
 */
void blockToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<BlockNode> &block,
               shared_ptr<BasicBlock> &loopJudge, shared_ptr<BasicBlock> &loopEnd, bool &afterJump) {
    if (block == nullptr) return;
    for (const auto &item : block->blockItems) {
        if (dynamic_cast<VarDeclNode *>(item.get())) {
            shared_ptr<VarDeclNode> varDecl = s_p_c<VarDeclNode>(item);
            for (const auto &varDef : varDecl->varDefList) {
                varDefToIr(func, bb, varDef, afterJump);
            }
        } else if (dynamic_cast<ConstDeclNode *>(item.get())) {
            shared_ptr<ConstDeclNode> constDecl = s_p_c<ConstDeclNode>(item);
            for (auto &constDef : constDecl->constDefList) {
                if (constDef->ident->ident->symbolType == SymbolType::CONST_ARRAY) {
                    shared_ptr<Value> value = make_shared<ConstantValue>(constDef);
                    module->globalConstants.push_back(value);
                    globalConstantMap.insert(
                            {constDef->ident->ident->usageName, s_p_c<ConstantValue>(value)});
                }
            }
        } else if (dynamic_cast<StmtNode *>(item.get())) {
            stmtToIr(func, bb, s_p_c<StmtNode>(item), loopJudge, loopEnd, afterJump);
        } else {
            cerr << "Error occurs in process blockToIr." << endl;
        }
    }
}

void blockToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<BlockNode> &block) {
    shared_ptr<BasicBlock> judge, end;
    bool afterJump = false;
    blockToIr(func, bb, block, judge, end, afterJump);
}

/**
 * Transform the variable definitions to IR.
 */
void varDefToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb,
                const shared_ptr<VarDefNode> &varDef, bool &afterJump) {
    if (afterJump) return;
    func->variables.insert({varDef->ident->ident->usageName,
                            varDef->dimension == 0 ? VariableType::INT : VariableType::POINTER});
    if (varDef->dimension == 0 && varDef->type == InitType::INIT) {
        shared_ptr<InitValValNode> initVal = s_p_c<InitValValNode>(varDef->initVal);
        shared_ptr<Value> exp = expToIr(func, bb, initVal->exp);
        if (exp->valueType == ValueType::INSTRUCTION) {
            shared_ptr<Instruction> insExp = s_p_c<Instruction>(exp);
            insExp->resultType = L_VAL_RESULT;
            insExp->caughtVarName = varDef->ident->ident->usageName;
        }
        writeLocalVariable(bb, varDef->ident->ident->usageName, exp);
    } else if (varDef->dimension != 0) {
        int units = 1;
        for (const auto &d : varDef->dimensions) units *= d;
        shared_ptr<Value> alloc = make_shared<AllocInstruction>(varDef->ident->ident->usageName,
                                                                units * _W_LEN, units, bb);
        bb->instructions.push_back(s_p_c<Instruction>(alloc));
        localArrayMap.insert({s_p_c<AllocInstruction>(alloc)->name,
                              s_p_c<AllocInstruction>(alloc)});
        if (varDef->type == InitType::INIT) {
            vector<pair<int, shared_ptr<ExpNode>>> initValues = varDef->initVal->toOneDimensionArray(0, units);
            int curIndex = 0;
            for (auto &it : initValues) {
                for (; curIndex < it.first; ++curIndex) {
                    shared_ptr<Value> zero = getNumberValue(0);;
                    shared_ptr<Value> offset = getNumberValue(curIndex);;
                    shared_ptr<Instruction> store = make_shared<StoreInstruction>(zero, alloc, offset, bb);
                    addUser(store, {zero, alloc, offset});
                    bb->instructions.push_back(store);
                }
                ++curIndex;
                shared_ptr<Value> exp = expToIr(func, bb, it.second);
                shared_ptr<Value> offset = getNumberValue(it.first);;
                shared_ptr<Instruction> store = make_shared<StoreInstruction>(exp, alloc, offset, bb);
                addUser(store, {exp, alloc, offset});
                bb->instructions.push_back(store);
            }
            for (; curIndex < units; ++curIndex) {
                shared_ptr<Value> zero = getNumberValue(0);
                shared_ptr<Value> offset = getNumberValue(curIndex);
                shared_ptr<Instruction> store = make_shared<StoreInstruction>(zero, alloc, offset, bb);
                addUser(store, {zero, alloc, offset});
                bb->instructions.push_back(store);
            }
        }
    }
}

/**
 * Transform the statements to IR.
 */
void stmtToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<StmtNode> &stmt,
              shared_ptr<BasicBlock> &loopJudge, shared_ptr<BasicBlock> &loopEnd, bool &afterJump) {
    if (afterJump) return;
    switch (stmt->type) {
        case StmtType::STMT_EMPTY:
            return;
        case StmtType::STMT_EXP: {
            expToIr(func, bb, stmt->exp);
            return;
        }
        case StmtType::STMT_BLOCK: {
            blockToIr(func, bb, stmt->block, loopJudge, loopEnd, afterJump);
            return;
        }
        case StmtType::STMT_ASSIGN: {
            shared_ptr<Value> value = expToIr(func, bb, stmt->exp);
            shared_ptr<SymbolTableItem> identItem = stmt->lVal->ident->ident;
            shared_ptr<Value> address, offset;
            switch (identItem->symbolType) {
                case SymbolType::VAR: {
                    if (identItem->blockId.first == 0) {
                        pointerToIr(stmt->lVal, address, offset, func, bb);
                        shared_ptr<Instruction> ins = make_shared<StoreInstruction>(value, address, offset, bb);
                        addUser(ins, {value, address, offset});
                        bb->instructions.push_back(ins);
                    } else {
                        if (value->valueType == ValueType::INSTRUCTION) {
                            shared_ptr<Instruction> insValue = s_p_c<Instruction>(value);
                            insValue->resultType = L_VAL_RESULT;
                            insValue->caughtVarName = stmt->lVal->ident->ident->usageName;
                        }
                        writeLocalVariable(bb, stmt->lVal->ident->ident->usageName, value);
                    }
                    return;
                }
                case SymbolType::ARRAY: {
                    if (value->valueType == ValueType::INSTRUCTION
                        && s_p_c<Instruction>(value)->resultType != L_VAL_RESULT) {
                        shared_ptr<Instruction> insValue = s_p_c<Instruction>(value);
                        insValue->resultType = L_VAL_RESULT;
                        insValue->caughtVarName = generateTempLeftValueName();
                    }
                    pointerToIr(stmt->lVal, address, offset, func, bb);
                    shared_ptr<Instruction> ins = make_shared<StoreInstruction>(value, address, offset, bb);
                    addUser(ins, {value, address, offset});
                    bb->instructions.push_back(ins);
                    return;
                }
                default:
                    cerr << "Error occurs in stmt assign to IR: invalid l-value type." << endl;
                    return;
            }
        }
        case StmtType::STMT_RETURN: {
            shared_ptr<Value> value = expToIr(func, bb, stmt->exp);
            shared_ptr<Instruction> ins = make_shared<ReturnInstruction>(FuncType::FUNC_INT, value, bb);
            addUser(ins, {value});
            bb->instructions.push_back(ins);
            afterJump = true;
            return;
        }
        case StmtType::STMT_RETURN_VOID: {
            shared_ptr<Value> value = nullptr;
            shared_ptr<Instruction> ins = make_shared<ReturnInstruction>(FuncType::FUNC_VOID, value, bb);
            bb->instructions.push_back(ins);
            afterJump = true;
            return;
        }
        case StmtType::STMT_IF: {
            // declare if and end.
            shared_ptr<BasicBlock> endIf = make_shared<BasicBlock>(func, true, loopDepth);
            shared_ptr<BasicBlock> ifStmt = make_shared<BasicBlock>(func, true, loopDepth);
            // transform condition.
            conditionToIr(func, bb, stmt->cond, ifStmt, endIf);
            // maintain successors and predecessors.
            bb->successors.insert({endIf, ifStmt});
            endIf->predecessors.insert(bb);
            ifStmt->predecessors.insert(bb);
            // bb comes to the end, assign it to endIf block;
            bb = endIf;
            // the if stmt should be added to blocks of the function.
            func->blocks.push_back(ifStmt);
            // define ifAfterJump to mark if the if come to an end itself.
            bool ifAfterJump = false;
            // analyze if stmt.
            stmtToIr(func, ifStmt, stmt->stmt, loopJudge, loopEnd, ifAfterJump);
            // if no jump happens, add a jump back to the end block, in case of the disordering of the blocks.
            if (!ifAfterJump) {
                shared_ptr<Instruction> jmp = make_shared<JumpInstruction>(endIf, ifStmt);
                ifStmt->instructions.push_back(jmp);
                // maintain if stmt successors and end if block's predecessors.
                ifStmt->successors.insert(endIf);
                endIf->predecessors.insert(ifStmt);
            }
            // add end if stmt to function.
            func->blocks.push_back(endIf);
            return;
        }
        case StmtType::STMT_IF_ELSE: {
            shared_ptr<BasicBlock> endIf = make_shared<BasicBlock>(func, true, loopDepth);
            shared_ptr<BasicBlock> ifStmt = make_shared<BasicBlock>(func, true, loopDepth);
            shared_ptr<BasicBlock> elseStmt = make_shared<BasicBlock>(func, true, loopDepth);
            conditionToIr(func, bb, stmt->cond, ifStmt, elseStmt);
            bb->successors.insert({ifStmt, elseStmt});
            ifStmt->predecessors.insert(bb);
            elseStmt->predecessors.insert(bb);
            bb = endIf;
            bool ifAfterJump = false, elseAfterJump = false;
            // if stmt.
            func->blocks.push_back(ifStmt);
            stmtToIr(func, ifStmt, stmt->stmt, loopJudge, loopEnd, ifAfterJump);
            if (!ifAfterJump) {
                shared_ptr<Instruction> jmpIf = make_shared<JumpInstruction>(endIf, ifStmt);
                ifStmt->instructions.push_back(jmpIf);
                // maintain successors and predecessors.
                ifStmt->successors.insert(endIf);
                endIf->predecessors.insert(ifStmt);
            }
            // different from if, analyze else here.
            func->blocks.push_back(elseStmt);
            stmtToIr(func, elseStmt, stmt->elseStmt, loopJudge, loopEnd, elseAfterJump);
            if (!elseAfterJump) {
                shared_ptr<Instruction> jmpElse = make_shared<JumpInstruction>(endIf, elseStmt);
                elseStmt->instructions.push_back(jmpElse);
                // maintain successors and predecessors.
                elseStmt->successors.insert(endIf);
                endIf->predecessors.insert(elseStmt);
            }
            if (ifAfterJump && elseAfterJump)
                afterJump = true;
            else
                func->blocks.push_back(endIf);
            return;
        }
        case StmtType::STMT_WHILE: {
            // declare loop head, loop end and loop body.
            shared_ptr<BasicBlock> whileEnd = make_shared<BasicBlock>(func, true, loopDepth);
            shared_ptr<BasicBlock> whileJudge = make_shared<BasicBlock>(func, true, loopDepth + 1);
            shared_ptr<BasicBlock> whileBody = make_shared<BasicBlock>(func, false, loopDepth + 1);
            shared_ptr<BasicBlock> preWhileBody = whileBody;
            // transform condition.
            conditionToIr(func, bb, stmt->cond, whileBody, whileEnd);
            /* maintain successors and predecessors,
               be careful of the whileJudge which cannot be analyze before whileBody. */
            bb->successors.insert({whileEnd, whileBody});
            whileEnd->predecessors.insert(bb);
            whileBody->predecessors.insert(bb);
            // assign bb as while end.
            bb = whileEnd;
            // push while body into blocks and analyze it.
            bool whileBodyAfterJump = false;
            func->blocks.push_back(whileBody);
            ++loopDepth; // goto while body.
            stmtToIr(func, whileBody, stmt->stmt, whileJudge, whileEnd, whileBodyAfterJump);
            --loopDepth;
            if (!whileBodyAfterJump) {
                shared_ptr<Instruction> jmpJudge = make_shared<JumpInstruction>(whileJudge, whileBody);
                whileBody->instructions.push_back(jmpJudge);
                whileBody->successors.insert(whileJudge);
                whileJudge->predecessors.insert(whileBody);
            }
            if (!whileJudge->predecessors.empty()) {
                // finish whileJudge block.
                func->blocks.push_back(whileJudge);
                ++loopDepth;
                conditionToIr(func, whileJudge, stmt->cond, preWhileBody, whileEnd);
                --loopDepth;
                whileEnd->predecessors.insert(whileJudge);
                whileJudge->successors.insert(whileEnd);
                // maintain previous while body's successors and predecessors.
                preWhileBody->predecessors.insert(whileJudge);
                whileJudge->successors.insert(preWhileBody);
            }
            sealBasicBlock(preWhileBody);
            // end.
            func->blocks.push_back(whileEnd);
            return;
        }
        case StmtType::STMT_BREAK: {
            if (!loopEnd) cerr << "Error occurs in stmt to IR: break without a loop." << endl;
            shared_ptr<Instruction> jmp = make_shared<JumpInstruction>(loopEnd, bb);
            bb->instructions.push_back(jmp);
            bb->successors.insert(loopEnd);
            loopEnd->predecessors.insert(bb);
            afterJump = true;
            return;
        }
        default:
            if (!loopJudge) cerr << "Error occurs in stmt to IR: continue without a loop." << endl;
            shared_ptr<Instruction> jmp = make_shared<JumpInstruction>(loopJudge, bb);
            bb->instructions.push_back(jmp);
            bb->successors.insert(loopJudge);
            loopJudge->predecessors.insert(bb);
            afterJump = true;
    }
}

/**
 * Transform common expression to IR.
 * @return the result Value of the expression, in non-return function call it does not have any meanings.
 */
shared_ptr<Value> expToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<ExpNode> &exp) {
    if (dynamic_cast<PrimaryExpNode *>(exp.get())) {
        auto p = s_p_c<PrimaryExpNode>(exp);
        switch (p->type) {
            case PrimaryExpType::PRIMARY_L_VAL: {
                shared_ptr<SymbolTableItem> identItem = p->lVal->ident->ident;
                shared_ptr<Value> address, offset;
                switch (identItem->symbolType) {
                    case SymbolType::VAR: {
                        if (identItem->blockId.first == 0) {
                            pointerToIr(p->lVal, address, offset, func, bb);
                            shared_ptr<Instruction> ins = make_shared<LoadInstruction>(address, offset, bb);
                            addUser(ins, {address, offset});
                            bb->instructions.push_back(ins);
                            return ins;
                        }
                        return readLocalVariable(bb, identItem->usageName);
                    }
                    case SymbolType::CONST_ARRAY:
                    case SymbolType::ARRAY: {
                        if (p->lVal->exps.empty()) {
                            if (localArrayMap.count(p->lVal->ident->ident->usageName) != 0)
                                return localArrayMap.at(p->lVal->ident->ident->usageName);
                            else if (globalVariableMap.count(p->lVal->ident->ident->usageName) != 0)
                                return globalVariableMap.at(p->lVal->ident->ident->usageName);
                            else if (globalConstantMap.count(p->lVal->ident->ident->usageName) != 0)
                                return globalConstantMap.at(p->lVal->ident->ident->usageName);
                            cerr << "Error occurs in process primary exp to IR: array is not defined." << endl;
                            return nullptr;
                        } else {
                            pointerToIr(p->lVal, address, offset, func, bb);
                            if (p->lVal->exps.size() == p->lVal->dimension) {
                                shared_ptr<Instruction> load = make_shared<LoadInstruction>(address, offset, bb);
                                addUser(load, {address, offset});
                                bb->instructions.push_back(load);
                                return load;
                            } else {
                                shared_ptr<Instruction> pt = make_shared<BinaryInstruction>(addOp, address, offset, bb);
                                addUser(pt, {address, offset});
                                bb->instructions.push_back(pt);
                                return pt;
                            }
                        }
                    }
                    default:
                        cerr << "Error occurs in process PrimaryExpNode to IR." << endl;
                        return nullptr;
                }
            }
            case PrimaryExpType::PRIMARY_STRING: {
                if (globalStringMap.count(p->str) != 0) {
                    return globalStringMap.at(p->str);
                }
                shared_ptr<StringValue> str = make_shared<StringValue>(p->str);
                globalStringMap[p->str] = str;
                module->globalStrings.push_back(str);
                return str;
            }
            case PrimaryExpType::PRIMARY_NUMBER:
                return getNumberValue(p->number);
            default:
                return expToIr(func, bb, p->exp);
        }
    } else if (dynamic_cast<UnaryExpNode *>(exp.get())) {
        auto p = s_p_c<UnaryExpNode>(exp);
        switch (p->type) {
            case UnaryExpType::UNARY_PRIMARY: {
                shared_ptr<Value> value = expToIr(func, bb, p->primaryExp);
                if (p->op == "+") return value;
                shared_ptr<Instruction> ins = make_shared<UnaryInstruction>(p->op, value, bb);
                addUser(ins, {value});
                bb->instructions.push_back(ins);
                return ins;
            }
            case UnaryExpType::UNARY_FUNC_NON_PARAM:
            case UnaryExpType::UNARY_FUNC: {
                vector<shared_ptr<Value>> params;
                if (p->funcRParams) {
                    for (const auto &i : p->funcRParams->exps) {
                        shared_ptr<Value> tmpExp = expToIr(func, bb, i);
                        if (p->funcRParams->exps.size() > 1 && tmpExp->valueType == ValueType::INSTRUCTION
                            && s_p_c<Instruction>(tmpExp)->resultType != L_VAL_RESULT) {
                            s_p_c<Instruction>(tmpExp)->resultType = L_VAL_RESULT;
                            s_p_c<Instruction>(tmpExp)->caughtVarName
                                    = generateArgumentLeftValueName(p->ident->ident->usageName);
                        }
                        params.push_back(tmpExp);
                    }
                }
                shared_ptr<Value> invoke;
                if (InvokeInstruction::sysFuncMap.count(p->ident->ident->name) != 0) {
                    invoke = make_shared<InvokeInstruction>(p->ident->ident->name, params, bb);
                } else {
                    shared_ptr<Function> targetFunction = globalFunctionMap.at(p->ident->ident->usageName);
                    func->callees.insert(targetFunction);
                    targetFunction->callers.insert(func);
                    invoke = make_shared<InvokeInstruction>(targetFunction, params, bb);
                }
                addUser(invoke, params);
                bb->instructions.push_back(s_p_c<Instruction>(invoke));
                if (p->op == "+") return invoke;
                shared_ptr<Instruction> ins = make_shared<UnaryInstruction>(p->op, invoke, bb);
                addUser(ins, {invoke});
                bb->instructions.push_back(ins);
                return ins;
            }
            default:
                shared_ptr<Value> value = expToIr(func, bb, p->unaryExp);
                if (p->op == "+") return value;
                shared_ptr<Instruction> ins = make_shared<UnaryInstruction>(p->op, value, bb);
                addUser(ins, {value});
                bb->instructions.push_back(ins);
                return ins;
        }
    } else if (dynamic_cast<MulExpNode *>(exp.get())) {
        auto p = s_p_c<MulExpNode>(exp);
        if (p->mulExp == nullptr) {
            return expToIr(func, bb, p->unaryExp);
        } else {
            shared_ptr<Value> lhs = expToIr(func, bb, p->mulExp);
            shared_ptr<Value> rhs = expToIr(func, bb, p->unaryExp);
            shared_ptr<Instruction> ins = make_shared<BinaryInstruction>(p->op, lhs, rhs, bb);
            addUser(ins, {lhs, rhs});
            bb->instructions.push_back(ins);
            return ins;
        }
    } else if (dynamic_cast<AddExpNode *>(exp.get())) {
        auto p = s_p_c<AddExpNode>(exp);
        if (p->addExp == nullptr) {
            return expToIr(func, bb, p->mulExp);
        } else {
            shared_ptr<Value> lhs = expToIr(func, bb, p->addExp);
            shared_ptr<Value> rhs = expToIr(func, bb, p->mulExp);
            shared_ptr<Instruction> ins = make_shared<BinaryInstruction>(p->op, lhs, rhs, bb);
            addUser(ins, {lhs, rhs});
            bb->instructions.push_back(ins);
            return ins;
        }
    } else if (dynamic_cast<RelExpNode *>(exp.get())) {
        auto p = s_p_c<RelExpNode>(exp);
        if (p->relExp == nullptr) {
            return expToIr(func, bb, p->addExp);
        } else {
            shared_ptr<Value> lhs = expToIr(func, bb, p->relExp);
            shared_ptr<Value> rhs = expToIr(func, bb, p->addExp);
            if (lhs->valueType == INSTRUCTION && s_p_c<Instruction>(lhs)->resultType == R_VAL_RESULT
                && rhs->valueType == INSTRUCTION && s_p_c<Instruction>(rhs)->resultType == R_VAL_RESULT) {
                s_p_c<Instruction>(lhs)->resultType = L_VAL_RESULT;
                s_p_c<Instruction>(lhs)->caughtVarName = generateTempLeftValueName();
            }
            shared_ptr<Instruction> ins = make_shared<BinaryInstruction>(p->op, lhs, rhs, bb);
            addUser(ins, {lhs, rhs});
            bb->instructions.push_back(ins);
            return ins;
        }
    } else if (dynamic_cast<EqExpNode *>(exp.get())) {
        auto p = s_p_c<EqExpNode>(exp);
        if (p->eqExp == nullptr) {
            return expToIr(func, bb, p->relExp);
        } else {
            shared_ptr<Value> lhs = expToIr(func, bb, p->eqExp);
            shared_ptr<Value> rhs = expToIr(func, bb, p->relExp);
            if (lhs->valueType == INSTRUCTION && s_p_c<Instruction>(lhs)->resultType == R_VAL_RESULT
                && rhs->valueType == INSTRUCTION && s_p_c<Instruction>(rhs)->resultType == R_VAL_RESULT) {
                s_p_c<Instruction>(lhs)->resultType = L_VAL_RESULT;
                s_p_c<Instruction>(lhs)->caughtVarName = generateTempLeftValueName();
            }
            shared_ptr<Instruction> ins = make_shared<BinaryInstruction>(p->op, lhs, rhs, bb);
            addUser(ins, {lhs, rhs});
            bb->instructions.push_back(ins);
            return ins;
        }
    } else {
        cerr << "Error occurs in process expToIr: invalid expression type." << endl;
    }
    return nullptr;
}

/**
 * Transform the condition expression of IF or WHILE statement, which may build new basic blocks.
 */
void conditionToIr(shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb, const shared_ptr<ExpNode> &cond,
                   shared_ptr<BasicBlock> &trueBlock, shared_ptr<BasicBlock> &falseBlock) {
    if (dynamic_cast<LOrExpNode *>(cond.get())) {
        const shared_ptr<LOrExpNode> lOrExp = s_p_c<LOrExpNode>(cond);
        if (lOrExp->lOrExp == nullptr) {
            conditionToIr(func, bb, lOrExp->lAndExp, trueBlock, falseBlock);
        } else {
            // declare a new basic block as the 2nd condition judge block.
            shared_ptr<BasicBlock> logicOrBlock = make_shared<BasicBlock>(func, true, loopDepth);
            conditionToIr(func, bb, lOrExp->lOrExp, trueBlock, logicOrBlock);
            // maintain the predecessors and successors of last basic block.
            bb->successors.insert({trueBlock, logicOrBlock});
            trueBlock->predecessors.insert(bb);
            logicOrBlock->predecessors.insert(bb);
            // change bb to the 2nd condition block, and then push back it to function's blocks.
            bb = logicOrBlock;
            func->blocks.push_back(logicOrBlock);
            // deal with the logic and condition.
            conditionToIr(func, bb, lOrExp->lAndExp, trueBlock, falseBlock);
        }
    } else if (dynamic_cast<LAndExpNode *>(cond.get())) {
        const shared_ptr<LAndExpNode> lAndExp = s_p_c<LAndExpNode>(cond);
        if (lAndExp->lAndExp != nullptr) {
            // declare a new basic block as the 2nd condition judge block.
            shared_ptr<BasicBlock> logicAndBlock = make_shared<BasicBlock>(func, true, loopDepth);
            conditionToIr(func, bb, lAndExp->lAndExp, logicAndBlock, falseBlock);
            // maintain the predecessors and successors of last basic block.
            bb->successors.insert({falseBlock, logicAndBlock});
            falseBlock->predecessors.insert(bb);
            logicAndBlock->predecessors.insert(bb);
            // change bb to the 2nd condition block, and then push back it to function's blocks.
            bb = logicAndBlock;
            func->blocks.push_back(logicAndBlock);
        }
        // deal with the equal condition.
        conditionToIr(func, bb, lAndExp->eqExp, trueBlock, falseBlock);
    } else if (dynamic_cast<EqExpNode *>(cond.get())) {
        shared_ptr<Value> exp = expToIr(func, bb, cond);
        shared_ptr<Value> ins = make_shared<BranchInstruction>(exp, trueBlock, falseBlock, bb);
        addUser(ins, {exp});
        bb->instructions.push_back(s_p_c<Instruction>(ins));
    } else if (dynamic_cast<CondNode *>(cond.get())) {
        const shared_ptr<LOrExpNode> lOrExp = s_p_c<CondNode>(cond)->lOrExp;
        conditionToIr(func, bb, lOrExp, trueBlock, falseBlock);
    } else {
        cerr << "Error occurs in condition to IR: invalid expression type." << endl;
    }
}

/**
 * Transform the address of array, global value, global array or constant value to IR.
 */
void pointerToIr(const shared_ptr<LValNode> &lVal, shared_ptr<Value> &address,
                 shared_ptr<Value> &offset, shared_ptr<Function> &func, shared_ptr<BasicBlock> &bb) {
    shared_ptr<SymbolTableItem> identItem = lVal->ident->ident;
    switch (identItem->symbolType) {
        case SymbolType::VAR: {
            if (identItem->blockId.first == 0) {
                address = globalVariableMap.at(identItem->usageName);
                offset = getNumberValue(0);
                return;
            } else {
                cerr << "Error occurs in pointer to IR: cast a local variable." << endl;
                return;
            }
        }
        case SymbolType::CONST_ARRAY:
        case SymbolType::ARRAY: {
            int size = 1;
            for (int i = 1; i < identItem->numOfEachDimension.size(); ++i) size *= identItem->numOfEachDimension.at(i);
            offset = nullptr;
            for (int i = 0; i < lVal->exps.size(); ++i) {
                shared_ptr<Value> number = getNumberValue(size);
                shared_ptr<Value> off = expToIr(func, bb, lVal->exps.at(i));
                shared_ptr<Value> mul = make_shared<BinaryInstruction>(mulOp, off, number, bb);
                addUser(mul, {number, off});
                bb->instructions.push_back(s_p_c<Instruction>(mul));
                if (offset) {
                    shared_ptr<Value> oldOffset = offset;
                    offset = make_shared<BinaryInstruction>(addOp, offset, mul, bb);
                    addUser(offset, {oldOffset, mul});
                    bb->instructions.push_back(s_p_c<Instruction>(offset));
                } else {
                    offset = mul;
                }
                if (i + 1 < identItem->numOfEachDimension.size()) size /= identItem->numOfEachDimension.at(i + 1);
            }
            if (lVal->exps.size() < identItem->numOfEachDimension.size()) {
                shared_ptr<Value> oldOffset = offset;
                shared_ptr<Value> four = getNumberValue(_W_LEN);
                offset = make_shared<BinaryInstruction>(mulOp, offset, four, bb);
                addUser(offset, {oldOffset, four});
                bb->instructions.push_back(s_p_c<Instruction>(offset));
            }
            if (identItem->symbolType == SymbolType::CONST_ARRAY)
                address = globalConstantMap.at(identItem->usageName);
            else if (identItem->blockId.first == 0)
                address = globalVariableMap.at(identItem->usageName);
            else
                address = localArrayMap.at(identItem->usageName);
            return;
        }
        default:
            cerr << "Error occurs in pointer to IR: invalid symbol type." << endl;
    }
}
