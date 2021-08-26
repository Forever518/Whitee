#ifndef COMPILER_IR_SSA_H
#define COMPILER_IR_SSA_H

#include "ir.h"

extern shared_ptr<Value> readLocalVariable(shared_ptr<BasicBlock> &bb, string &varName);

extern void writeLocalVariable(shared_ptr<BasicBlock> &bb, const string &varName, const shared_ptr<Value> &value);

extern void sealBasicBlock(shared_ptr<BasicBlock> &bb);

extern shared_ptr<Value> addPhiOperands(shared_ptr<BasicBlock> &bb, string &varName, shared_ptr<PhiInstruction> &phi);

extern shared_ptr<Value> removeTrivialPhi(shared_ptr<PhiInstruction> &phi);

#endif
