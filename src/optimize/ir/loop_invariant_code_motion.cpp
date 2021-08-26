#include "ir_optimize.h"

#include <algorithm>
#include <queue>

unordered_map<shared_ptr<BasicBlock>, unordered_set<shared_ptr<BasicBlock>>> inDominate;
unordered_map<shared_ptr<BasicBlock>, unordered_set<shared_ptr<BasicBlock>>> outDominate;
unordered_map<shared_ptr<BasicBlock>, unordered_set<shared_ptr<BasicBlock>>> loopBlocks;
unordered_map<shared_ptr<BasicBlock>, shared_ptr<BasicBlock>> newForwardBlocks;

void loopInvariantMotion(shared_ptr<Function> &func);

void buildDominateTree(shared_ptr<BasicBlock> &entryBlock, shared_ptr<Function> &func);

void findLoopBlocks(shared_ptr<Function> &func);

void findInvariantCodes(shared_ptr<BasicBlock> &firstBlock);

void fixNewForwardBlock(shared_ptr<Function> &func, shared_ptr<BasicBlock> &firstBlock);

inline bool judgeInLoop(shared_ptr<Value> &value, unordered_set<shared_ptr<BasicBlock>> &blocksInLoop);

void loopInvariantCodeMotion(shared_ptr<Module> &module) {
    for (auto &func : module->functions) {
        loopInvariantMotion(func);
    }
}

void loopInvariantMotion(shared_ptr<Function> &func) {
    inDominate.clear();
    outDominate.clear();
    loopBlocks.clear();
    newForwardBlocks.clear();
    buildDominateTree(func->entryBlock, func);
    findLoopBlocks(func);
    for (auto &bb : func->blocks) {
        findInvariantCodes(bb);
    }
    for (auto &item : newForwardBlocks) {
        shared_ptr<BasicBlock> first = item.first;
        fixNewForwardBlock(func, first);
    }
}

void buildDominateTree(shared_ptr<BasicBlock> &entryBlock, shared_ptr<Function> &func) {
    unordered_set<shared_ptr<BasicBlock>> uSet;
    for (auto &bb : func->blocks) {
        uSet.insert(bb);
    }
    unordered_set<shared_ptr<BasicBlock>> tempSet;
    tempSet.insert(entryBlock);
    outDominate[entryBlock] = tempSet;
    for (auto &bb : func->blocks) {
        if (bb != entryBlock) {
            outDominate[bb] = uSet;
            tempSet.clear();
            inDominate[bb] = tempSet;
        }
    }
    bool outChanged;
    do {
        outChanged = false;
        for (auto &bb : func->blocks) {
            if (bb != entryBlock) {
                unordered_set<shared_ptr<BasicBlock>> inSet = uSet;
                for (auto &pred : bb->predecessors) {
                    tempSet = outDominate.at(pred);
                    for (auto item = inSet.begin(); item != inSet.end();) {
                        if (tempSet.count(*item) == 0) {
                            item = inSet.erase(item);
                        } else ++item;
                    }
                }
                inDominate[bb] = inSet;
                unordered_set<shared_ptr<BasicBlock>> outSet = inSet;
                outSet.insert(bb);
                if (outSet.size() != outDominate.at(bb).size()) outChanged = true;
                outDominate[bb] = outSet;
            }
        }
    } while (outChanged);
}

void findLoopBlocks(shared_ptr<Function> &func) {
    for (auto &bb : func->blocks) {
        for (auto &suc : bb->successors) {
            if (outDominate.at(bb).count(suc) != 0) {
                unordered_set<shared_ptr<BasicBlock>> tempLoopSet;
                queue<shared_ptr<BasicBlock>> loopQueue;
                loopQueue.push(bb);
                tempLoopSet.insert(bb);
                tempLoopSet.insert(suc);
                if (bb->loopDepth != suc->loopDepth) {
                    cerr << "Error occurs in process loop invariant code motion: "
                            "loop depth does not equal." << endl;
                }
                while (!loopQueue.empty()) {
                    shared_ptr<BasicBlock> top = loopQueue.front();
                    loopQueue.pop();
                    if (top != suc) {
                        for (auto &pred : top->predecessors) {
                            if (tempLoopSet.count(pred) == 0) {
                                if (pred->loopDepth < bb->loopDepth) {
                                    cerr << "Error occurs in process loop invariant code motion: "
                                            "predecessor's loop depth less than block." << endl;
                                }
                                tempLoopSet.insert(pred);
                                loopQueue.push(pred);
                            }
                        }
                    }
                }
                if (loopBlocks.count(suc) != 0) {
                    unordered_set<shared_ptr<BasicBlock>> newLoopSet = loopBlocks.at(suc);
                    newLoopSet.merge(tempLoopSet);
                    loopBlocks[suc] = newLoopSet;
                } else {
                    loopBlocks[suc] = tempLoopSet;
                }
            }
        }
    }
}

