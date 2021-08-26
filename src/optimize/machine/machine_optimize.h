#ifndef COMPILER_MACHINE_OPTIMIZE_H
#define COMPILER_MACHINE_OPTIMIZE_H

#include "../../machine_ir/machine_ir.h"
#include <cmath>

#define WORD_BIT 32


unsigned int countPowerOfTwo(unsigned int x);

unsigned int calDivMagicNumber(unsigned int x);

unsigned int calDivShiftNumber(unsigned int x);

vector<int> canConvertTwoPowMul(unsigned int x);

vector<shared_ptr<MachineIns>>
mulOptimization(shared_ptr<BinaryInstruction> &binaryIns, shared_ptr<MachineFunc> &machineFunc);

vector<shared_ptr<MachineIns>>
divOptimization(shared_ptr<BinaryInstruction> &binaryIns, shared_ptr<MachineFunc> &machineFunc);

void delete_imm_jump(shared_ptr<MachineModule> &machineModule);

void delete_useless_compute(shared_ptr<MachineModule> &machineModule);

void reduce_redundant_move(shared_ptr<MachineModule> &machineModule);

void merge_mla_and_mls(shared_ptr<MachineModule> &machineModule);

void exchange_branch_ins(shared_ptr<MachineModule> &machineModule);

#endif
