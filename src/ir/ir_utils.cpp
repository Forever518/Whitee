#include "ir_utils.h"

#include <iostream>
#include <queue>

const unordered_set<InstructionType> noResultTypes{ // NOLINT
        InstructionType::BR,
        InstructionType::JMP,
        InstructionType::RET,
        InstructionType::INVOKE,
        InstructionType::STORE
};

unordered_set<shared_ptr<BasicBlock>> blockRelationTree;

void buildBlockRelationTree(const shared_ptr<BasicBlock> &bb);

void removeUnusedInstructions(shared_ptr<BasicBlock> &bb) {
    auto it = bb->instructions.begin();
    while (it != bb->instructions.end()) {
        if (noResultTypes.count((*it)->type) == 0 && (*it)->users.empty()) {
            (*it)->abandonUse();
            it = bb->instructions.erase(it);
        } else if (noResultTypes.count((*it)->type) != 0 && !(*it)->valid) {
            it = bb->instructions.erase(it);
        } else if ((*it)->type == INVOKE && (*it)->users.empty()) {
            shared_ptr<InvokeInstruction> invoke = s_p_c<InvokeInstruction>(*it);
            if (invoke->invokeType == COMMON && !invoke->targetFunction->hasSideEffect) {
                (*it)->abandonUse();
                it = bb->instructions.erase(it);
            } else ++it;
        } else ++it;
    }
    VISIT_ALL_PHIS:
    unordered_set<shared_ptr<PhiInstruction>> phis = bb->phis;
    for (auto phi : phis) {
        if (phi->onlyHasBlockUserOrUserEmpty()) {
            phi->abandonUse();
            bb->phis.erase(phi);
            continue;
        }
        unordered_map<shared_ptr<BasicBlock>, shared_ptr<Value>> operands = phi->operands;
        bool op = false;
        for (auto &entry : operands) {
            if (bb->predecessors.count(entry.first) == 0) {
                if (phi->getOperandValueCount(entry.second) == 1)
                    entry.second->users.erase(phi);
                phi->operands.erase(entry.first);
                op = true;
            }
        }
        if (op) {
            removeTrivialPhi(phi);
            goto VISIT_ALL_PHIS;
        }
    }
}

void removeUnusedBasicBlocks(shared_ptr<Function> &func) {
    blockRelationTree.clear();
    buildBlockRelationTree(func->entryBlock);
    auto it = func->blocks.begin();
    while (it != func->blocks.end()) {
        if (blockRelationTree.count(*it) == 0) {
            for (int i = (*it)->instructions.size() - 1; i >= 0; --i) {
                shared_ptr<Value> selfIns = (*it)->instructions.at(i);
                unordered_set<shared_ptr<Value>> users = selfIns->users;
                for (auto &user : users) {
                    if (user->valueType == ValueType::INSTRUCTION) {
                        shared_ptr<Instruction> userIns = s_p_c<Instruction>(user);
                        if (userIns->block != *it && userIns->block->valid) {
                            if (userIns->type == InstructionType::PHI) {
                                shared_ptr<PhiInstruction> phi = s_p_c<PhiInstruction>(userIns);
                                unordered_map<shared_ptr<BasicBlock>, shared_ptr<Value>> operands = phi->operands;
                                for (auto &op : operands) {
                                    if (op.first == *it || op.second == selfIns) {
                                        phi->operands.erase(op.first);
                                    }
                                }
                                removeTrivialPhi(phi);
                            } else {
                                string undefinedName = "unused block's instruction " + to_string(selfIns->id);
                                shared_ptr<Value> newVal = make_shared<UndefinedValue>(undefinedName);
                                user->replaceUse(selfIns, newVal);
                            }
                        }
                    }
                }
                selfIns->abandonUse();
            }
            (*it)->abandonUse();
            it = func->blocks.erase(it);
        } else ++it;
    }
}

