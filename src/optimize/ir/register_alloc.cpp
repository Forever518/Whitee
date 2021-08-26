#include "ir_optimize.h"

#include <stack>
#include <queue>
#include <ctime>
#include <set>

time_t startAllocTime;
bool conflictGraphBuildSuccess;
unsigned long CONFLICT_GRAPH_TIMEOUT = 10;

unordered_map<shared_ptr<Value>, shared_ptr<unordered_set<shared_ptr<Value>>>> conflictGraph;
unordered_map<shared_ptr<BasicBlock>, shared_ptr<unordered_map<shared_ptr<BasicBlock>,
        unordered_set<shared_ptr<BasicBlock>>>>> blockPath;

inline bool checkRegAllocTimeout(time_t startTime, unsigned long timeout) {
    if (time(nullptr) - startTime > timeout) {
        conflictGraphBuildSuccess = false;
        return true;
    }
    return false;
}

void initConflictGraph(shared_ptr<Function> &func);

void buildConflictGraph(shared_ptr<Function> &func);

void allocRegister(shared_ptr<Function> &func);

void addAliveValue(shared_ptr<Instruction> &value, shared_ptr<Instruction> &owner,
                   shared_ptr<BasicBlock> &ownerBlock);

void addAliveValue(shared_ptr<Value> &value, shared_ptr<Instruction> &owner,
                   shared_ptr<BasicBlock> &ownerBlock);

unordered_set<shared_ptr<BasicBlock>>
getBlockPathPoints(shared_ptr<BasicBlock> &from, shared_ptr<BasicBlock> &to);

void getBlockReachableBlocks(shared_ptr<BasicBlock> &bb, shared_ptr<BasicBlock> &cannotArrive,
                             unordered_set<shared_ptr<BasicBlock>> &ans);

void outputConflictGraph(const string &funcName);

void registerAlloc(shared_ptr<Function> &func) {
    conflictGraph.clear();
    blockPath.clear();
    initConflictGraph(func);
    conflictGraphBuildSuccess = true;
    startAllocTime = time(nullptr);
    buildConflictGraph(func);
    if (_debugIrOptimize) outputConflictGraph(func->name);
    if (conflictGraphBuildSuccess) allocRegister(func);
    else {
        cerr << "Time out when building conflict graph of function " << func->name << "." << endl;
    }
}

void initConflictGraph(shared_ptr<Function> &func) {
    for (auto &arg : func->params) {
        shared_ptr<unordered_set<shared_ptr<Value>>> tempSet
                = make_shared<unordered_set<shared_ptr<Value>>>();
        conflictGraph[arg] = tempSet;
    }
    for (auto &bb : func->blocks) {
        for (auto &ins : bb->instructions) {
            if (ins->resultType == L_VAL_RESULT && conflictGraph.count(ins) == 0) {
                shared_ptr<unordered_set<shared_ptr<Value>>> tempSet
                        = make_shared<unordered_set<shared_ptr<Value>>>();
                conflictGraph[ins] = tempSet;
            }
        }
    }
}

/**
 * These codes maybe redundant.
 * @param func
 */
