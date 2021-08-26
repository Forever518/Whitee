#include "ir_optimize.h"

void deadArrayDelete(shared_ptr<Module> &module) {
    for (auto glb = module->globalVariables.begin(); glb != module->globalVariables.end();) {
        unordered_set<shared_ptr<Value>> users = (*glb)->users;
        for (auto &user : users) {
            if (!dynamic_cast<StoreInstruction *>(user.get())) {
                goto GLOBAL_USER_STORE_JUDGE;
            }
        }
        for (auto &user : users) {
            user->abandonUse();
        }
        (*glb)->abandonUse();
        glb = module->globalVariables.erase(glb);
        continue;
        GLOBAL_USER_STORE_JUDGE:
        ++glb;
    }
    for (auto &func : module->functions) {
        for (auto &bb : func->blocks) {
            for (auto &ins : bb->instructions) {
                if (ins->type == InstructionType::ALLOC) {
                    bool canDelete = true;
                    for (auto &user : ins->users) {
                        if (!dynamic_cast<StoreInstruction *>(user.get())) {
                            canDelete = false;
                            break;
                        }
                    }
                    if (canDelete) {
                        ins->abandonUse();
                        unordered_set<shared_ptr<Value>> users = ins->users;
                        for (auto &user : users) {
                            user->abandonUse();
                        }
                    }
                }
            }
        }
    }
}