void removeUnusedFunctions(shared_ptr<Module> &module) {
    auto func = module->functions.begin();
    while (func != module->functions.end()) {
        if ((*func)->callers.empty() && (*func)->name != "main") {
            (*func)->abandonUse();
            func = module->functions.erase(func);
        } else ++func;
    }
}

void removeBlockPredecessor(shared_ptr<BasicBlock> &bb, shared_ptr<BasicBlock> &pre) {
    pre->successors.erase(bb);
    bb->predecessors.erase(pre);
    if (bb->predecessors.empty()) {
        unordered_set<shared_ptr<BasicBlock>> successorsCopy = bb->successors;
        bb->successors.clear();
        for (auto b : successorsCopy) {
            removeBlockPredecessor(b, bb);
        }
    }
}

void buildBlockRelationTree(const shared_ptr<BasicBlock> &bb) {
    blockRelationTree.insert(bb);
    for (const shared_ptr<BasicBlock> &b : bb->successors) {
        if (blockRelationTree.count(b) == 0) {
            buildBlockRelationTree(b);
        }
    }
}

void countFunctionSideEffect(shared_ptr<Module> &module) {
    for (auto &func : module->functions) {
        func->hasSideEffect = false;
        for (auto &arg : func->params) {
            if (s_p_c<ParameterValue>(arg)->variableType == VariableType::POINTER) {
                func->hasSideEffect = true;
                goto FUNC_SIDE_EFFECT_CONTINUE;
            }
        }
        for (auto &bb : func->blocks) {
            for (auto &ins : bb->instructions) {
                if (ins->type == InstructionType::STORE) {
                    if (s_p_c<StoreInstruction>(ins)->address->valueType != INSTRUCTION) {
                        func->hasSideEffect = true;
                        goto FUNC_SIDE_EFFECT_CONTINUE;
                    }
                } else if (ins->type == InstructionType::LOAD) {
                    if (s_p_c<LoadInstruction>(ins)->address->valueType != INSTRUCTION) {
                        func->hasSideEffect = true;
                        goto FUNC_SIDE_EFFECT_CONTINUE;
                    }
                } else if (ins->type == InstructionType::BINARY) {
                    if (s_p_c<BinaryInstruction>(ins)->lhs->valueType == ValueType::CONSTANT
                        || s_p_c<BinaryInstruction>(ins)->lhs->valueType == ValueType::GLOBAL
                        || s_p_c<BinaryInstruction>(ins)->rhs->valueType == ValueType::CONSTANT
                        || s_p_c<BinaryInstruction>(ins)->rhs->valueType == ValueType::GLOBAL) {
                        func->hasSideEffect = true;
                        goto FUNC_SIDE_EFFECT_CONTINUE;
                    }
                } else if (ins->type == InstructionType::INVOKE) {
                    if (s_p_c<InvokeInstruction>(ins)->invokeType != COMMON) {
                        func->hasSideEffect = true;
                        goto FUNC_SIDE_EFFECT_CONTINUE;
                    } else if (s_p_c<InvokeInstruction>(ins)->targetFunction->hasSideEffect) {
                        func->hasSideEffect = true;
                        goto FUNC_SIDE_EFFECT_CONTINUE;
                    }
                    for (auto &arg : s_p_c<InvokeInstruction>(ins)->params) {
                        if (arg->valueType == ValueType::GLOBAL || arg->valueType == ValueType::CONSTANT) {
                            func->hasSideEffect = true;
                            goto FUNC_SIDE_EFFECT_CONTINUE;
                        }
                    }
                }
            }
        }
        FUNC_SIDE_EFFECT_CONTINUE:
        continue;
    }
}

void addUser(const shared_ptr<Value> &user, initializer_list<shared_ptr<Value>> used) {
    for (const auto &u : used) {
        u->users.insert(user);
    }
}

void addUser(const shared_ptr<Value> &user, const vector<shared_ptr<Value>> &used) {
    for (const auto &u : used) {
        u->users.insert(user);
    }
}

