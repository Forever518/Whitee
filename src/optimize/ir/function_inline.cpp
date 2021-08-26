#include "ir_optimize.h"

string localVarName = "function_return_temp"; // NOLINT

const unsigned int MAX_INLINE_INSTRUCTION_CNT = 10000;
const unsigned int MAX_INLINE_WITH_POINTER_ARGUMENT = 20;

void inlineTargetFunction(shared_ptr<Function> &func);

void replaceInvoke(shared_ptr<Function> &callerFunc, shared_ptr<BasicBlock> &callerBlock,
                   shared_ptr<InvokeInstruction> &invoke, shared_ptr<Function> &toBeInline);

shared_ptr<Instruction>
copyInstruction(shared_ptr<Instruction> &toBeCopied, shared_ptr<PhiInstruction> &endPhi,
                shared_ptr<BasicBlock> &newBlock, shared_ptr<BasicBlock> &endBlock,
                unordered_map<shared_ptr<BasicBlock>, shared_ptr<BasicBlock>> &copyBlockMap,
                unordered_map<shared_ptr<Value>, shared_ptr<Value>> &copyVarMap);

shared_ptr<Value> findValueInMap(shared_ptr<Value> &value,
                                 unordered_map<shared_ptr<Value>, shared_ptr<Value>> &copyVarMap);

shared_ptr<BasicBlock> findBlockInMap(shared_ptr<BasicBlock> &bb,
                                      unordered_map<shared_ptr<BasicBlock>, shared_ptr<BasicBlock>> &copyBlockMap);

void functionInline(shared_ptr<Module> &module) {
    START_INLINE:
    auto it = module->functions.end() - 1;
    do {
        if ((*it)->name != "main" && (*it)->callers.count(*it) == 0
            && (*it)->fitInline(MAX_INLINE_INSTRUCTION_CNT, MAX_INLINE_WITH_POINTER_ARGUMENT)) {
            inlineTargetFunction(*it);
            deadCodeElimination(module);
            goto START_INLINE;
        }
        --it;
    } while (it != module->functions.begin() - 1);
}

void inlineTargetFunction(shared_ptr<Function> &func) {
    unordered_set<shared_ptr<Function>> callers = func->callers;
    for (auto caller : callers) {
        QUERY_BLOCK_LABEL:
        vector<shared_ptr<BasicBlock>> blocks = caller->blocks;
        for (auto bb : blocks) {
            vector<shared_ptr<Instruction>> instructions = bb->instructions;
            for (auto &ins : instructions) {
                if (ins->type == InstructionType::INVOKE) {
                    shared_ptr<InvokeInstruction> invoke = s_p_c<InvokeInstruction>(ins);
                    if (invoke->targetFunction == func) {
                        replaceInvoke(caller, bb, invoke, func);
                        goto QUERY_BLOCK_LABEL;
                    }
                }
            }
        }
        caller->callees.erase(func);
        func->callers.erase(caller);
        for (auto &callee : func->callees) {
            caller->callees.insert(callee);
            callee->callers.insert(caller);
        }
    }
}

/**
 * To inline an invoke instruction, new basic blocks need to be built.
 */