void buildConflictGraph(shared_ptr<Function> &func) {
    /// judge parameters.
    for (auto &ins : func->params) {
        if (checkRegAllocTimeout(startAllocTime, CONFLICT_GRAPH_TIMEOUT)) return;
        for (auto &user : ins->users) {
            if (checkRegAllocTimeout(startAllocTime, CONFLICT_GRAPH_TIMEOUT)) return;
            if (user->valueType != INSTRUCTION) {
                cerr << "Error occurs in register alloc: got a non-instruction user." << endl;
            }
            shared_ptr<Instruction> userIns = s_p_c<Instruction>(user);
            if (userIns->type == PHI) {
                shared_ptr<PhiMoveInstruction> phiMov = s_p_c<PhiInstruction>(userIns)->phiMove;
                for (auto &op : phiMov->phi->operands) {
                    if (op.second == ins) {
                        shared_ptr<BasicBlock> phiMovLocation = op.first;
                        if (phiMovLocation->aliveValues.count(ins) != 0) continue;
                        else if (phiMov->blockALiveValues.at(phiMovLocation).count(ins) != 0) continue;
                        auto it = func->entryBlock->instructions.begin();
                        bool searchOtherBlocks = true;
                        while (it != func->entryBlock->instructions.end()) {
                            addAliveValue(ins, *it, func->entryBlock);
                            if (*it == phiMov && phiMovLocation == func->entryBlock) {
                                searchOtherBlocks = false;
                                break;
                            }
                            ++it;
                        }
                        if (searchOtherBlocks) {
                            unordered_set<shared_ptr<BasicBlock>> pathNodes;
                            if (blockPath.count(func->entryBlock) != 0) {
                                if (blockPath.at(func->entryBlock)->count(phiMovLocation) != 0) {
                                    pathNodes = blockPath.at(func->entryBlock)->at(phiMovLocation);
                                } else {
                                    pathNodes = getBlockPathPoints(func->entryBlock, phiMovLocation);
                                }
                            } else pathNodes = getBlockPathPoints(func->entryBlock, phiMovLocation);
                            for (auto &b : pathNodes) {
                                b->aliveValues.insert(ins);
                            }
                            it = phiMovLocation->instructions.begin();
                            while (it != phiMovLocation->instructions.end()) {
                                addAliveValue(ins, *it, phiMovLocation);
                                if (*it == phiMov) break;
                                ++it;
                            }
                        }
                    }
                }
            } else {
                if (userIns->block->aliveValues.count(ins) != 0 || userIns->aliveValues.count(ins) != 0) {
                    continue;
                } else { // mark this instruction alive all along the path to userIns.
                    auto it = func->entryBlock->instructions.begin();
                    bool searchOtherBlocks = true;
                    while (it != func->entryBlock->instructions.end()) {
                        addAliveValue(ins, *it, func->entryBlock);
                        if (*it == userIns) {
                            searchOtherBlocks = false;
                            break;
                        }
                        ++it;
                    }
                    // the user is not just after the insVal, search for block paths.
                    if (searchOtherBlocks) {
                        // maintain the passing block and add them to alive nodes.
                        unordered_set<shared_ptr<BasicBlock>> pathNodes;
                        if (blockPath.count(func->entryBlock) != 0) {
                            if (blockPath.at(func->entryBlock)->count(userIns->block) != 0) {
                                pathNodes = blockPath.at(func->entryBlock)->at(userIns->block);
                            } else {
                                pathNodes = getBlockPathPoints(func->entryBlock, userIns->block);
                            }
                        } else {
                            pathNodes = getBlockPathPoints(func->entryBlock, userIns->block);
                        }
                        for (auto &b : pathNodes) {
                            b->aliveValues.insert(ins);
                        }
                        // search the user block and mark alive.
                        it = userIns->block->instructions.begin();
                        while (it != userIns->block->instructions.end()) {
                            addAliveValue(ins, *it, userIns->block);
                            if (*it == userIns) {
                                break;
                            }
                            ++it;
                        }
                    }
                }
            }
        }
    }
    /// judge instructions.
    for (auto &bb : func->blocks) {
        if (checkRegAllocTimeout(startAllocTime, CONFLICT_GRAPH_TIMEOUT)) return;
        for (auto ins = bb->instructions.begin(); ins != bb->instructions.end(); ++ins) {
            if (checkRegAllocTimeout(startAllocTime, CONFLICT_GRAPH_TIMEOUT)) return;
            shared_ptr<Instruction> insVal = *ins;
            if (insVal->type != PHI_MOV && insVal->resultType == L_VAL_RESULT) {
                for (auto &user : insVal->users) {
                    if (checkRegAllocTimeout(startAllocTime, CONFLICT_GRAPH_TIMEOUT)) return;
                    if (user->valueType != INSTRUCTION) {
                        cerr << "Error occurs in register alloc: got a non-instruction user." << endl;
                    }
                    shared_ptr<Instruction> userIns = s_p_c<Instruction>(user);
                    if (userIns->type == PHI) { // deal with phi user.
                        // find the phi move location(can be many).
                        shared_ptr<PhiMoveInstruction> phiMov = s_p_c<PhiInstruction>(userIns)->phiMove;
                        for (auto &op : phiMov->phi->operands) {
                            if (op.second == insVal) {
                                shared_ptr<BasicBlock> phiMovLocation = op.first;
                                if (phiMovLocation->aliveValues.count(insVal) != 0) {
                                    continue;
                                } else if (phiMov->blockALiveValues.at(phiMovLocation).count(insVal) != 0) {
                                    continue;
                                }
                                auto it = ins + 1;
                                bool searchOtherBlocks = true;
                                while (it != bb->instructions.end()) {
                                    addAliveValue(insVal, *it, bb);
                                    if (*it == phiMov && phiMovLocation == bb) {
                                        searchOtherBlocks = false;
                                        break;
                                    }
                                    ++it;
                                }
                                if (searchOtherBlocks) {
                                    unordered_set<shared_ptr<BasicBlock>> pathNodes;
                                    if (blockPath.count(bb) != 0) {
                                        if (blockPath.at(bb)->count(phiMovLocation) != 0) {
                                            pathNodes = blockPath.at(bb)->at(phiMovLocation);
                                        } else {
                                            pathNodes = getBlockPathPoints(bb, phiMovLocation);
                                        }
                                    } else pathNodes = getBlockPathPoints(bb, phiMovLocation);
                                    for (auto &b : pathNodes) {
                                        b->aliveValues.insert(insVal);
                                    }
                                    it = phiMovLocation->instructions.begin();
                                    while (it != phiMovLocation->instructions.end()) {
                                        addAliveValue(insVal, *it, phiMovLocation);
                                        if (*it == phiMov) break;
                                        ++it;
                                    }
                                }
                            }
                        }
                    } else {
                        // if the user block or the instruction has mark this instruction alive, continue.
                        if (userIns->block->aliveValues.count(insVal) != 0 || userIns->aliveValues.count(insVal) != 0) {
                            continue;
                        } else { // mark this instruction alive all along the path to userIns.
                            auto it = ins + 1;
                            bool searchOtherBlocks = true;
                            while (it != bb->instructions.end()) {
                                addAliveValue(insVal, *it, bb);
                                if (*it == userIns) {
                                    searchOtherBlocks = false;
                                    break;
                                }
                                ++it;
                            }
                            // the user is not just after the insVal, search for block paths.
                            if (searchOtherBlocks) {
                                // maintain the passing block and add them to alive nodes.
                                unordered_set<shared_ptr<BasicBlock>> pathNodes;
                                if (blockPath.count(bb) != 0) {
                                    if (blockPath.at(bb)->count(userIns->block) != 0) {
                                        pathNodes = blockPath.at(bb)->at(userIns->block);
                                    } else {
                                        pathNodes = getBlockPathPoints(bb, userIns->block);
                                    }
                                } else {
                                    pathNodes = getBlockPathPoints(bb, userIns->block);
                                }
                                for (auto &b : pathNodes) {
                                    b->aliveValues.insert(insVal);
                                }
                                // search the user block and mark alive.
                                it = userIns->block->instructions.begin();
                                while (it != userIns->block->instructions.end()) {
                                    addAliveValue(insVal, *it, userIns->block);
                                    if (*it == userIns) {
                                        break;
                                    }
                                    ++it;
                                }
                            }
                        }
                    }
                }
            } else if (insVal->type == PHI_MOV) { // deal with phi move.
                shared_ptr<PhiMoveInstruction> phiMove = s_p_c<PhiMoveInstruction>(insVal);
                shared_ptr<BasicBlock> targetPhiBlock = phiMove->phi->block;
                auto it = ins + 1;
                while (it != bb->instructions.end()) {
                    addAliveValue(insVal, *it, bb);
                    ++it;
                }
                if (targetPhiBlock->aliveValues.count(insVal) != 0
                    || phiMove->phi->aliveValues.count(insVal) != 0)
                    continue;
                it = targetPhiBlock->instructions.begin();
                while (it != targetPhiBlock->instructions.end()) {
                    addAliveValue(insVal, *it, targetPhiBlock);
                    if (*it == phiMove->phi) break;
                    ++it;
                }
            }
        }
    }
    for (auto &bb : func->blocks) {
        for (auto &aliveVal : bb->aliveValues) {
            for (auto &aliveValInside : bb->aliveValues) {
                if (aliveVal != aliveValInside) {
                    if (conflictGraph.count(aliveVal) == 0) {
                        cerr << "Error occurs in process register alloc: "
                                "conflict graph does not have a l-value." << endl;
                    } else {
                        conflictGraph.at(aliveVal)->insert(aliveValInside);
                    }
                }
            }
            for (auto &ins : bb->instructions) {
                if (ins->resultType == L_VAL_RESULT) {
                    conflictGraph.at(aliveVal)->insert(ins);
                    conflictGraph.at(ins)->insert(aliveVal);
                }
            }
        }
        for (auto &ins : bb->instructions) {
            unordered_set<shared_ptr<Value>> aliveValues;
            if (ins->type != PHI_MOV) {
                aliveValues = ins->aliveValues;
            } else {
                aliveValues = s_p_c<PhiMoveInstruction>(ins)->blockALiveValues.at(bb);
            }
            for (auto &aliveVal : aliveValues) {
                for (auto &aliveValInside : aliveValues) {
                    if (aliveVal != aliveValInside) {
                        if (conflictGraph.count(aliveVal) == 0) {
                            cerr << "Error occurs in process register alloc: "
                                    "conflict graph does not have a l-value." << endl;
                        } else {
                            conflictGraph.at(aliveVal)->insert(aliveValInside);
                        }
                    }
                }
            }
        }
    }
}

