#include "ir_optimize.h"

void deadCodeElimination(shared_ptr<Module> &module) {
    removeUnusedFunctions(module);
    countFunctionSideEffect(module);
    for (auto var = module->globalStrings.begin(); var != module->globalStrings.end();) {
        if ((*var)->users.empty()) {
            var = module->globalConstants.erase(var);
        } else {
            auto it = (*var)->users.begin();
            while (it != (*var)->users.end()) {
                if ((*it)->valueType == ValueType::INSTRUCTION
                    && (!s_p_c<Instruction>((*it))->block->valid
                        || !s_p_c<Instruction>((*it))->block->function->valid)) {
                    it = (*var)->users.erase(it);
                } else ++it;
            }
            ++var;
        }
    }
    for (auto var = module->globalVariables.begin(); var != module->globalVariables.end();) {
        if ((*var)->users.empty()) {
            var = module->globalVariables.erase(var);
        } else {
            auto it = (*var)->users.begin();
            while (it != (*var)->users.end()) {
                if ((*it)->valueType == ValueType::INSTRUCTION
                    && (!s_p_c<Instruction>((*it))->block->valid
                        || !s_p_c<Instruction>((*it))->block->function->valid)) {
                    it = (*var)->users.erase(it);
                } else ++it;
            }
            ++var;
        }
    }
    for (auto var = module->globalConstants.begin(); var != module->globalConstants.end();) {
        if ((*var)->users.empty()) {
            var = module->globalConstants.erase(var);
        } else {
            auto it = (*var)->users.begin();
            while (it != (*var)->users.end()) {
                if ((*it)->valueType == ValueType::INSTRUCTION
                    && (!s_p_c<Instruction>((*it))->block->valid
                        || !s_p_c<Instruction>((*it))->block->function->valid)) {
                    it = (*var)->users.erase(it);
                } else ++it;
            }
            ++var;
        }
    }
    vector<shared_ptr<Function>> functions = module->functions;
    for (auto &func : functions) {
        removeUnusedBasicBlocks(func);
        vector<shared_ptr<BasicBlock>> blocks = func->blocks;
        for (auto &bb : blocks) {
            removeUnusedInstructions(bb);
        }
    }
    fixRightValue(module);
}
