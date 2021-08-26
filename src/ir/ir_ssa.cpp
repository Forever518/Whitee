#include "ir_ssa.h"

#include <iostream>

shared_ptr<Value> readLocalVariableRecursively(shared_ptr<BasicBlock> &bb, string &varName);

shared_ptr<Value> readLocalVariable(shared_ptr<BasicBlock> &bb, string &varName) {
    if (bb->localVarSsaMap.count(varName) != 0) return bb->localVarSsaMap.at(varName);
    return readLocalVariableRecursively(bb, varName);
}

void writeLocalVariable(shared_ptr<BasicBlock> &bb, const string &varName, const shared_ptr<Value> &value) {
    bb->localVarSsaMap[varName] = value;
    if (dynamic_cast<PhiInstruction *>(value.get())) {
        value->users.insert(bb);
    }
}

shared_ptr<Value> readLocalVariableRecursively(shared_ptr<BasicBlock> &bb, string &varName) {
    if (!bb->sealed) {
        shared_ptr<PhiInstruction> emptyPhi = make_shared<PhiInstruction>(varName, bb);
        bb->incompletePhis[varName] = emptyPhi;
        writeLocalVariable(bb, varName, emptyPhi);
        return emptyPhi;
    } else if (bb->predecessors.size() == 1) {
        shared_ptr<BasicBlock> predecessor = *(bb->predecessors.begin());
        shared_ptr<Value> val = readLocalVariable(predecessor, varName);
        writeLocalVariable(bb, varName, val);
        return val;
    } else {
        shared_ptr<Value> val = make_shared<PhiInstruction>(varName, bb);
        writeLocalVariable(bb, varName, val);
        shared_ptr<PhiInstruction> phi = s_p_c<PhiInstruction>(val);
        bb->phis.insert(phi);
        val = addPhiOperands(bb, varName, phi);
        writeLocalVariable(bb, varName, val);
        return val;
    }
}

shared_ptr<Value> addPhiOperands(shared_ptr<BasicBlock> &bb, string &varName, shared_ptr<PhiInstruction> &phi) {
    for (auto &it : bb->predecessors) {
        shared_ptr<BasicBlock> pred = it;
        shared_ptr<Value> v = readLocalVariable(pred, varName);
        shared_ptr<BasicBlock> block;
        phi->operands.insert({it, v});
        v->users.insert(phi);
    }
    return removeTrivialPhi(phi);
}

shared_ptr<Value> removeTrivialPhi(shared_ptr<PhiInstruction> &phi) {
    shared_ptr<Value> same;
    shared_ptr<Value> self = phi->shared_from_this();
    for (auto &it : phi->operands) {
        if (it.second == same || it.second == self) continue;
        if (same != nullptr) return phi;
        same = it.second;
    }
    if (same == nullptr) same = make_shared<UndefinedValue>(phi->localVarName);
    phi->users.erase(phi);
    unordered_set<shared_ptr<Value>> users = phi->users;
    shared_ptr<Value> toBeReplaced = phi;
    phi->block->phis.erase(phi);
    for (auto &it : users) {
        it->replaceUse(toBeReplaced, same);
    }
    if (_isBuildingIr) {
        for (auto &it : phi->operands) {
            it.second->users.erase(phi);
        }
        phi->valid = false;
    } else {
        phi->abandonUse();
    }
    for (auto &it : users) {
        if (dynamic_cast<PhiInstruction *>(it.get())) {
            shared_ptr<PhiInstruction> use = s_p_c<PhiInstruction>(it);
            shared_ptr<Value> newVal = removeTrivialPhi(use);
            if (use == same) {
                same = newVal;
            }
        }
    }
    return same;
}

void sealBasicBlock(shared_ptr<BasicBlock> &bb) {
    if (!bb->sealed) {
        for (auto &it : bb->incompletePhis) {
            string varName = it.first;
            shared_ptr<PhiInstruction> phi = it.second;
            bb->phis.insert(phi);
            addPhiOperands(bb, varName, phi);
        }
        bb->incompletePhis.clear();
        bb->sealed = true;
    } else {
        cerr << "Error occurs in process seal basic block: the block is sealed." << endl;
    }
}