void allocRegister(shared_ptr<Function> &func) {
    stack<shared_ptr<Value>> variableWithRegs;
    unordered_map<shared_ptr<Value>, shared_ptr<unordered_set<shared_ptr<Value>>>> tempGraph = conflictGraph;
    ALLOC_REGISTER_START:
    for (auto &it : tempGraph) {
        shared_ptr<Value> var = it.first;
        if (it.second->size() < _GLB_REG_CNT) {
            for (auto &val : *it.second) {
                tempGraph.at(val)->erase(var);
            }
            tempGraph.erase(var);
            variableWithRegs.push(var);
            goto ALLOC_REGISTER_START;
        }
    }
    if (!tempGraph.empty()) {
        shared_ptr<Value> abandon = tempGraph.begin()->first;
        for (auto &it : tempGraph) {
            shared_ptr<Value> ins = it.first;
            if (func->variableWeight.at(ins) < func->variableWeight.at(abandon)
                || (func->variableWeight.at(ins) == func->variableWeight.at(abandon) && ins->id < abandon->id)) {
                abandon = ins;
            }
        }
        for (auto &it : *tempGraph.at(abandon)) {
            tempGraph.at(it)->erase(abandon);
        }
        tempGraph.erase(abandon);
        for (auto &it : *conflictGraph.at(abandon)) {
            conflictGraph.at(it)->erase(abandon);
        }
        conflictGraph.erase(abandon);
        func->variableWithoutReg.insert(abandon);
        goto ALLOC_REGISTER_START;
    }
    unordered_set<string> validRegs;
    for (int i = 0; i < _GLB_REG_CNT; ++i) validRegs.insert(to_string(i + _GLB_REG_START));
    while (!variableWithRegs.empty()) {
        unordered_set<string> regs = validRegs;
        shared_ptr<Value> ins = variableWithRegs.top();
        variableWithRegs.pop();
        for (auto &it : *conflictGraph.at(ins)) {
            if (func->variableRegs.count(it) != 0) regs.erase(func->variableRegs.at(it));
        }
        func->variableRegs[ins] = *regs.begin();
    }
}

