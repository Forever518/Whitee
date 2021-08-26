#ifndef COMPILER_MACHINE_IR_BUILD_H
#define COMPILER_MACHINE_IR_BUILD_H

#include "../ir/ir_build.h"
#include "machine_ir.h"

shared_ptr<MachineModule> buildMachineModule(shared_ptr<Module> &module);

shared_ptr<MachineBB>
bbToMachineBB(shared_ptr<BasicBlock> &bb, shared_ptr<MachineFunc> &machineFunction, shared_ptr<Module> &module);

#endif
