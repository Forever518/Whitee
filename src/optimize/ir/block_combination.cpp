#include "ir_optimize.h"

void blockCombination(shared_ptr<Module> &module) {
    for (auto &func : module->functions) {
        for (int i = 0; i < func->blocks.size(); ++i) {
            shared_ptr<BasicBlock> &bb = func->blocks.at(i);
            if (bb->instructions.empty()) continue;
            if (bb->successors.size() == 1) {
                shared_ptr<BasicBlock> successor = *bb->successors.begin();
                if (successor != bb && successor->predecessors.size() == 1) {
                    vector<shared_ptr<Instruction>> sIns = successor->instructions;
                    if (bb->instructions.at(bb->instructions.size() - 1)->type != InstructionType::JMP) {
                        cerr << "Error occurs in process block combination: the last instruction is not jump." << endl;
                    }
                    bb->instructions.erase(--bb->instructions.end());
                    bb->instructions.insert(bb->instructions.end(),
                                            successor->instructions.begin(),
                                            successor->instructions.end());
                    for (auto &ins : successor->instructions) {
                        ins->block = bb;
                    }
                    if (!successor->phis.empty()) {
                        cerr << "Error occurs in process block combination: phis is not empty." << endl;
                    }
                    bb->successors = successor->successors;
                    // TODO: MERGE LOCAL VAR SSA MAP?
                    unordered_set<shared_ptr<BasicBlock>> successors = bb->successors;
                    for (auto &it : successors) {
                        it->predecessors.insert(bb);
                        unordered_set<shared_ptr<PhiInstruction>> phis = it->phis;
                        for (auto &phi : phis) {
                            phi->replaceUse(successor, bb);
                        }
                    }
                    successor->abandonUse();
                    --i;
                }
            }
        }
    }
}
