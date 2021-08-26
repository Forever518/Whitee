#ifndef COMPILER_IR_OPTIMIZE_H
#define COMPILER_IR_OPTIMIZE_H

#include "../../ir/ir.h"
#include "../../ir/ir_utils.h"
#include "../../ir/ir_ssa.h"
#include "../../ir/ir_check.h"

#include <iostream>
#include <fstream>

extern string debugMessageDirectory;

extern const unsigned int OPTIMIZE_TIMES;

extern unsigned long DEAD_BLOCK_CODE_GROUP_DELETE_TIMEOUT;
extern unsigned long CONFLICT_GRAPH_TIMEOUT;

extern void optimizeIr(shared_ptr<Module> &module, OptimizeLevel level);

void constantFolding(shared_ptr<Module> &module);

void deadCodeElimination(shared_ptr<Module> &module);

void functionInline(shared_ptr<Module> &module);

void constantBranchConversion(shared_ptr<Module> &module);

void blockCombination(shared_ptr<Module> &module);

void readOnlyVariableToConstant(shared_ptr<Module> &module);

void localArrayFolding(shared_ptr<Module> &module);

void deadArrayDelete(shared_ptr<Module> &module);

void arrayExternalLift(shared_ptr<Module> &module);

void deadBlockCodeGroupDelete(shared_ptr<Module> &module);

void loopInvariantCodeMotion(shared_ptr<Module> &module);

void localCommonSubexpressionElimination(shared_ptr<Module> &module);

// some end optimize functions.
void endOptimize(shared_ptr<Module> &module, OptimizeLevel level);

void calculateVariableWeight(shared_ptr<Function> &func);

void registerAlloc(shared_ptr<Function> &func);

#endif
