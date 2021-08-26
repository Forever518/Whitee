#include "ir_optimize.h"

void constantBranchConversion(shared_ptr<Module> &module) {
    for (auto &func : module->functions) {
        for (auto &bb : func->blocks) {
            for (auto &ins : bb->instructions) {
                if (ins->type == InstructionType::BR) {
                    shared_ptr<BranchInstruction> br = s_p_c<BranchInstruction>(ins);
                    if (br->condition->valueType == ValueType::NUMBER) {
                        shared_ptr<NumberValue> num = s_p_c<NumberValue>(br->condition);
                        if (num->number == 0) {
                            removeBlockPredecessor(br->trueBlock, bb);
                            ins = make_shared<JumpInstruction>(br->falseBlock, bb);
                            br->abandonUse();
                        } else {
                            removeBlockPredecessor(br->falseBlock, bb);
                            ins = make_shared<JumpInstruction>(br->trueBlock, bb);
                            br->abandonUse();
                        }
                    }
                }
            }
        }
    }
}
