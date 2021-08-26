#include "ir_optimize.h"

bool globalVarHasWriteUser(const shared_ptr<Value> &globalVar) {
    for (auto &user : globalVar->users) {
        if (dynamic_cast<StoreInstruction *>(user.get())) return true;
        if (dynamic_cast<InvokeInstruction *>(user.get())) return true;
        if (dynamic_cast<BinaryInstruction *>(user.get()))
            if (globalVarHasWriteUser(user)) return true;
    }
    return false;
}

void globalVariableToConstant(shared_ptr<Value> &globalVar, shared_ptr<Module> &module) {
    for (auto it = module->globalVariables.begin(); it != module->globalVariables.end(); ++it) {
        if (*it == globalVar) {
            module->globalVariables.erase(it);
            break;
        }
    }
    shared_ptr<GlobalValue> global = s_p_c<GlobalValue>(globalVar);
    if (global->variableType == VariableType::INT) {
        shared_ptr<Value> constantNumber = getNumberValue(global->initValues.at(0));
        unordered_set<shared_ptr<Value>> users = global->users;
        for (auto user : users) {
            if (!dynamic_cast<LoadInstruction *>(user.get())) {
                cerr << "Error occurs in process global variable to constant:"
                        " global has other type users except load." << endl;
            } else {
                unordered_set<shared_ptr<Value>> userUsers = user->users;
                for (auto &loadUser : userUsers) {
                    loadUser->replaceUse(user, constantNumber);
                }
                user->abandonUse();
            }
        }
        global->abandonUse();
    } else {
        shared_ptr<Value> constantArray = make_shared<ConstantValue>(global);
        unordered_set<shared_ptr<Value>> users = global->users;
        for (auto &user : users) {
            user->replaceUse(globalVar, constantArray);
        }
        global->abandonUse();
        module->globalConstants.push_back(constantArray);
    }
}

void readOnlyVariableToConstant(shared_ptr<Module> &module) {
    vector<shared_ptr<Value>> globalVariables = module->globalVariables;
    for (auto &globalVar : globalVariables) {
        if (!globalVarHasWriteUser(globalVar)) {
            globalVariableToConstant(globalVar, module);
        }
    }
}
