#ifndef COMPILER_IR_CHECK_H
#define COMPILER_IR_CHECK_H

#include "ir.h"
#include "ir_utils.h"

/**
 * @return true if IR has no bugs.
 */
extern bool irCheck(const shared_ptr<Module> &);

extern bool globalIrCorrect;
extern bool irUserCheck;

extern bool globalWarningPermit;

#endif
