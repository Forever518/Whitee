#ifndef COMPILER_IR_UTILS_H
#define COMPILER_IR_UTILS_H

#include "ir.h"
#include "ir_ssa.h"

// used at any time when optimizing ir.
extern const unordered_set<InstructionType> noResultTypes;

extern void removeUnusedInstructions(shared_ptr<BasicBlock> &bb);

extern void removeUnusedBasicBlocks(shared_ptr<Function> &func);

extern void removeUnusedFunctions(shared_ptr<Module> &module);

extern void removeBlockPredecessor(shared_ptr<BasicBlock> &bb, shared_ptr<BasicBlock> &pre);

extern void countFunctionSideEffect(shared_ptr<Module> &module);

extern void addUser(const shared_ptr<Value> &user, initializer_list<shared_ptr<Value>> used);

extern void addUser(const shared_ptr<Value> &user, const vector<shared_ptr<Value>> &used);

// used in ir built finished.
extern void removePhiUserBlocksAndMultiCmp(shared_ptr<Module> &module);

// used in ir or optimize finished.
extern void fixRightValue(shared_ptr<Module> &module);

extern void getFunctionRequiredStackSize(shared_ptr<Function> &func);

extern void phiElimination(shared_ptr<Function> &func);

extern void mergeAliveValuesToInstruction(shared_ptr<Function> &func);

#endif
