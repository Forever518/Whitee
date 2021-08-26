#ifndef COMPILER_IR_BUILD_H
#define COMPILER_IR_BUILD_H

#include "../basic/std/compile_std.h"
#include "../front/syntax/syntax_tree.h"
#include "../front/syntax/syntax_analyze.h"
#include "ir.h"
#include "ir_ssa.h"
#include "ir_utils.h"

extern shared_ptr<Module> buildIrModule(shared_ptr<CompUnitNode> &compUnit);

#endif