void removePhiUserBlocksAndMultiCmp(shared_ptr<Module> &module) {
    for (auto &func : module->functions) {
        for (auto &bb : func->blocks) {
            for (auto ins = bb->instructions.begin(); ins != bb->instructions.end(); ++ins) {
                if ((*ins)->type == InstructionType::CMP && ins + 1 != bb->instructions.end()) {
                    if ((*(ins + 1))->type != InstructionType::BR) {
                        (*ins)->type = InstructionType::BINARY;
                    }
                }
            }
            for (auto &phi : bb->phis) {
                unordered_set<shared_ptr<Value>> users = phi->users;
                for (auto &user : users) {
                    if (user->valueType == ValueType::BASIC_BLOCK) {
                        phi->users.erase(user);
                    }
                }
            }
        }
    }
}

void fixRightValue(shared_ptr<Module> &module) {
    // pre-deal with the alloc and non-users function call.
    for (auto &func : module->functions) {
        for (auto &bb : func->blocks) {
            for (auto &ins : bb->instructions) {
                if (ins->type == ALLOC) {
                    ins->resultType = OTHER_RESULT;
                } else if (ins->type == INVOKE && s_p_c<InvokeInstruction>(ins)->users.empty()) {
                    s_p_c<InvokeInstruction>(ins)->resultType = OTHER_RESULT;
                }
            }
        }
    }
    for (auto &func : module->functions) {
        for (auto &bb : func->blocks) {
            for (auto &ins : bb->instructions) {
                if (ins->users.size() == 1 && ins->resultType == R_VAL_RESULT) {
                    if (ins->users.begin()->get()->valueType == ValueType::INSTRUCTION
                        && ins->block != s_p_c<Instruction>(*ins->users.begin())->block) {
                        ins->resultType = L_VAL_RESULT;
                        ins->caughtVarName = generateTempLeftValueName();
                    }
                }
            }
        }
    }
}

void getFunctionRequiredStackSize(shared_ptr<Function> &func) {
    unsigned int size = 4 * _W_LEN;
    unordered_set<shared_ptr<Value>> phiMovSet;
    for (auto &bb : func->blocks) {
        for (auto &ins : bb->instructions) {
            if (ins->type == PHI_MOV && phiMovSet.count(ins) == 0 && func->variableRegs.count(ins) == 0) {
                phiMovSet.insert(ins);
                size += _W_LEN;
            } else if (ins->resultType == L_VAL_RESULT && func->variableRegs.count(ins) == 0) {
                size += _W_LEN;
            } else if (ins->type == ALLOC) {
                size += s_p_c<AllocInstruction>(ins)->bytes;
            }
        }
    }
    func->requiredStackSize = size;
}

void phiElimination(shared_ptr<Function> &func) {
    for (auto &bb : func->blocks) {
        for (auto phi : bb->phis) {
            shared_ptr<Instruction> phiMov = make_shared<PhiMoveInstruction>(phi);
            for (auto &operand : phi->operands) {
                shared_ptr<BasicBlock> pred = operand.first;
                if (!pred->instructions.empty()) {
                    auto it = pred->instructions.end() - 1;
                    if ((*it)->type == JMP) {
                        pred->instructions.insert(it, phiMov);
                    } else if ((*it)->type == BR) {
                        if (it != pred->instructions.begin() && (*(it - 1))->type == CMP) {
                            pred->instructions.insert(it - 1, phiMov);
                        } else {
                            pred->instructions.insert(it, phiMov);
                        }
                    }
                } else {
                    cerr << "Error occurs in process phi elimination: empty predecessor." << endl;
                }
            }
            phi->phiMove = s_p_c<PhiMoveInstruction>(phiMov);
            bb->instructions.insert(bb->instructions.begin(), phi);
        }
    }
}

void mergeAliveValuesToInstruction(shared_ptr<Function> &func) {
    for (auto &bb : func->blocks) {
        for (auto &ins : bb->instructions) {
            unordered_set<shared_ptr<Value>> blockAliveValues = bb->aliveValues;
            ins->aliveValues.merge(blockAliveValues);
        }
    }
}