void replaceInvoke(shared_ptr<Function> &callerFunc, shared_ptr<BasicBlock> &callerBlock,
                   shared_ptr<InvokeInstruction> &invoke, shared_ptr<Function> &toBeInline) {
    unordered_map<shared_ptr<Value>, shared_ptr<Value>> funcInlineVarMap; // var in callee <--> var in caller
    unordered_map<shared_ptr<BasicBlock>, shared_ptr<BasicBlock>> funcInlineBlockMap; // bb in callee <--> bb in caller
    vector<shared_ptr<BasicBlock>> inlineBlocks;
    shared_ptr<BasicBlock> endBlock = make_shared<BasicBlock>(callerFunc, true, callerBlock->loopDepth);
    shared_ptr<PhiInstruction> endPhi;
    if (invoke->resultType == L_VAL_RESULT) {
        endPhi = make_shared<PhiInstruction>(invoke->caughtVarName, endBlock);
        endPhi->resultType = L_VAL_RESULT;
    } else {
        endPhi = make_shared<PhiInstruction>(localVarName, endBlock);
    }
    for (int i = 0; i < invoke->params.size(); ++i) {
        funcInlineVarMap[toBeInline->params.at(i)] = invoke->params.at(i);
        if (invoke->params.at(i)->valueType == ValueType::INSTRUCTION
            && s_p_c<Instruction>(invoke->params.at(i))->resultType != L_VAL_RESULT) {
            s_p_c<Instruction>(invoke->params.at(i))->resultType = L_VAL_RESULT;
            s_p_c<Instruction>(invoke->params.at(i))->caughtVarName = generateArgumentLeftValueName(toBeInline->name);
        }
    }
    // initialize all new blocks and new phis.
    vector<shared_ptr<BasicBlock>> toBeInlineBlocks = toBeInline->blocks;
    for (auto &it : toBeInlineBlocks) {
        shared_ptr<BasicBlock> tempBlock
                = make_shared<BasicBlock>(callerFunc, true, it->loopDepth + callerBlock->loopDepth);
        inlineBlocks.push_back(tempBlock);
        funcInlineBlockMap[it] = tempBlock;
        unordered_set<shared_ptr<PhiInstruction>> phis = it->phis;
        for (auto &phi : phis) {
            shared_ptr<PhiInstruction> tempPhi = make_shared<PhiInstruction>(phi->localVarName, tempBlock);
            funcInlineVarMap[phi] = tempPhi;
            tempBlock->phis.insert(tempPhi);
        }
    }
    // add the new block to the vector's end, and then update it's predecessors and successors.
    inlineBlocks.push_back(endBlock);
    endBlock->successors = callerBlock->successors;
    callerBlock->successors.clear();
    unordered_set<shared_ptr<BasicBlock>> successors = endBlock->successors;
    for (auto &s : successors) {
        s->predecessors.erase(callerBlock);
        s->predecessors.insert(endBlock);
        unordered_set<shared_ptr<PhiInstruction>> phis = s->phis;
        for (auto &phi : phis) {
            unordered_map<shared_ptr<BasicBlock>, shared_ptr<Value>> ops = phi->operands;
            for (auto &it : ops) {
                if (it.first == callerBlock) {
                    shared_ptr<Value> val = it.second;
                    phi->operands.erase(callerBlock);
                    phi->operands[endBlock] = val;
                }
            }
        }
    }
    // if return int replace all invoke with end phi.
    unordered_set<shared_ptr<Value>> invokeUsers = invoke->users;
    if (toBeInline->funcType == FuncType::FUNC_INT) {
        for (auto &user : invokeUsers) {
            shared_ptr<Value> invokeVal = invoke;
            shared_ptr<Value> endPhiVal = endPhi;
            user->replaceUse(invokeVal, endPhiVal);
        }
    }
    // automatically abandon invoke, because marking some IR members invalid is dangerous.
    vector<shared_ptr<Value>> invokeParams = invoke->params;
    for (auto &arg : invokeParams) {
        arg->users.erase(invoke);
    }
    invoke->valid = false;
    // put new instruction into map.
    toBeInlineBlocks = toBeInline->blocks;
    for (auto &it : toBeInlineBlocks) {
        vector<shared_ptr<Instruction>> instructions = it->instructions;
        for (auto &ins : instructions) {
            shared_ptr<Instruction> newIns = copyInstruction(ins, endPhi, funcInlineBlockMap.at(it), endBlock,
                                                             funcInlineBlockMap, funcInlineVarMap);
            if (ins->type == InstructionType::RET && toBeInline->funcType == FuncType::FUNC_INT) {
                funcInlineVarMap[ins] = endPhi;
            } else {
                funcInlineVarMap[ins] = newIns;
            }
        }
    }
    // update new phis.
    toBeInlineBlocks = toBeInline->blocks;
    for (auto &it : toBeInlineBlocks) {
        unordered_set<shared_ptr<PhiInstruction>> phis = it->phis;
        for (auto &phi : phis) {
            shared_ptr<Value> phiValue = phi;
            shared_ptr<PhiInstruction> newPhi
                    = s_p_c<PhiInstruction>(findValueInMap(phiValue, funcInlineVarMap));
            unordered_map<shared_ptr<BasicBlock>, shared_ptr<Value>> operands = phi->operands;
            for (auto &op : operands) {
                shared_ptr<BasicBlock> opBb = op.first;
                shared_ptr<Value> opVal = op.second;
                newPhi->operands[findBlockInMap(opBb, funcInlineBlockMap)]
                        = findValueInMap(opVal, funcInlineVarMap);
                addUser(newPhi, {findValueInMap(opVal, funcInlineVarMap)});
            }
            unordered_set<shared_ptr<Value>> users = phi->users;
            for (auto user : users) {
                if (user->valueType == ValueType::INSTRUCTION) {
                    if (dynamic_cast<ReturnInstruction *>(user.get())
                        && s_p_c<ReturnInstruction>(user)->funcType != FuncType::FUNC_INT)
                        continue;
                    shared_ptr<Value> newVal = findValueInMap(user, funcInlineVarMap);
                    newPhi->users.insert(newVal);
                } else {
                    cerr << "Error occurs in process copy instrucion: phi's user has other type." << endl;
                }
            }
        }
    }
    // update predecessors and successors.
    shared_ptr<BasicBlock> newEntryBlock = findBlockInMap(toBeInline->entryBlock, funcInlineBlockMap);
    newEntryBlock->predecessors.insert(callerBlock);
    callerBlock->successors.insert(newEntryBlock);
    toBeInlineBlocks = toBeInline->blocks;
    for (auto &it : toBeInlineBlocks) {
        shared_ptr<BasicBlock> newBlock = findBlockInMap(it, funcInlineBlockMap);
        for (auto s : it->successors) {
            newBlock->successors.insert(findBlockInMap(s, funcInlineBlockMap));
        }
        for (auto p : it->predecessors) {
            newBlock->predecessors.insert(findBlockInMap(p, funcInlineBlockMap));
        }
    }
    // split basic block.
    for (auto it = callerBlock->instructions.begin(); it != callerBlock->instructions.end(); ++it) {
        if (*it == invoke) {
            endBlock->instructions = vector(it + 1, callerBlock->instructions.end());
            callerBlock->instructions.erase(it, callerBlock->instructions.end());
            for (auto &ins : endBlock->instructions) {
                ins->block = endBlock;
            }
            break;
        }
    }

    // insert blocks into function.
    for (auto it = callerFunc->blocks.begin(); it != callerFunc->blocks.end(); ++it) {
        if (*it == callerBlock) {
            callerFunc->blocks.insert(it + 1, inlineBlocks.begin(), inlineBlocks.end());
            break;
        }
    }    // judge if return.
    if (toBeInline->funcType == FuncType::FUNC_INT) {
        endBlock->phis.insert(endPhi);
        removeTrivialPhi(endPhi);
    }
    shared_ptr<BasicBlock> callerBlockTargetBlock = findBlockInMap(toBeInline->entryBlock, funcInlineBlockMap);
    shared_ptr<Instruction> jumpIns = make_shared<JumpInstruction>(callerBlockTargetBlock, callerBlock);
    callerBlock->instructions.push_back(jumpIns);
    // remove trivial phis.
    for (auto &it : inlineBlocks) {
        unordered_set<shared_ptr<PhiInstruction>> phis = it->phis;
        for (auto phi : phis) {
            removeTrivialPhi(phi);
        }
    }
}