void findInvariantCodes(shared_ptr<BasicBlock> &firstBlock) {
    if (loopBlocks.count(firstBlock) == 0) return;
    unordered_set<shared_ptr<BasicBlock>> blocksInLoop = loopBlocks.at(firstBlock);
    for (auto &bb : loopBlocks.at(firstBlock)) {
        for (auto it = bb->instructions.begin(); it != bb->instructions.end();) {
            shared_ptr<Instruction> &ins = *it;
            bool motion = false;
            switch (ins->type) {
                case BINARY: {
                    shared_ptr<BinaryInstruction> bin = s_p_c<BinaryInstruction>(ins);
                    motion = !judgeInLoop(bin->lhs, blocksInLoop) && !judgeInLoop(bin->rhs, blocksInLoop);
                    break;
                }
                case UNARY: {
                    shared_ptr<UnaryInstruction> unary = s_p_c<UnaryInstruction>(ins);
                    motion = !judgeInLoop(unary->value, blocksInLoop);
                    break;
                }
                default:; // TODO: judge invoke, load or store.
            }
            if (motion) {
                if (newForwardBlocks.count(firstBlock) == 0) {
                    shared_ptr<BasicBlock> newBb = make_shared<BasicBlock>(firstBlock->function, true,
                                                                           firstBlock->loopDepth - 1);
                    newForwardBlocks[firstBlock] = newBb;
                }
                shared_ptr<BasicBlock> b = newForwardBlocks.at(firstBlock);
                b->instructions.push_back(ins);
                ins->block = b;
                if (ins->resultType == R_VAL_RESULT) {
                    ins->resultType = L_VAL_RESULT;
                    ins->caughtVarName = generateTempLeftValueName();
                }
                it = bb->instructions.erase(it);
            } else ++it;
        }
    }
}

void fixNewForwardBlock(shared_ptr<Function> &func, shared_ptr<BasicBlock> &firstBlock) {
    unordered_set<shared_ptr<BasicBlock>> blocksInLoop = loopBlocks.at(firstBlock);
    shared_ptr<BasicBlock> newBlock = newForwardBlocks.at(firstBlock);
    unordered_set<shared_ptr<BasicBlock>> predecessors = firstBlock->predecessors;
    shared_ptr<JumpInstruction> jumpIns = make_shared<JumpInstruction>(firstBlock, newBlock);
    newBlock->instructions.push_back(jumpIns);
    for (auto &pred : predecessors) {
        if (blocksInLoop.count(pred) == 0) {
            pred->successors.erase(firstBlock);
            firstBlock->predecessors.erase(pred);
            pred->successors.insert(newBlock);
            newBlock->predecessors.insert(pred);
            if (pred->instructions.empty()) {
                cerr << "Error occurs in process loop invariant motion: empty instruction vector." << endl;
            } else {
                auto it = pred->instructions.end() - 1;
                if ((*it)->type == JMP) {
                    s_p_c<JumpInstruction>(*it)->targetBlock = newBlock;
                } else if ((*it)->type == BR) {
                    shared_ptr<BranchInstruction> br = s_p_c<BranchInstruction>(*it);
                    if (br->trueBlock == firstBlock) br->trueBlock = newBlock;
                    if (br->falseBlock == firstBlock) br->falseBlock = newBlock;
                } else {
                    cerr << "Error occurs in process loop invariant motion: "
                            "the last instruction of a block is not branch or jump." << endl;
                }
            }
        }
    }
    newBlock->successors.insert(firstBlock);
    firstBlock->predecessors.insert(newBlock);
    for (auto &phi : firstBlock->phis) {
        unordered_map<shared_ptr<BasicBlock>, shared_ptr<Value>> operands = phi->operands;
        shared_ptr<PhiInstruction> newPhi = make_shared<PhiInstruction>(phi->localVarName, newBlock);
        for (auto &it : operands) {
            if (blocksInLoop.count(it.first) == 0) {
                shared_ptr<Value> val = it.second;
                newPhi->operands[it.first] = val;
                val->users.erase(phi);
                val->users.insert(newPhi);
                phi->operands.erase(it.first);
            }
        }
        if (!newPhi->operands.empty()) {
            phi->operands[newBlock] = newPhi;
            newPhi->users.insert(phi);
            newBlock->phis.insert(newPhi);
        }
    }
    unordered_set<shared_ptr<PhiInstruction>> phis = newBlock->phis;
    for (auto phi : phis) {
        removeTrivialPhi(phi);
    }
    phis = firstBlock->phis;
    for (auto phi : phis) {
        removeTrivialPhi(phi);
    }
    for (auto it = func->blocks.begin(); it != func->blocks.end(); ++it) {
        if (*it == firstBlock) {
            it = func->blocks.insert(it, newBlock) + 1;
        }
    }
}

inline bool judgeInLoop(shared_ptr<Value> &value, unordered_set<shared_ptr<BasicBlock>> &blocksInLoop) {
    return value->valueType == INSTRUCTION && blocksInLoop.count(s_p_c<Instruction>(value)->block) != 0;
}
