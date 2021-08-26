#include "ir_optimize.h"

void blockCommonSubexpressionElimination(shared_ptr<BasicBlock> &bb);

void localCommonSubexpressionElimination(shared_ptr<Module> &module) {
    for (auto &func : module->functions) {
        for (auto &bb : func->blocks) {
            blockCommonSubexpressionElimination(bb);
        }
    }
}

void blockCommonSubexpressionElimination(shared_ptr<BasicBlock> &bb) {
    unordered_map<unsigned long long, unordered_set<shared_ptr<Value>>> hashMap;
    for (auto it = bb->instructions.begin(); it != bb->instructions.end();) {
        shared_ptr<Instruction> ins = *it;
        if (ins->type == BINARY || ins->type == UNARY) {
            unsigned long long hashCode = ins->hashCode();
            if (hashMap.count(hashCode) != 0) {
                unordered_set<shared_ptr<Value>> tempSet = hashMap.at(hashCode);
                bool replace = false;
                for (auto i : tempSet) {
                    if (ins->equals(i)) {
                        replace = true;
                        if (i->valueType != INSTRUCTION) {
                            cerr << "Error occurs in process LCSE: non-instruction value in map." << endl;
                        }
                        shared_ptr<Instruction> insInMap = s_p_c<Instruction>(i);
                        if (insInMap->resultType == R_VAL_RESULT) {
                            insInMap->resultType = L_VAL_RESULT;
                            insInMap->caughtVarName = generateTempLeftValueName();
                        }
                        unordered_set<shared_ptr<Value>> users = ins->users;
                        shared_ptr<Value> toBeReplaced = ins;
                        for (auto &user : users) {
                            user->replaceUse(toBeReplaced, i);
                        }
                        ins->abandonUse();
                        it = bb->instructions.erase(it);
                        break;
                    }
                }
                if (!replace) ++it;
            } else {
                unordered_set<shared_ptr<Value>> newSet;
                newSet.insert(ins);
                hashMap[hashCode] = newSet;
                ++it;
            }
        } else ++it;
    }
}
