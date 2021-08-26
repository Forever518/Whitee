#include "ir_optimize.h"

void foldLocalArray(shared_ptr<AllocInstruction> &alloc) {
    bool visit = false;
    shared_ptr<BasicBlock> &bb = alloc->block;
    unordered_map<int, shared_ptr<Value>> arrValues;
    unordered_map<int, shared_ptr<StoreInstruction>> arrStores;
    unordered_set<int> canErase;
    for (auto ins = bb->instructions.begin(); ins != bb->instructions.end();) {
        if (!visit && *ins != alloc) {
            ++ins;
            continue;
        } else if (!visit && *ins == alloc) {
            visit = true;
            ++ins;
            continue;
        }
        if ((*ins)->type == InstructionType::INVOKE) {
            shared_ptr<InvokeInstruction> invoke = s_p_c<InvokeInstruction>(*ins);
            for (auto &arg : invoke->params) {
                if (arg == alloc) return;
            }
        } else if ((*ins)->type == InstructionType::BINARY) {
            shared_ptr<BinaryInstruction> bin = s_p_c<BinaryInstruction>(*ins);
            if (bin->lhs == alloc || bin->rhs == alloc) return;
        } else if ((*ins)->type == InstructionType::STORE) {
            shared_ptr<StoreInstruction> store = s_p_c<StoreInstruction>(*ins);
            if (store->address == alloc && store->offset->valueType == ValueType::NUMBER) {
                shared_ptr<NumberValue> off = s_p_c<NumberValue>(store->offset);
                if (arrStores.count(off->number) != 0 && canErase.count(off->number) != 0) {
                    arrStores.at(off->number)->abandonUse();
                } else {
                    canErase.insert(off->number);
                }
                arrValues[off->number] = store->value;
                arrStores[off->number] = store;
            } else if (store->address == alloc) {
                for (auto &item : arrStores) {
                    if (canErase.count(item.first) != 0)
                        item.second->abandonUse();
                }
                return;
            }
        } else if ((*ins)->type == InstructionType::LOAD) {
            shared_ptr<LoadInstruction> load = s_p_c<LoadInstruction>(*ins);
            if (load->address == alloc && load->offset->valueType == ValueType::NUMBER) {
                shared_ptr<NumberValue> off = s_p_c<NumberValue>(load->offset);
                if (arrValues.count(off->number) != 0) {
                    shared_ptr<Value> val = arrValues.at(off->number);
                    unordered_set<shared_ptr<Value>> users = load->users;
                    for (auto &u : users) {
                        shared_ptr<Value> toBeReplace = load;
                        u->replaceUse(toBeReplace, val);
                    }
                    if (val->valueType == INSTRUCTION && s_p_c<Instruction>(val)->resultType == R_VAL_RESULT) {
                        s_p_c<Instruction>(val)->resultType = L_VAL_RESULT;
                        s_p_c<Instruction>(val)->caughtVarName = generateTempLeftValueName();
                    }
                    if (val->valueType != ValueType::NUMBER)
                        canErase.erase(off->number);
                    load->abandonUse();
                    ins = bb->instructions.erase(ins);
                    continue;
                }
            } else if (load->address == alloc) {
                canErase.clear();
            }
        }
        ++ins;
    }
}

void localArrayFolding(shared_ptr<Module> &module) {
    for (auto &func : module->functions) {
        for (auto &bb : func->blocks) {
            vector<shared_ptr<Instruction>> instructions = bb->instructions;
            for (auto &ins : instructions) {
                if (ins->type == InstructionType::ALLOC) {
                    shared_ptr<AllocInstruction> alloc = s_p_c<AllocInstruction>(ins);
                    foldLocalArray(alloc);
                }
            }
        }
    }
}