unordered_set<shared_ptr<BasicBlock>>
getBlockPathPoints(shared_ptr<BasicBlock> &from, shared_ptr<BasicBlock> &to) {
    unordered_set<shared_ptr<BasicBlock>> fromReachable;
    unordered_set<shared_ptr<BasicBlock>> ans;
    unordered_set<shared_ptr<BasicBlock>> tempReachable;
    getBlockReachableBlocks(from, from, fromReachable);
    for (auto bb : fromReachable) {
        tempReachable.clear();
        getBlockReachableBlocks(bb, from, tempReachable);
        if (tempReachable.count(to) != 0) {
            ans.insert(bb);
        }
    }
    ans.erase(from);
    if (blockPath.count(from) != 0) {
        shared_ptr<unordered_map<shared_ptr<BasicBlock>,
                unordered_set<shared_ptr<BasicBlock>>>> tempMap = blockPath.at(from);
        if (tempMap->count(to) != 0) {
            cerr << "Error occurs in process register alloc: search path twice." << endl;
        } else {
            tempMap->insert({to, ans});
            blockPath[from] = tempMap;
        }
    } else {
        shared_ptr<unordered_map<shared_ptr<BasicBlock>, unordered_set<shared_ptr<BasicBlock>>>> tempMap
                = make_shared<unordered_map<shared_ptr<BasicBlock>, unordered_set<shared_ptr<BasicBlock>>>>();
        tempMap->insert({to, ans});
        blockPath[from] = tempMap;
    }
    return ans;
}

