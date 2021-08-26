#include "ir_optimize.h"

void arrayExternalLift(shared_ptr<Module> &module) {
    for (auto &func : module->functions) {
        for (auto &bb : func->blocks) {
            for (auto &ins : bb->instructions) {
                if (ins->type == InstructionType::ALLOC) {
                    bool canExternalLift = true;
                    map<int, int> constValues;
                    unordered_set<shared_ptr<Value>> insUsers = ins->users;
                    for (auto &user : insUsers) {
                        if (dynamic_cast<StoreInstruction *>(user.get())) {
                            shared_ptr<StoreInstruction> store = s_p_c<StoreInstruction>(user);
                            if (store->value->valueType == NUMBER && store->offset->valueType == NUMBER) {
                                int number = s_p_c<NumberValue>(store->offset)->number;
                                if (constValues.count(number) != 0) {
                                    canExternalLift = false;
                                    break;
                                } else {
                                    constValues[number] = s_p_c<NumberValue>(store->value)->number;
                                }
                            } else {
                                canExternalLift = false;
                                break;
                            }
                        } else if (!dynamic_cast<LoadInstruction *>(user.get())) {
                            canExternalLift = false;
                            break;
                        }
                    }
                    if (canExternalLift) {
                        shared_ptr<AllocInstruction> alloc = s_p_c<AllocInstruction>(ins);
                        shared_ptr<Value> allocVal = alloc;
                        shared_ptr<ConstantValue> constant = make_shared<ConstantValue>();
                        shared_ptr<Value> constVal = constant;
                        constant->size = alloc->units;
                        constant->dimensions = vector({alloc->units});
                        constant->values = constValues;
                        constant->name = alloc->name;
                        module->globalConstants.push_back(constant);
                        unordered_set<shared_ptr<Value>> users = alloc->users;
                        for (auto &user : users) {
                            if (dynamic_cast<StoreInstruction *> (user.get())) {
                                user->abandonUse();
                            } else {
                                user->replaceUse(allocVal, constVal);
                            }
                        }
                        alloc->abandonUse();
                    }
                }
            }
        }
    }
}