shared_ptr<Instruction>
copyInstruction(shared_ptr<Instruction> &toBeCopied, shared_ptr<PhiInstruction> &endPhi,
                shared_ptr<BasicBlock> &newBlock, shared_ptr<BasicBlock> &endBlock,
                unordered_map<shared_ptr<BasicBlock>, shared_ptr<BasicBlock>> &copyBlockMap,
                unordered_map<shared_ptr<Value>, shared_ptr<Value>> &copyVarMap) {
    shared_ptr<Instruction> ret;
    switch (toBeCopied->type) {
        case InstructionType::ALLOC: {
            shared_ptr<AllocInstruction> alloc = s_p_c<AllocInstruction>(toBeCopied);
            ret = make_shared<AllocInstruction>(alloc->name, alloc->bytes, alloc->units, newBlock);
            break;
        }
        case InstructionType::INVOKE: {
            shared_ptr<InvokeInstruction> invoke = s_p_c<InvokeInstruction>(toBeCopied);
            vector<shared_ptr<Value>> newParams;
            for (auto &it : invoke->params) {
                newParams.push_back(findValueInMap(it, copyVarMap));
            }
            if (invoke->invokeType == COMMON) {
                ret = make_shared<InvokeInstruction>(invoke->targetFunction, newParams, newBlock);
            } else {
                ret = make_shared<InvokeInstruction>(invoke->targetName, newParams, newBlock);
            }
            addUser(ret, newParams);
            break;
        }
        case InstructionType::JMP: {
            shared_ptr<JumpInstruction> jump = s_p_c<JumpInstruction>(toBeCopied);
            shared_ptr<BasicBlock> targetBlock = findBlockInMap(jump->targetBlock, copyBlockMap);
            ret = make_shared<JumpInstruction>(targetBlock, newBlock);
            break;
        }
        case InstructionType::BR: {
            shared_ptr<BranchInstruction> br = s_p_c<BranchInstruction>(toBeCopied);
            shared_ptr<Value> cond = findValueInMap(br->condition, copyVarMap);
            shared_ptr<BasicBlock> trueBlock = findBlockInMap(br->trueBlock, copyBlockMap);
            shared_ptr<BasicBlock> falseBlock = findBlockInMap(br->falseBlock, copyBlockMap);
            ret = make_shared<BranchInstruction>(cond, trueBlock, falseBlock, newBlock);
            addUser(ret, {cond});
            break;
        }
        case InstructionType::STORE: {
            shared_ptr<StoreInstruction> store = s_p_c<StoreInstruction>(toBeCopied);
            shared_ptr<Value> val = findValueInMap(store->value, copyVarMap);
            shared_ptr<Value> add = findValueInMap(store->address, copyVarMap);
            shared_ptr<Value> off = findValueInMap(store->offset, copyVarMap);
            ret = make_shared<StoreInstruction>(val, add, off, newBlock);
            addUser(ret, {val, add, off});
            break;
        }
        case InstructionType::RET: {
            shared_ptr<ReturnInstruction> retIns = s_p_c<ReturnInstruction>(toBeCopied);
            shared_ptr<BasicBlock> returnBlock = findBlockInMap(retIns->block, copyBlockMap);
            endBlock->predecessors.insert(returnBlock);
            returnBlock->successors.insert(endBlock);
            if (retIns->funcType == FuncType::FUNC_INT) {
                shared_ptr<Value> retVal = findValueInMap(retIns->value, copyVarMap);
                endPhi->operands[returnBlock] = retVal;
                if (retVal->valueType == ValueType::INSTRUCTION
                    && s_p_c<Instruction>(retVal)->resultType != L_VAL_RESULT) {
                    s_p_c<Instruction>(retVal)->resultType = L_VAL_RESULT;
                    s_p_c<Instruction>(retVal)->caughtVarName = generatePhiLeftValueName(endPhi->caughtVarName);
                }
                addUser(endPhi, {retVal});
            }
            ret = make_shared<JumpInstruction>(endBlock, newBlock);
            break;
        }
        case InstructionType::LOAD: {
            shared_ptr<LoadInstruction> load = s_p_c<LoadInstruction>(toBeCopied);
            shared_ptr<Value> add = findValueInMap(load->address, copyVarMap);
            shared_ptr<Value> off = findValueInMap(load->offset, copyVarMap);
            ret = make_shared<LoadInstruction>(add, off, newBlock);
            addUser(ret, {add, off});
            break;
        }
        case InstructionType::UNARY: {
            shared_ptr<UnaryInstruction> unary = s_p_c<UnaryInstruction>(toBeCopied);
            shared_ptr<Value> val = findValueInMap(unary->value, copyVarMap);
            ret = make_shared<UnaryInstruction>(unary->op, val, newBlock);
            addUser(ret, {val});
            break;
        }
        case InstructionType::CMP:
        case InstructionType::BINARY: {
            shared_ptr<BinaryInstruction> binary = s_p_c<BinaryInstruction>(toBeCopied);
            shared_ptr<Value> lhs = findValueInMap(binary->lhs, copyVarMap);
            shared_ptr<Value> rhs = findValueInMap(binary->rhs, copyVarMap);
            ret = make_shared<BinaryInstruction>(binary->op, lhs, rhs, newBlock);
            addUser(ret, {lhs, rhs});
            break;
        }
        default:;
    }
    newBlock->instructions.push_back(ret);
    if (toBeCopied->resultType == L_VAL_RESULT) {
        ret->resultType = L_VAL_RESULT;
        ret->caughtVarName = toBeCopied->caughtVarName;
    }
    return ret;
}

shared_ptr<Value> findValueInMap(shared_ptr<Value> &value,
                                 unordered_map<shared_ptr<Value>, shared_ptr<Value>> &copyVarMap) {
    if (copyVarMap.count(value) != 0) return copyVarMap.at(value);
    if (value->valueType == ValueType::INSTRUCTION) {
        cerr << "Error occurs in process function inline: uncaught instruction." << endl;
        return nullptr;
    }
    return value;
}

shared_ptr<BasicBlock> findBlockInMap(shared_ptr<BasicBlock> &bb,
                                      unordered_map<shared_ptr<BasicBlock>, shared_ptr<BasicBlock>> &copyBlockMap) {
    if (copyBlockMap.count(bb) != 0) return copyBlockMap.at(bb);
    cerr << "Error occurs in process function inline: undefined new block." << endl;
    return nullptr;
}