void getBlockReachableBlocks(shared_ptr<BasicBlock> &bb, shared_ptr<BasicBlock> &cannotArrive,
                             unordered_set<shared_ptr<BasicBlock>> &ans) {
    queue<shared_ptr<BasicBlock>> blockQueue;
    blockQueue.push(bb);
    while (!blockQueue.empty()) {
        shared_ptr<BasicBlock> top = blockQueue.front();
        blockQueue.pop();
        for (auto &suc : top->successors) {
            if (ans.count(suc) == 0 && suc != cannotArrive) {
                ans.insert(suc);
                blockQueue.push(suc);
            }
        }
    }
}

void addAliveValue(shared_ptr<Instruction> &value, shared_ptr<Instruction> &owner,
                   shared_ptr<BasicBlock> &ownerBlock) {
    if (value->resultType == L_VAL_RESULT) {
        if (owner->type == PHI_MOV) {
            s_p_c<PhiMoveInstruction>(owner)->blockALiveValues.at(ownerBlock).insert(value);
        } else owner->aliveValues.insert(value);
    }
}

void addAliveValue(shared_ptr<Value> &value, shared_ptr<Instruction> &owner,
                   shared_ptr<BasicBlock> &ownerBlock) {
    if (value->valueType == INSTRUCTION) {
        shared_ptr<Instruction> insVal = s_p_c<Instruction>(value);
        addAliveValue(insVal, owner, ownerBlock);
    } else if (value->valueType == PARAMETER) {
        if (owner->type == PHI_MOV) {
            s_p_c<PhiMoveInstruction>(owner)->blockALiveValues.at(ownerBlock).insert(value);
        } else owner->aliveValues.insert(value);
    } else {
        cerr << "Error occurs in process add alive value: add a non-instruction and non-parameter value." << endl;
    }
}

void outputConflictGraph(const string &funcName) {
    if (_debugIrOptimize) {
        const string fileName = debugMessageDirectory + "ir_conflict_graph.txt";
        ofstream irOptimizeStream(fileName, ios::app);
        irOptimizeStream << "Function <" << funcName << ">:" << endl;
        map<unsigned int, shared_ptr<unordered_set<shared_ptr<Value>>>> tempMap;
        for (auto &val : conflictGraph) {
            tempMap[val.first->id] = val.second;
        }
        for (auto &value : tempMap) {
            irOptimizeStream << "<" << value.first << ">:";
            set<unsigned int> tempValSet;
            for (auto &edge : *value.second) {
                tempValSet.insert(edge->id);
            }
            for (auto &edge : tempValSet) {
                irOptimizeStream << " <" << edge << ">";
            }
            irOptimizeStream << endl;
        }
        irOptimizeStream << endl;
        irOptimizeStream.close();
    }
}
