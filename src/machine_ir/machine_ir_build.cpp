#include <iostream>
#include <utility>
#include <set>
#include <stack>
#include <algorithm>

#include "machine_ir_build.h"
#include "../basic/std/compile_std.h"
#include "../optimize/machine/machine_optimize.h"

extern bool judgeImmValid(unsigned int imm, bool mov);

unordered_map<shared_ptr<BasicBlock>, shared_ptr<MachineBB>> IRB2MachB;

int const_pool_id = 0;
int ins_count = 0;
int pre_ins_count = 0;
set<int> invalid_imm;

Cond cmp_op = NON;
bool true_cmp = false;

unordered_map<mit::InsType, string> instype2string = { // NOLINT
        {mit::ADD,         "ADD"},
        {mit::SUB,         "SUB"},
        {mit::RSB,         "RSB"},
        {mit::MUL,         "MUL"},
        {mit::DIV,         "SDIV"},
        {mit::AND,         "AND"},
        {mit::ORR,         "ORR"},
        {mit::ASR,         "ASR"},
        {mit::LSR,         "LSR"},
        {mit::LSL,         "LSL"},
        {mit::SMULL,       "SMULL"},
        {mit::LOAD,        "LDR"},
        {mit::PSEUDO_LOAD, "LDR"},
        {mit::STORE,       "STR"},
        {mit::POP,         "POP"},
        {mit::PUSH,        "PUSH"},
        {mit::MOV,         "MOV"},
        {mit::MOVW,        "MOVW"},
        {mit::MOVT,        "MOVT"},
        {mit::CMP,         "CMP"},
        {mit::BRANCH,      "B"},
        {mit::BLINK,       "BL"},
        {mit::BRETURN,     "BX"},
        {mit::MLS,         "MLS"},
        {mit::MLA,         "MLA"},
        {mit::GLOBAL,      "GLOBAL"},
        {mit::COMMENT,     "COMMENT"}
};

unordered_map<SType, string> stype2string{ // NOLINT
        {NONE, ""},
        {ASR,  "ASR"},
        {LSR,  "LSR"},
        {LSL,  "LSL"}
};

unordered_map<Cond, string> cond2string{ // NOLINT
        {NON, ""},
        {EQ,  "EQ"},
        {NE,  "NE"},
        {LS,  "LT"},
        {LE,  "LE"},
        {GE,  "GE"},
        {GT,  "GT"}
};

unordered_map<State, string> state2string{ // NOLINT
        {VIRTUAL,      "^"},
        {IMM,          "#"},
        {REG,          "R"},
        {GLOB_INT,     "@"},
        {GLOB_POINTER, "@*"},
        {LABEL,        "."}
};

void loadImm2Reg(int num, shared_ptr<Operand> des, vector<shared_ptr<MachineIns>> &res, bool mov);

void loadVal2Reg(shared_ptr<Value> &val, shared_ptr<Operand> &des, shared_ptr<MachineFunc> &machineFunc,
                 vector<shared_ptr<MachineIns>> &res, bool mov, int compensate = 0, string reg = "3");

vector<shared_ptr<MachineIns>> genRetIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc);

vector<shared_ptr<MachineIns>> genJmpIns(shared_ptr<Instruction> &ins);

vector<shared_ptr<MachineIns>>
genInvokeIns2(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc, shared_ptr<Module> &module);

vector<shared_ptr<MachineIns>> genUnaryIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc);

vector<shared_ptr<MachineIns>> genBinaryIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc);

vector<shared_ptr<MachineIns>> genStoreIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc);

vector<shared_ptr<MachineIns>> genLoadIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc);

void genAlloc(shared_ptr<MachineFunc> &machineFunc, shared_ptr<Instruction> &ins);

vector<shared_ptr<MachineIns>> genBIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc);

vector<shared_ptr<MachineIns>> genCmpIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc);

vector<shared_ptr<MachineIns>>
genPhiMov(shared_ptr<Instruction> &ins, shared_ptr<BasicBlock> &basicBlock, shared_ptr<MachineFunc> &machineFunc);

vector<shared_ptr<MachineIns>> genPhi(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc);

vector<shared_ptr<MachineIns>> genGlobIns(shared_ptr<MachineModule> &machineModule);

set<string> tempRegPool; // NOLINT
unordered_map<shared_ptr<Value>, string> lValRegMap;
unordered_map<shared_ptr<Value>, string> rValRegMap;
unordered_set<string> regInUse;

// we need to record vars' addr in this step
// for local vars, we need to record the offset to the sp
// for global vars, we need to record the label

shared_ptr<MachineModule> buildMachineModule(shared_ptr<Module> &module) {
    shared_ptr<MachineModule> machineModule = make_shared<MachineModule>(); // NOLINT
    machineModule->globalConstants = module->globalConstants;
    machineModule->globalVariables = module->globalVariables;

    for (auto &func : module->functions) {
        // if (_debugMachineIr) cout << func->name + ":" << endl;
        shared_ptr<MachineFunc> machineFunction = make_shared<MachineFunc>();
        //machineModule->machineFunctions.push_back(machineFunction);
        machineFunction->name = func->name;
        machineFunction->funcType = func->funcType;
        machineFunction->params = func->params;
        machineFunction->stackSize = func->requiredStackSize + _W_LEN;
        machineFunction->stackPointer = 0;

        /// begin: clear all register pools.
        while (!tempRegPool.empty()) tempRegPool.clear();
        regInUse.clear();
        rValRegMap.clear();
        lValRegMap = func->variableRegs;
        for (int i = _TMP_REG_CNT - 2; i >= 0; --i) {
            tempRegPool.insert(to_string(_TMP_REG_START + i));
        }
        tempRegPool.insert("14");
        for (auto &it : lValRegMap) regInUse.insert(it.second);
        /// end: clear all register pools.

        shared_ptr<MachineBB> func_epilogue = make_shared<MachineBB>(func->blocks[0]->getValueId(), machineFunction);
        /// manage parameters
        int para_size = 0;
        for (int i = 4; i < machineFunction->params.size(); ++i) {
            if (lValRegMap.count(machineFunction->params[i]) != 0) {
                shared_ptr<Operand> des = make_shared<Operand>(REG, lValRegMap.at(machineFunction->params[i]));
                shared_ptr<Operand> stack = make_shared<Operand>(REG, "13");
                shared_ptr<Operand> offset = make_shared<Operand>(IMM, to_string((i - 4) * 4));
                shared_ptr<MemoryIns> load_para = make_shared<MemoryIns>(mit::LOAD, OFFSET, NON, NONE, 0, des, stack,
                                                                         offset);
                func_epilogue->MachineInstructions.push_back(load_para);
            } else {
                machineFunction->var2offset.insert(pair<string, int>(to_string(machineFunction->params[i]->id),
                                                                     (i - 4) * 4 + machineFunction->stackSize));
            }
        }
        for (int i = 0; i < machineFunction->params.size() && i < 4; ++i) {
            if (lValRegMap.count(machineFunction->params[i]) == 0) //no reg
            {
                shared_ptr<Operand> para_reg = make_shared<Operand>(REG, to_string(i));
                shared_ptr<Operand> stack = make_shared<Operand>(REG, "13");
                shared_ptr<Operand> offset = make_shared<Operand>(IMM, to_string(-16 + i * 4));
                shared_ptr<MemoryIns> storeParam = make_shared<MemoryIns>(mit::STORE, OFFSET, NON, NONE, 0, para_reg,
                                                                          stack, offset);
                func_epilogue->MachineInstructions.push_back(storeParam);
                machineFunction->var2offset.insert(pair<string, int>(to_string(machineFunction->params[i]->id),
                                                                     -16 + i * 4 + machineFunction->stackSize));
            } else {
                shared_ptr<Operand> para_reg = make_shared<Operand>(REG, to_string(i));
                shared_ptr<Operand> des_reg = make_shared<Operand>(REG, lValRegMap.at(machineFunction->params[i]));
                shared_ptr<MovIns> mov2Des = make_shared<MovIns>(NON, NONE, 0, des_reg, para_reg);
                func_epilogue->MachineInstructions.push_back(mov2Des);
            }
        }
        ///lr: temp reg
        shared_ptr<Operand> lr = make_shared<Operand>(REG, "14");
        shared_ptr<Operand> stack = make_shared<Operand>(REG, "13");
        shared_ptr<Operand> lrSpace = make_shared<Operand>(IMM, "-20");
        shared_ptr<MemoryIns> storeLR = make_shared<MemoryIns>(mit::STORE, OFFSET, NON, NONE, 0, lr, stack, lrSpace);
        func_epilogue->MachineInstructions.push_back(storeLR);
        ///move stack
        shared_ptr<Operand> stack_size;
        if (judgeImmValid(machineFunction->stackSize, false)) {
            stack_size = make_shared<Operand>(IMM, to_string(machineFunction->stackSize));
        } else {
            stack_size = make_shared<Operand>(REG, "0");
            loadImm2Reg(machineFunction->stackSize, stack_size, func_epilogue->MachineInstructions, true);
        }
        shared_ptr<BinaryIns> moveStack = make_shared<BinaryIns>(mit::SUB, NON, NONE, 0, stack, stack_size, stack);
        func_epilogue->MachineInstructions.push_back(moveStack);
        machineFunction->machineBlocks.push_back(func_epilogue);

        ///for each block in func, mapping it into machineFunc.
        for (auto &bb:func->blocks) {
            // if (_debugMachineIr) cout << "block" + to_string(bb->id) + ":" << endl;
            machineFunction->machineBlocks.push_back(bbToMachineBB(bb, machineFunction, module));
        }

        machineModule->machineFunctions.push_back(machineFunction);
    }

    for (auto &machineFunc:machineModule->machineFunctions) {
        for (auto &machineBB:machineFunc->machineBlocks) {
            shared_ptr<GlobalIns> block_label = make_shared<GlobalIns>("block" + to_string(machineBB->index), "");
            machineBB->MachineInstructions.insert(machineBB->MachineInstructions.begin(), block_label);
        }
    }

    for (auto &machineFunc:machineModule->machineFunctions) {
        for (auto &machineBB:machineFunc->machineBlocks) {
            for (auto it = machineBB->MachineInstructions.begin(); it != machineBB->MachineInstructions.end();) {
                auto ins = *it;
                if (ins->type == mit::COMMENT) {
                    ++it;
                    continue;
                }
                ins_count++;
                if (ins->type == mit::PSEUDO_LOAD) {
                    shared_ptr<PseudoLoad> ldr = s_p_c<PseudoLoad>(ins);
                    if (ldr->isGlob) {
                        ldr->label->value =
                                ldr->label->value + to_string(const_pool_id) + "_whitee_" + to_string(const_pool_id);
                    } else {
                        if (ldr->label->value.at(0) == '_') {
                            invalid_imm.insert(-stoi(ldr->label->value.substr(1)));
                        } else {
                            invalid_imm.insert(stoi(ldr->label->value));
                        }

                        ldr->label->value = "invalid_imm_" + to_string(const_pool_id) + "_" + ldr->label->value;
                    }
                }
                if (ins_count - pre_ins_count >= 800) {
                    vector<shared_ptr<MachineIns>> res;
                    res = genGlobIns(machineModule);
                    it = machineBB->MachineInstructions.insert(it + 1, res.begin(), res.end()) + res.size();
                    pre_ins_count = ins_count;
                    ins_count += res.size();
                } else {
                    ++it;
                }
            }
            if (ins_count - pre_ins_count >= 800) {
                vector<shared_ptr<MachineIns>> res;
                res = genGlobIns(machineModule);
                machineBB->MachineInstructions.insert(machineBB->MachineInstructions.end(), res.begin(), res.end());
                pre_ins_count = ins_count;
                ins_count += res.size();
            }
            if (!_optimizeMachineIr) {
                vector<shared_ptr<MachineIns>> res;
                res = genGlobIns(machineModule);
                machineBB->MachineInstructions.insert(machineBB->MachineInstructions.end(), res.begin(),
                                                      res.end());
                pre_ins_count = ins_count;
            }
        }
    }
    vector<shared_ptr<MachineIns>> res;
    res = genGlobIns(machineModule);
    shared_ptr<MachineBB> machineBB = machineModule->machineFunctions.back()->machineBlocks.back();
    machineBB->MachineInstructions.insert(machineBB->MachineInstructions.end(), res.begin(),
                                          res.end());
    pre_ins_count = ins_count;

    if (_optimizeMachineIr) {
        delete_imm_jump(machineModule);
        delete_useless_compute(machineModule);
        reduce_redundant_move(machineModule);
        merge_mla_and_mls(machineModule);
        exchange_branch_ins(machineModule);
    }

    return machineModule;
}

shared_ptr<MachineBB>
bbToMachineBB(shared_ptr<BasicBlock> &bb, shared_ptr<MachineFunc> &machineFunction, shared_ptr<Module> &module) {
    shared_ptr<MachineBB> machineBB = make_shared<MachineBB>(bb->id, machineFunction);
    IRB2MachB.insert(pair<shared_ptr<BasicBlock>, shared_ptr<MachineBB>>(bb, machineBB));
    for (auto &ins : bb->instructions) {
        /*
         * for each ins, we need to mapping it into machineIns.
         * for the result(ssa left value) in each type of ins except BR, JMP, RET, STORE,
         * we propose it as a local var, we need to record its offset and increase stackSize.
         */
        vector<shared_ptr<MachineIns>> res;
        string content = ins->toString();
        shared_ptr<Comment> ir = make_shared<Comment>(content);
        switch (ins->type) {
            case RET:
                res = genRetIns(ins, machineFunction);
                break;
            case BR:
                res = genBIns(ins, machineFunction);
                true_cmp = false;
                break;
            case JMP:
                res = genJmpIns(ins);
                break;
            case INVOKE:
                res = genInvokeIns2(ins, machineFunction, module);
                break;
            case UNARY:
                res = genUnaryIns(ins, machineFunction);
                break;
            case BINARY:
                res = genBinaryIns(ins, machineFunction);
                break;
            case CMP:
                true_cmp = true;
                res = genCmpIns(ins, machineFunction);
                break;
            case ALLOC:
                genAlloc(machineFunction, ins);
                break;
            case LOAD:
                res = genLoadIns(ins, machineFunction);
                break;
            case STORE:
                res = genStoreIns(ins, machineFunction);
                break;
            case PHI_MOV:
                res = genPhiMov(ins, bb, machineFunction);
                break;
            case PHI:
                res = genPhi(ins, machineFunction);
                break;
            default:
                break;
        }
        machineBB->MachineInstructions.insert(machineBB->MachineInstructions.end(), ir);
        machineBB->MachineInstructions.insert(machineBB->MachineInstructions.end(), res.begin(), res.end());
        if (tempRegPool.size() + rValRegMap.size() != _TMP_REG_CNT) {
            cerr << "machine_ir_build: temp reg error!" << endl;
        }
    }

    return machineBB;
}

string allocTempRegister() {
    if (!tempRegPool.empty()) {
        string reg = *tempRegPool.begin();
        tempRegPool.erase(reg);
        if (regInUse.count(reg) != 0) {
            cerr << "Alloc a register in use." << endl;
        }
        regInUse.insert(reg);
        return reg;
    } else {
        cerr << "Temp registers are not enough." << endl;
        return "error";
    }
}

void releaseTempRegister(const string &reg) {
    set<string> copy = tempRegPool;
    while (!copy.empty()) {
        if (*copy.begin() == reg) {
            cerr << "Try to release a register in pool." << endl;
            return;
        }
        copy.erase(copy.begin());
    }
    if (regInUse.count(reg) == 0) {
        cerr << "Register " + reg + " is not in the use list but trying to release it." << endl;
    }
    regInUse.erase(reg);
    tempRegPool.insert(reg);
}

void loadImm2Reg(int num, shared_ptr<Operand> des, vector<shared_ptr<MachineIns>> &res, bool mov) {
    shared_ptr<Operand> imm;
    if (judgeImmValid(num, mov)) { //can move to reg
        imm = make_shared<Operand>(IMM, to_string(num));
        shared_ptr<MovIns> mov2Reg = make_shared<MovIns>(NON, NONE, 0, des, imm);
        res.push_back(mov2Reg);
    } else { //load to reg
        if (_optimizeMachineIr) {
            unsigned short low16 = (unsigned int) num & 0x0000FFFFU;
            unsigned short high16 = ((unsigned int) num & 0xFFFF0000U) >> 16U;
            shared_ptr<Operand> low = make_shared<Operand>(IMM, to_string(low16));
            shared_ptr<Operand> high = make_shared<Operand>(IMM, to_string(high16));
            shared_ptr<MovIns> movw = make_shared<MovIns>(mit::MOVW, NON, NONE, 0, des, low);
            shared_ptr<MovIns> movt = make_shared<MovIns>(mit::MOVT, NON, NONE, 0, des, high);
            res.push_back(movw);
            if (high16 != 0) res.push_back(movt);
        } else {
            if (num < 0) {
                imm = make_shared<Operand>(LABEL, "_" + to_string(abs(num)));
            } else {
                imm = make_shared<Operand>(LABEL, to_string(num));
            }
            shared_ptr<PseudoLoad> load2Reg = make_shared<PseudoLoad>(NON, NONE, 0, imm, des, false);
            res.push_back(load2Reg);
        }
    }
}

void loadOffset(int offset, shared_ptr<Operand> &off, string reg, vector<shared_ptr<MachineIns>> &res) {
    if (offset < 4096 && offset > -4096) {
        off = make_shared<Operand>(IMM, to_string(offset));
        return;
    }
    off = make_shared<Operand>(REG, reg);
    loadImm2Reg(offset, off, res, true);
}

void loadGlobVar2Reg(shared_ptr<GlobalValue> &glob, shared_ptr<Operand> &des, vector<shared_ptr<MachineIns>> &res) {
    shared_ptr<Operand> op;
    if (glob->variableType == POINTER) {
        op = make_shared<Operand>(GLOB_POINTER, glob->name);
    } else {
        op = make_shared<Operand>(GLOB_INT, glob->name);
    }
    shared_ptr<PseudoLoad> loadAddr = make_shared<PseudoLoad>(NON, NONE, 0, op, des, true);
    res.push_back(loadAddr);
}

void loadConst2Reg(shared_ptr<ConstantValue> &cons, shared_ptr<Operand> &des, vector<shared_ptr<MachineIns>> &res) {
    shared_ptr<Operand> op = make_shared<Operand>(GLOB_POINTER, cons->name);
    shared_ptr<PseudoLoad> loadAddr = make_shared<PseudoLoad>(NON, NONE, 0, op, des, true);
    res.push_back(loadAddr);
}

void loadMemory2Reg(shared_ptr<Value> &var, shared_ptr<Operand> &des, shared_ptr<Operand> &offset,
                    vector<shared_ptr<MachineIns>> &res) {
    shared_ptr<Operand> stack = make_shared<Operand>(REG, "13");
    if (var->valueType == INSTRUCTION && s_p_c<Instruction>(var)->type == ALLOC) {
        shared_ptr<BinaryIns> addrToReg = make_shared<BinaryIns>(mit::ADD, NON, NONE, 0, stack, offset, des);
        res.push_back(addrToReg);
    } else {
        shared_ptr<MemoryIns> load_para = make_shared<MemoryIns>(mit::LOAD, OFFSET, NON, NONE, 0, des, stack, offset);
        res.push_back(load_para);
    }
}

void loadVal2Reg(shared_ptr<Value> &val, shared_ptr<Operand> &des, shared_ptr<MachineFunc> &machineFunc,
                 vector<shared_ptr<MachineIns>> &res, bool mov, int compensate, string reg) {
    if (val->valueType == NUMBER) {
        int imm = s_p_c<NumberValue>(val)->number;
        loadImm2Reg(imm, des, res, mov);
    } else if (val->valueType == GLOBAL) {
        shared_ptr<GlobalValue> glob_var = s_p_c<GlobalValue>(val);
        loadGlobVar2Reg(glob_var, des, res);
    } else if (val->valueType == CONSTANT) {
        shared_ptr<ConstantValue> const_var = s_p_c<ConstantValue>(val);
        loadConst2Reg(const_var, des, res);
    } else { //VIRTUAL
        shared_ptr<Operand> off;
        if (machineFunc->var2offset.count(to_string(val->id)) == 0) {
            if (rValRegMap.count(val) != 0 || lValRegMap.count(val) != 0) {
                string exist_reg;
                if (rValRegMap.count(val) != 0) exist_reg = rValRegMap.at(val);
                else exist_reg = lValRegMap.at(val);
                if (exist_reg == des->value) { //same reg
                    return;
                } else { //different reg
                    shared_ptr<Operand> ori = make_shared<Operand>(REG, exist_reg);
                    shared_ptr<MovIns> mov2Des = make_shared<MovIns>(NON, NONE, 0, des, ori);
                    res.push_back(mov2Des);
                    return;
                }
            }

            cerr << "machine_ir_build: unalloc memory" << endl;
            return;
        }
        int offset = machineFunc->var2offset.at(to_string(val->id)) + compensate;
        if (val->valueType == INSTRUCTION && s_p_c<Instruction>(val)->type == ALLOC) { //use ADD instead of LDR+OFFSET
            if (judgeImmValid(offset, false)) {
                off = make_shared<Operand>(IMM, to_string(offset));
            } else {
                off = make_shared<Operand>(REG, reg);
                loadImm2Reg(offset, off, res, true);
            }
        } else {
            loadOffset(offset, off, reg, res);
        }
        loadMemory2Reg(val, des, off, res);
    }
}

void storeNewValue(shared_ptr<Operand> &des, int val_id, shared_ptr<MachineFunc> &machineFunc,
                   vector<shared_ptr<MachineIns>> &res, string reg = "3") {
    shared_ptr<Operand> stack = make_shared<Operand>(REG, "13");
    shared_ptr<Operand> offset;
    loadOffset(machineFunc->stackPointer, offset, reg, res);
    shared_ptr<MemoryIns> strValue = make_shared<MemoryIns>(mit::STORE, OFFSET, NON, NONE, 0, des, stack, offset);
    res.push_back(strValue);
    machineFunc->var2offset.insert(
            pair<string, int>(to_string(val_id), machineFunc->stackPointer));
    machineFunc->stackPointer += 4;
}

void store2Memory(shared_ptr<Operand> &des, int val_id, shared_ptr<MachineFunc> &machineFunc,
                  vector<shared_ptr<MachineIns>> &res) {
    if (machineFunc->var2offset.count(to_string(val_id)) == 0) { // new value
        string reg = allocTempRegister();
        storeNewValue(des, val_id, machineFunc, res, reg);
        releaseTempRegister(reg);
        releaseTempRegister(des->value);
    } else { // existing value
        shared_ptr<Operand> stack = make_shared<Operand>(REG, "13");
        shared_ptr<Operand> offset;
        string reg = allocTempRegister();
        loadOffset(machineFunc->var2offset.at(to_string(val_id)), offset, reg, res);
        shared_ptr<MemoryIns> storeExistVal = make_shared<MemoryIns>(mit::STORE, OFFSET, NON, NONE, 0, des, stack,
                                                                     offset);
        res.push_back(storeExistVal);
        releaseTempRegister(reg);
        releaseTempRegister(des->value);
    }
}

void loadOperand(shared_ptr<Value> &val, shared_ptr<Operand> &des, shared_ptr<MachineFunc> &machineFunc,
                 vector<shared_ptr<MachineIns>> &res, bool mov, string reg = "3", bool regRequired = true) {
    if (regRequired) {
        loadVal2Reg(val, des, machineFunc, res, true, 0, reg);
    } else {
        if (val->valueType == NUMBER) {
            int imm = s_p_c<NumberValue>(val)->number;
            if (judgeImmValid(imm, mov)) {
                des->state = IMM;
                des->value = to_string(imm);
            } else {
                loadVal2Reg(val, des, machineFunc, res, true, 0, reg);
            }
        } else {
            loadVal2Reg(val, des, machineFunc, res, true, 0, reg);
        }
    }
}

/**
 * @param valueId
 * @param op is the Operand
 * @return true if the register need releasing.
 */
bool readRegister(shared_ptr<Value> &val, shared_ptr<Operand> &op, shared_ptr<MachineFunc> &machineFunc,
                  vector<shared_ptr<MachineIns>> &res, bool mov, bool regRequired) {
    if (val->valueType == INSTRUCTION && s_p_c<Instruction>(val)->resultType == L_VAL_RESULT) {
        if (lValRegMap.count(val) != 0) {
            op->value = lValRegMap.at(val);
            return false;
        } else {
            op->value = allocTempRegister();
            loadOperand(val, op, machineFunc, res, mov, op->value, regRequired);
            return true;
        }
    } else if (val->valueType == INSTRUCTION && s_p_c<Instruction>(val)->resultType == R_VAL_RESULT) {
        if (rValRegMap.count(val) == 0) {
            cerr << "Error occurs in process read register: read a r-value without register." << endl;
            return false;
        } else {
            op->value = rValRegMap.at(val);
            rValRegMap.erase(val);
            return true;
        }
    } else {
        if (val->valueType == NUMBER) {
            int num = s_p_c<NumberValue>(val)->number;
            if (regRequired) {
                op->value = allocTempRegister();
                op->state = REG;
                loadImm2Reg(num, op, res, mov);
                return true;
            } else {
                if (judgeImmValid(num, mov)) {
                    op->value = to_string(num);
                    op->state = IMM;
                    return false;
                } else {
                    op->value = allocTempRegister();
                    op->state = REG;
                    loadImm2Reg(num, op, res, mov);
                    return true;
                }
            }
        }
        if (val->valueType == GLOBAL) {
            string reg = allocTempRegister();
            op->value = reg;
            op->state = REG;
            shared_ptr<GlobalValue> glob_val = static_pointer_cast<GlobalValue>(val);
            loadGlobVar2Reg(glob_val, op, res);
            return true;
        }
        if (val->valueType == CONSTANT) {
            string reg = allocTempRegister();
            op->value = reg;
            op->state = REG;
            shared_ptr<ConstantValue> const_val = static_pointer_cast<ConstantValue>(val);
            loadConst2Reg(const_val, op, res);
            return true;
        }
        if (val->valueType == PARAMETER) {
            string reg = allocTempRegister();
            op->value = reg;
            op->state = REG;
            loadOperand(val, op, machineFunc, res, mov, reg, regRequired);
            return true;
        }
        if (val->valueType == INSTRUCTION && static_pointer_cast<Instruction>(val)->type == ALLOC) {
            string reg = allocTempRegister();
            op->value = reg;
            op->state = REG;
            loadOperand(val, op, machineFunc, res, mov, reg, regRequired);
            return true;
        }

        return false;
    }
}

/**
 * @param val
 * @param op
 * @return true if the register need releasing.
 */
bool writeRegister(shared_ptr<Value> &val, shared_ptr<Operand> &op, shared_ptr<MachineFunc> &machineFunc,
                   vector<shared_ptr<MachineIns>> &res) {
    if (val->valueType == INSTRUCTION && s_p_c<Instruction>(val)->resultType == L_VAL_RESULT) {
        if (lValRegMap.count(val) != 0) {
            op->value = lValRegMap.at(val);
            return false;
        } else {
            op->value = allocTempRegister();
            return true;
        }
    } else if (val->valueType == INSTRUCTION && s_p_c<Instruction>(val)->resultType == R_VAL_RESULT) {
        if (rValRegMap.count(val) != 0) {
            cerr << "Error occurs in process read register: write a r-value with register." << endl;
            return false;
        } else {
            op->value = allocTempRegister();
            rValRegMap[val] = op->value;
            return false;
        }
    } else {
        return false;
    }
}

vector<shared_ptr<MachineIns>> genRetIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc) {
    vector<shared_ptr<MachineIns>> res;
    shared_ptr<BXIns> bx = make_shared<BXIns>(NON, NONE, 0);   // bx lr

    if (s_p_c<ReturnInstruction>(ins)->funcType == FUNC_INT) {
        shared_ptr<Operand> op1 = make_shared<Operand>(REG, "0");
        shared_ptr<Value> ret_val = s_p_c<ReturnInstruction>(ins)->value;
        if (rValRegMap.count(ret_val) != 0 || lValRegMap.count(ret_val) != 0) {
            string ret_reg;
            if (rValRegMap.count(ret_val) != 0) {
                ret_reg = rValRegMap.at(ret_val);
                releaseTempRegister(ret_reg);
                rValRegMap.erase(ret_val);
            } else {
                ret_reg = lValRegMap.at(ret_val);
            }
            if (ret_reg != "0") {
                shared_ptr<Operand> ori_reg = make_shared<Operand>(REG, ret_reg);
                shared_ptr<MovIns> move2R0 = make_shared<MovIns>(NON, NONE, 0, op1, ori_reg);
                res.push_back(move2R0);
            }
        } else {
            loadVal2Reg(ret_val, op1, machineFunc, res, true);
        }
    }
    //end of function
    shared_ptr<Operand> stack = make_shared<Operand>(REG, "13");
    int para_size;
    int para_num = machineFunc->params.size();
    if ((para_num - 4) < 0) {
        para_size = 0;
    } else {
        para_size = para_num - 4;
    }
    int restore_size = machineFunc->stackSize + para_size * 4;
    shared_ptr<Operand> restore_stack;
    if (judgeImmValid(restore_size, false)) {
        restore_stack = make_shared<Operand>(IMM, to_string(restore_size));
    } else {
        restore_stack = make_shared<Operand>(REG, "1");
        loadImm2Reg(restore_size, restore_stack, res, true);
    }
    shared_ptr<BinaryIns> restore_func = make_shared<BinaryIns>(mit::ADD, NON, NONE, 0, stack, restore_stack, stack);
    res.push_back(restore_func);
    ///restore lr to pc
    shared_ptr<Operand> pc = make_shared<Operand>(REG, "15");
    shared_ptr<Operand> lrOff = make_shared<Operand>(IMM, to_string(-20 - para_size * 4));
    shared_ptr<MemoryIns> restoreLR = make_shared<MemoryIns>(mit::LOAD, OFFSET, NON, NONE, 0, pc, stack, lrOff);
    res.push_back(restoreLR);

    return res;
}

vector<shared_ptr<MachineIns>> genJmpIns(shared_ptr<Instruction> &ins) {
    vector<shared_ptr<MachineIns>> res;

    string label = "block" + std::to_string(s_p_c<JumpInstruction>(ins)->targetBlock->id);
    shared_ptr<BIns> b = make_shared<BIns>(NON, NONE, 0, label);

    res.push_back(b);
    return res;
}

vector<shared_ptr<MachineIns>>
genInvokeIns2(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc, shared_ptr<Module> &module) {
    vector<shared_ptr<MachineIns>> res;
    shared_ptr<InvokeInstruction> invoke = static_pointer_cast<InvokeInstruction>(ins);
    //save context
    bool useR0 = false;
    shared_ptr<StackIns> push = make_shared<StackIns>(NON, NONE, 0, true);
    set<int> reg_index;
    int context_size = 0;
    for (auto &r_val:rValRegMap) {
        if (r_val.second == "0") useR0 = true;
        reg_index.insert(stoi(r_val.second));
    }
    shared_ptr<Function> current_func;
    for (const auto &func:module->functions) {
        if (func->name == machineFunc->name) {
            current_func = func;
            break;
        }
    }
    set<int> current_reg_index;
    if (current_func != nullptr) {
        for (auto &alive_val:ins->aliveValues) {
            if (current_func->variableRegs.count(alive_val) != 0) {
                current_reg_index.insert(stoi(current_func->variableRegs.at(alive_val)));
            }
        }
    }
    reg_index.insert(current_reg_index.begin(), current_reg_index.end());
    for (auto index:reg_index) {
        context_size += 4;
        shared_ptr<Operand> reg = make_shared<Operand>(REG, to_string(index));
        push->regs.push_back(reg);
    }

    if (!push->regs.empty()) {
        res.push_back(push);
    }
    //save params
    //save rest params
    int i;
    int para_size = 0;
    int compensate = context_size;
    for (i = invoke->params.size() - 1; i >= 4;) {
        para_size += 4;
        int j = 0;
        shared_ptr<StackIns> push_param = make_shared<StackIns>(NON, NONE, 0, true);
        set<int> regs;
        while (i >= 4 && j < 4) {
            shared_ptr<Operand> tmp_param = make_shared<Operand>(REG, to_string(3 - j));
            loadVal2Reg(invoke->params[i], tmp_param, machineFunc, res, true, compensate, tmp_param->value);
            regs.insert(3 - j);
            j++;
            i--;
        }
        for (auto reg:regs) {
            shared_ptr<Operand> param = make_shared<Operand>(REG, to_string(reg));
            push_param->regs.push_back(param);
        }
        res.push_back(push_param);
        compensate += j * 4;
    }
    //save first 4 params
    while (i >= 0) {
        shared_ptr<Operand> init_param = make_shared<Operand>(REG, to_string(i));
        if (lValRegMap.count(invoke->params[i]) == 0 && rValRegMap.count(invoke->params[i]) == 0) {
            loadVal2Reg(invoke->params[i], init_param, machineFunc, res, true, compensate, to_string(i));
        } else {
            string init_reg;
            if (lValRegMap.count(invoke->params[i]) != 0) {
                init_reg = lValRegMap.at(invoke->params[i]);
            } else {
                init_reg = rValRegMap.at(invoke->params[i]);
                rValRegMap.erase(invoke->params[i]);
                releaseTempRegister(init_reg);
            }
            init_param->value = init_reg;
        }
        if (init_param->value != to_string(i)) {
            shared_ptr<Operand> des = make_shared<Operand>(REG, to_string(i));
            shared_ptr<MovIns> mov2R = make_shared<MovIns>(NON, NONE, 0, des, init_param);
            res.push_back(mov2R);
        }
        i--;
    }
    if (invoke->targetFunction == nullptr &&
        (invoke->targetName == "_sysy_starttime" || invoke->targetName == "_sysy_stoptime")) {
        shared_ptr<Operand> R0 = make_shared<Operand>(REG, "0");
        shared_ptr<Operand> one = make_shared<Operand>(IMM, "1");
        shared_ptr<MovIns> mov1 = make_shared<MovIns>(NON, NONE, 0, R0, one);
        res.push_back(mov1);
    }
    //bl
    string targetName;
    if (invoke->targetFunction != nullptr) {
        targetName = invoke->targetFunction->name;
    } else {
        targetName = invoke->targetName;
        if (targetName == "starttime" || targetName == "stoptime") {
            targetName = "_sysy_" + targetName;
        }
    }
    shared_ptr<BLIns> blink = make_shared<BLIns>(NON, NONE, 0, targetName);
    res.push_back(blink);
    //save return value in R0
    bool needFetch = false;
    bool needMove = false;
    if ((s_p_c<InvokeInstruction>(ins)->invokeType == GET_ARRAY ||
         s_p_c<InvokeInstruction>(ins)->invokeType == GET_CHAR ||
         s_p_c<InvokeInstruction>(ins)->invokeType == GET_INT ||
         (s_p_c<InvokeInstruction>(ins)->targetFunction != nullptr &&
          s_p_c<InvokeInstruction>(ins)->targetFunction->funcType == FUNC_INT))) {
        if (useR0) {
            needFetch = true;
            shared_ptr<Operand> ret = make_shared<Operand>(REG, "0");
            shared_ptr<Operand> stack = make_shared<Operand>(REG, "13");
            shared_ptr<Operand> offset = make_shared<Operand>(IMM, "-4");
            shared_ptr<MemoryIns> storeR0 = make_shared<MemoryIns>(mit::STORE, OFFSET, NON, NONE, 0, ret, stack,
                                                                   offset);
            res.push_back(storeR0);
        } else {
            needMove = true;
        }
    }
    //restore context
    shared_ptr<StackIns> pop = make_shared<StackIns>(NON, NONE, 0, false);
    for (int j = 0; j < push->regs.size(); ++j) {
        pop->regs.push_back(push->regs[j]);
    }
    if (!pop->regs.empty()) {
        res.push_back(pop);
    }
    //fetch return value R0
    if (needMove) {
        shared_ptr<Operand> ret = make_shared<Operand>(REG, "0");
        shared_ptr<Operand> final_des = make_shared<Operand>(REG, "1");
        shared_ptr<Value> i_ins = ins;
        bool release_des = writeRegister(i_ins, final_des, machineFunc, res);
        shared_ptr<MovIns> move2Des = make_shared<MovIns>(NON, NONE, 0, final_des, ret);
        res.push_back(move2Des);
        if (release_des) {
            store2Memory(final_des, ins->id, machineFunc, res);
        }
    }
    if (needFetch) {
        shared_ptr<Operand> final_des = make_shared<Operand>(REG, "1");
        shared_ptr<Operand> stack = make_shared<Operand>(REG, "13");
        shared_ptr<Value> i_ins = ins;
        bool release_des = writeRegister(i_ins, final_des, machineFunc, res);
        shared_ptr<Operand> offset = make_shared<Operand>(IMM, to_string(-context_size - 4));
        shared_ptr<MemoryIns> fetchR0 = make_shared<MemoryIns>(mit::LOAD, OFFSET, NON, NONE, 0, final_des, stack,
                                                               offset);
        res.push_back(fetchR0);
        if (release_des) {
            store2Memory(final_des, ins->id, machineFunc, res);
        }
    }
    return res;
}

vector<shared_ptr<MachineIns>> genUnaryIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc) {
    shared_ptr<UnaryInstruction> ui = s_p_c<UnaryInstruction>(ins);
    vector<shared_ptr<MachineIns>> res;
    if (ui->op == "-") {
        shared_ptr<Operand> op1 = make_shared<Operand>(IMM, "0");
        shared_ptr<Operand> op2 = make_shared<Operand>(REG, "3");
        bool release2 = readRegister(ui->value, op2, machineFunc, res, true, true);
        if (release2) {
            releaseTempRegister(op2->value);
        }
        shared_ptr<Operand> rd = make_shared<Operand>(REG, "1");
        shared_ptr<Value> u_ins = ins;
        bool release_rd = writeRegister(u_ins, rd, machineFunc, res);

        shared_ptr<BinaryIns> bi = make_shared<BinaryIns>(mit::RSB, NON, NONE, 0, op2, op1, rd);
        res.push_back(bi);
        if (release_rd) {
            store2Memory(rd, ui->id, machineFunc, res);
        }
    } else {
        shared_ptr<Operand> op2 = make_shared<Operand>(IMM, "0");
        shared_ptr<Operand> op1 = make_shared<Operand>(REG, "2");
        bool release1 = readRegister(ui->value, op1, machineFunc, res, true, true);
        if (release1) {
            releaseTempRegister(op1->value);
        }
        shared_ptr<CmpIns> cmp = make_shared<CmpIns>(NON, NONE, 0, op1, op2);
        res.push_back(cmp);
        shared_ptr<Operand> rd = make_shared<Operand>(REG, "1");
        shared_ptr<Value> u_ins = ins;
        bool release_rd = writeRegister(u_ins, rd, machineFunc, res);
        shared_ptr<Operand> ans0 = make_shared<Operand>(IMM, "0");
        shared_ptr<Operand> ans1 = make_shared<Operand>(IMM, "1");
        shared_ptr<MovIns> mv1 = make_shared<MovIns>(EQ, NONE, 0, rd, ans1);
        shared_ptr<MovIns> mv2 = make_shared<MovIns>(NE, NONE, 0, rd, ans0);
        res.push_back(mv1);
        res.push_back(mv2);
        if (release_rd) {
            store2Memory(rd, ui->id, machineFunc, res);
        }
    }
    return res;
}

vector<shared_ptr<MachineIns>> genBinaryIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc) {
    vector<shared_ptr<MachineIns>> res;
    shared_ptr<Operand> rd;
    if (s_p_c<BinaryInstruction>(ins)->op == "%") {
        shared_ptr<Operand> d_op1 = make_shared<Operand>(REG, "2");
        shared_ptr<Value> lhs = s_p_c<BinaryInstruction>(ins)->lhs;
        bool release1 = readRegister(lhs, d_op1, machineFunc, res, true, true);
        shared_ptr<Operand> d_op2 = make_shared<Operand>(REG, "3");
        shared_ptr<Value> rhs = s_p_c<BinaryInstruction>(ins)->rhs;
        bool release2 = readRegister(rhs, d_op2, machineFunc, res, true, true);

        shared_ptr<Operand> d_rd = make_shared<Operand>(REG, "1");

        d_rd->value = allocTempRegister();
        shared_ptr<BinaryIns> sdiv = make_shared<BinaryIns>(mit::DIV, NON, NONE, 0, d_op1, d_op2, d_rd);
        res.push_back(sdiv);
        releaseTempRegister(d_rd->value);
        if (release2) releaseTempRegister(d_op2->value);
        if (release1) releaseTempRegister(d_op1->value);
        rd = make_shared<Operand>(REG, "0");
        shared_ptr<Value> ans = ins;
        bool release_ans = writeRegister(ans, rd, machineFunc, res);
        shared_ptr<TriIns> mls = make_shared<TriIns>(mit::MLS, NON, NONE, 0, d_rd, d_op2, d_op1, rd);
        res.push_back(mls);
        if (release_ans) {
            store2Memory(rd, ins->id, machineFunc, res);
        }
    } else { //+ - * / && ||
        if (s_p_c<BinaryInstruction>(ins)->op == ">" || s_p_c<BinaryInstruction>(ins)->op == "<" ||
            s_p_c<BinaryInstruction>(ins)->op == "<=" || s_p_c<BinaryInstruction>(ins)->op == ">=" ||
            s_p_c<BinaryInstruction>(ins)->op == "==" || s_p_c<BinaryInstruction>(ins)->op == "!=") {
            return genCmpIns(ins, machineFunc);
        }
        if (_optimizeDivAndMul && s_p_c<BinaryInstruction>(ins)->rhs->valueType == NUMBER) {
            shared_ptr<BinaryInstruction> binaryIns = s_p_c<BinaryInstruction>(ins);
            if (s_p_c<BinaryInstruction>(ins)->op == "*") {
                return mulOptimization(binaryIns, machineFunc);
            } else if (s_p_c<BinaryInstruction>(ins)->op == "/") {
                return divOptimization(binaryIns, machineFunc);
            }
        }
        shared_ptr<Operand> op1 = make_shared<Operand>(REG, "2");
        shared_ptr<Value> lhs = s_p_c<BinaryInstruction>(ins)->lhs;
        bool release1 = readRegister(lhs, op1, machineFunc, res, true, true);
        shared_ptr<Operand> op2 = make_shared<Operand>(REG, "3");
        shared_ptr<Value> rhs = s_p_c<BinaryInstruction>(ins)->rhs;
        bool release2;
        if (s_p_c<BinaryInstruction>(ins)->op == "*" ||
            s_p_c<BinaryInstruction>(ins)->op == "/") {
            release2 = readRegister(rhs, op2, machineFunc, res, true, true);
        } else {
            release2 = readRegister(rhs, op2, machineFunc, res, false, false);
        }
        if (release2) releaseTempRegister(op2->value);
        if (release1) releaseTempRegister(op1->value);
        rd = make_shared<Operand>(REG, "1");
        shared_ptr<Value> b_ins = ins;
        bool release_rd = writeRegister(b_ins, rd, machineFunc, res);
        mit::InsType type = s_p_c<BinaryInstruction>(ins)->op == "+" ? mit::ADD :
                            s_p_c<BinaryInstruction>(ins)->op == "-" ? mit::SUB :
                            s_p_c<BinaryInstruction>(ins)->op == "*" ? mit::MUL :
                            s_p_c<BinaryInstruction>(ins)->op == "/" ? mit::DIV :
                            s_p_c<BinaryInstruction>(ins)->op == "&&" ? mit::AND : mit::ORR;
        shared_ptr<BinaryIns> binary = make_shared<BinaryIns>(type, NON, NONE, 0, op1, op2, rd);
        res.push_back(binary);
        if (release_rd) {
            store2Memory(rd, ins->id, machineFunc, res);
        }
    }

    return res;
}

vector<shared_ptr<MachineIns>> genCmpIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc) {
    vector<shared_ptr<MachineIns>> res;
    shared_ptr<BinaryInstruction> bi = s_p_c<BinaryInstruction>(ins);
    shared_ptr<Operand> op1 = make_shared<Operand>(REG, "2");
    shared_ptr<Value> lhs = bi->lhs;
    bool release1 = readRegister(lhs, op1, machineFunc, res, true, true);
    shared_ptr<Operand> op2 = make_shared<Operand>(REG, "3");
    shared_ptr<Value> rhs = bi->rhs;
    bool release2 = readRegister(rhs, op2, machineFunc, res, false, false);
    shared_ptr<CmpIns> cmp = make_shared<CmpIns>(NON, NONE, 0, op1, op2);
    res.push_back(cmp);
    if (release2) releaseTempRegister(op2->value);
    if (release1) releaseTempRegister(op1->value);
    shared_ptr<Operand> ans = make_shared<Operand>(REG, "1");
    shared_ptr<Value> cmp_ins = ins;
    bool release_rd = writeRegister(cmp_ins, ans, machineFunc, res);
    shared_ptr<Operand> one = make_shared<Operand>(IMM, "1");
    shared_ptr<Operand> zero = make_shared<Operand>(IMM, "0");
    shared_ptr<MovIns> assign_t = make_shared<MovIns>(NON, NONE, 0, ans, one);
    shared_ptr<MovIns> assign_f = make_shared<MovIns>(NON, NONE, 0, ans, zero);
    if (bi->op == "==") {
        assign_t->cond = EQ;
        assign_f->cond = NE;
        cmp_op = NE;
    } else if (bi->op == "!=") {
        assign_t->cond = NE;
        assign_f->cond = EQ;
        cmp_op = EQ;
    } else if (bi->op == "<") {
        assign_t->cond = LS;
        assign_f->cond = GE;
        cmp_op = GE;
    } else if (bi->op == ">") {
        assign_t->cond = GT;
        assign_f->cond = LE;
        cmp_op = LE;
    } else if (bi->op == "<=") {
        assign_t->cond = LE;
        assign_f->cond = GT;
        cmp_op = GT;
    } else {
        assign_t->cond = GE;
        assign_f->cond = LS;
        cmp_op = LS;
    }
    if (!true_cmp) {
        res.push_back(assign_t);
        res.push_back(assign_f);
        if (release_rd) {
            store2Memory(ans, bi->id, machineFunc, res);
        }
    } else {
        if (release_rd) releaseTempRegister(ans->value);
    }

    return res;
}

vector<shared_ptr<MachineIns>> genBIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc) {
    vector<shared_ptr<MachineIns>> res;
    shared_ptr<BranchInstruction> br = s_p_c<BranchInstruction>(ins);
    shared_ptr<Operand> op1 = make_shared<Operand>(REG, "2");
    bool release1 = readRegister(br->condition, op1, machineFunc, res, true, true);
    shared_ptr<Operand> op2 = make_shared<Operand>(IMM, "0");
    shared_ptr<CmpIns> cmpIns = make_shared<CmpIns>(NON, NONE, 0, op1, op2);
    if (release1) releaseTempRegister(op1->value);
    //false case
    string false_label = "block" + to_string(br->falseBlock->id);
    shared_ptr<BIns> bfIns = make_shared<BIns>(EQ, NONE, 0, false_label);
    //true case
    string true_label = "block" + to_string(br->trueBlock->id);
    shared_ptr<BIns> btIns = make_shared<BIns>(NON, NONE, 0, true_label);
    if (cmp_op != NON) {
        bfIns->cond = cmp_op;
    }
    if (!true_cmp) {
        res.push_back(cmpIns);
    }
    res.push_back(bfIns);
    res.push_back(btIns);
    cmp_op = NON;
    return res;
}

void genAlloc(shared_ptr<MachineFunc> &machineFunc, shared_ptr<Instruction> &ins) {
    shared_ptr<AllocInstruction> al = s_p_c<AllocInstruction>(ins);
    machineFunc->var2offset.insert(pair<string, int>(to_string(al->id), machineFunc->stackPointer));
    machineFunc->stackPointer += al->bytes;
}

vector<shared_ptr<MachineIns>> genLoadIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc) {
    shared_ptr<LoadInstruction> li = s_p_c<LoadInstruction>(ins);
    vector<shared_ptr<MachineIns>> res;
    if (li->address->valueType == PARAMETER || li->address->valueType == GLOBAL ||
        li->address->valueType == CONSTANT || (li->address->valueType == INSTRUCTION &&
                                               s_p_c<Instruction>(li->address)->type ==
                                               BINARY)) {
        shared_ptr<Operand> t_base = make_shared<Operand>(REG, "1");
        bool release_base = readRegister(li->address, t_base, machineFunc, res, true, true);
        shared_ptr<Operand> t_offset = make_shared<Operand>(REG, "3");
        shared_ptr<Shift> t_s = make_shared<Shift>();
        bool release_offset;
        if (li->offset->valueType == NUMBER) {
            int off = s_p_c<NumberValue>(li->offset)->number * 4;
            string reg = allocTempRegister();
            t_offset->value = reg;
            loadOffset(off, t_offset, reg, res);
            release_offset = true;
            if (t_offset->state == IMM) {
                releaseTempRegister(reg);
                release_offset = false;
            }
            t_s->type = NONE;
            t_s->shift = 0;
        } else {
            release_offset = readRegister(li->offset, t_offset, machineFunc, res, true, true);
            t_s->type = LSL;
            t_s->shift = 2;
        }
        if (release_offset) releaseTempRegister(t_offset->value);
        if (release_base) releaseTempRegister(t_base->value);
        shared_ptr<Operand> t_rd = make_shared<Operand>(REG, "2");
        shared_ptr<Value> l_ins = ins;
        bool release_rd = writeRegister(l_ins, t_rd, machineFunc, res);
        shared_ptr<MemoryIns> t_load = make_shared<MemoryIns>(mit::LOAD, OFFSET, NON, t_s, t_rd, t_base, t_offset);
        res.push_back(t_load);
        if (release_rd) {
            store2Memory(t_rd, li->id, machineFunc, res);
        }
    } else { //use sp
        //compute offset
        shared_ptr<Operand> f_pre = make_shared<Operand>(REG, "2");
        f_pre->value = allocTempRegister();
        f_pre->state = REG;
        loadImm2Reg(machineFunc->var2offset.at(to_string(li->address->id)), f_pre, res, true);
        shared_ptr<Operand> t_off = make_shared<Operand>(REG, "3");
        bool release_off;
        shared_ptr<Shift> t_s = make_shared<Shift>();
        if (li->offset->valueType == NUMBER) {
            int off = s_p_c<NumberValue>(li->offset)->number * 4;
            if (judgeImmValid(off, false)) {
                t_off->state = IMM;
                t_off->value = to_string(off);
                release_off = false;
            } else {
                t_off->value = allocTempRegister();
                t_off->state = REG;
                loadImm2Reg(off, t_off, res, true);
                release_off = true;
            }
            t_s->type = NONE;
            t_s->shift = 0;
        } else {
            release_off = readRegister(li->offset, t_off, machineFunc, res, true, true);
            t_s->type = LSL;
            t_s->shift = 2;
        }
        if (release_off) releaseTempRegister(t_off->value);
        releaseTempRegister(f_pre->value);
        shared_ptr<Operand> f_aft = make_shared<Operand>(REG, "3");
        f_aft->value = allocTempRegister();
        f_aft->state = REG;
        shared_ptr<BinaryIns> add = make_shared<BinaryIns>(mit::ADD, NON, t_s, f_pre, t_off, f_aft);
        res.push_back(add);
        releaseTempRegister(f_aft->value);
        //load
        shared_ptr<Operand> des = make_shared<Operand>(REG, "2");
        shared_ptr<Value> l_ins = ins;
        bool release_des = writeRegister(l_ins, des, machineFunc, res);
        shared_ptr<Operand> base = make_shared<Operand>(REG, "13");
        shared_ptr<MemoryIns> load = make_shared<MemoryIns>(mit::LOAD, OFFSET, NON, NONE, 0, des, base, f_aft);
        res.push_back(load);
        if (release_des) {
            store2Memory(des, li->id, machineFunc, res);
        }
    }

    return res;
}

vector<shared_ptr<MachineIns>> genStoreIns(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc) {
    shared_ptr<StoreInstruction> si = s_p_c<StoreInstruction>(ins);
    vector<shared_ptr<MachineIns>> res;
    if (si->address->valueType == PARAMETER || si->address->valueType == GLOBAL ||
        (si->address->valueType == INSTRUCTION &&
         s_p_c<Instruction>(si->address)->type == BINARY)) { //pointer
        shared_ptr<Operand> t_base = make_shared<Operand>(REG, "1");
        bool release_base = readRegister(si->address, t_base, machineFunc, res, true, true);
        shared_ptr<Operand> t_rd = make_shared<Operand>(REG, "2");
        bool release_rd = readRegister(si->value, t_rd, machineFunc, res, true, true);
        shared_ptr<Operand> t_offset = make_shared<Operand>(REG, "3");
        bool release_offset;
        shared_ptr<Shift> t_s = make_shared<Shift>();
        if (si->offset->valueType == NUMBER) {
            int off = s_p_c<NumberValue>(si->offset)->number * 4;
            string reg = allocTempRegister();
            t_offset->value = reg;
            loadOffset(off, t_offset, t_offset->value, res);
            release_offset = true;
            if (t_offset->state == IMM) {
                release_offset = false;
                releaseTempRegister(reg);
            }
            t_s->shift = 0;
            t_s->type = NONE;
        } else {
            release_offset = readRegister(si->offset, t_offset, machineFunc, res, true, true);
            t_s->shift = 2;
            t_s->type = LSL;
        }
        shared_ptr<MemoryIns> t_store = make_shared<MemoryIns>(mit::STORE, OFFSET, NON, t_s, t_rd, t_base, t_offset);
        res.push_back(t_store);
        if (release_offset) releaseTempRegister(t_offset->value);
        if (release_rd) releaseTempRegister(t_rd->value);
        if (release_base) releaseTempRegister(t_base->value);
    } else { //use sp
        //compute offset
        shared_ptr<Operand> f_pre = make_shared<Operand>(REG, "2");
        f_pre->value = allocTempRegister();
        f_pre->state = REG;
        loadImm2Reg(machineFunc->var2offset.at(to_string(si->address->id)), f_pre, res, true);
        shared_ptr<Operand> t_off = make_shared<Operand>(REG, "3");
        bool release_offset;
        shared_ptr<Shift> t_s = make_shared<Shift>();
        if (si->offset->valueType == NUMBER) {
            int off = s_p_c<NumberValue>(si->offset)->number * 4;
            if (judgeImmValid(off, false)) {
                t_off->value = to_string(off);
                t_off->state = IMM;
                release_offset = false;
            } else {
                t_off->value = allocTempRegister();
                t_off->state = REG;
                loadImm2Reg(off, t_off, res, true);
                release_offset = true;
            }
            t_s->type = NONE;
            t_s->shift = 0;
        } else {
            release_offset = readRegister(si->offset, t_off, machineFunc, res, true, true);
            t_s->type = LSL;
            t_s->shift = 2;
        }
        releaseTempRegister(f_pre->value);
        if (release_offset) releaseTempRegister(t_off->value);
        shared_ptr<Operand> f_aft = make_shared<Operand>(REG, "3");
        f_aft->value = allocTempRegister();
        f_aft->state = REG;
        shared_ptr<BinaryIns> add = make_shared<BinaryIns>(mit::ADD, NON, t_s, f_pre, t_off, f_aft);
        res.push_back(add);
        //store
        shared_ptr<Operand> obj = make_shared<Operand>(REG, "2");
        bool release_obj = readRegister(si->value, obj, machineFunc, res, true, true);
        shared_ptr<Operand> base = make_shared<Operand>(REG, "13");
        shared_ptr<MemoryIns> store = make_shared<MemoryIns>(mit::STORE, OFFSET, NON, NONE, 0, obj, base, f_aft);
        res.push_back(store);
        if (release_obj) releaseTempRegister(obj->value);
        releaseTempRegister(f_aft->value);
    }
    return res;
}

vector<shared_ptr<MachineIns>> genPhiMov(shared_ptr<Instruction> &ins, shared_ptr<BasicBlock> &basicBlock,
                                         shared_ptr<MachineFunc> &machineFunc) //PhiMoveIns
{
    vector<shared_ptr<MachineIns>> res;
    shared_ptr<PhiMoveInstruction> p_move = static_pointer_cast<PhiMoveInstruction>(ins);
    shared_ptr<Value> target = p_move->phi->operands.at(basicBlock);
    shared_ptr<Operand> op = make_shared<Operand>(REG, "3");
    bool release_target = readRegister(target, op, machineFunc, res, true, true);
    shared_ptr<Operand> des = make_shared<Operand>(REG, "2");
    shared_ptr<Value> p_ins = ins;
    if (release_target) releaseTempRegister(op->value);
    bool release_des = writeRegister(p_ins, des, machineFunc, res);
    shared_ptr<MovIns> move2Des = make_shared<MovIns>(NON, NONE, 0, des, op);
    res.push_back(move2Des);
    if (release_des) {
        store2Memory(des, ins->id, machineFunc, res);
    }
    return res;
}

vector<shared_ptr<MachineIns>> genPhi(shared_ptr<Instruction> &ins, shared_ptr<MachineFunc> &machineFunc) {
    vector<shared_ptr<MachineIns>> res;
    shared_ptr<PhiInstruction> phi = static_pointer_cast<PhiInstruction>(ins);
    shared_ptr<Operand> phi_mov = make_shared<Operand>(REG, "3");
    shared_ptr<Value> p_mov = phi->phiMove;
    bool release_mov = readRegister(p_mov, phi_mov, machineFunc, res, true, true);
    shared_ptr<Operand> target = make_shared<Operand>(REG, "2");
    shared_ptr<Value> p_ins = ins;
    bool release_target = writeRegister(p_ins, target, machineFunc, res);
    shared_ptr<MovIns> move2Target = make_shared<MovIns>(NON, NONE, 0, target, phi_mov);
    res.push_back(move2Target);
    if (release_target) {
        store2Memory(target, ins->id, machineFunc, res);
    }
    if (release_mov) releaseTempRegister(phi_mov->value);
    return res;
}

vector<shared_ptr<MachineIns>> genGlobIns(shared_ptr<MachineModule> &machineModule) {
    vector<shared_ptr<MachineIns>> res;
    string pool_label = "next" + to_string(const_pool_id);
    shared_ptr<BIns> skip = make_shared<BIns>(NON, NONE, 0, pool_label);
    //res.push_back(skip);
    bool need = false;
    for (auto &glob_var:machineModule->globalVariables) {
        string name = s_p_c<GlobalValue>(glob_var)->name + to_string(const_pool_id) + "_whitee_" +
                      to_string(const_pool_id);
        string value = s_p_c<GlobalValue>(glob_var)->name;
        shared_ptr<GlobalIns> glob_var_label = make_shared<GlobalIns>(name, value);
        res.push_back(glob_var_label);
        need = true;
    }
    for (auto &glob_const:machineModule->globalConstants) {
        string name = s_p_c<ConstantValue>(glob_const)->name + to_string(const_pool_id) + "_whitee_" +
                      to_string(const_pool_id);
        string value = s_p_c<ConstantValue>(glob_const)->name;
        shared_ptr<GlobalIns> glob_const_label = make_shared<GlobalIns>(name, value);
        res.push_back(glob_const_label);
        need = true;
    }
    for (int num:invalid_imm) {
        string name;
        if (num < 0) {
            name = "invalid_imm_" + to_string(const_pool_id) + "__" + to_string(abs(num));
        } else {
            name = "invalid_imm_" + to_string(const_pool_id) + "_" + to_string(num);
        }
        string value = to_string(num);
        shared_ptr<GlobalIns> invalid_num_label = make_shared<GlobalIns>(name, value);
        res.push_back(invalid_num_label);
        need = true;
    }
    invalid_imm.clear();
    string next_name = "next" + to_string(const_pool_id);
    string value;
    shared_ptr<GlobalIns> next = make_shared<GlobalIns>(next_name, value);
    if (need) {
        res.insert(res.begin(), skip);
        res.push_back(next);
        const_pool_id++;
    }
    return res;
}

string BinaryIns::toString() {
    string type = instype2string.at(this->type);
    string op1_ = state2string.at(op1->state) + op1->value;
    string op2_ = state2string.at(op2->state) + op2->value;
    string rd_ = state2string.at(rd->state) + rd->value;
    string cond = cond2string.at(this->cond);
    string stype = stype2string.at(this->shift->type);
    string out = type + cond + " " + rd_ + ", " + op1_ + ", " + op2_ + stype;
    return out;
}

string TriIns::toString() {
    string type = instype2string.at(this->type);
    string op1_ = state2string.at(op1->state) + op1->value;
    string op2_ = state2string.at(op2->state) + op2->value;
    string op3_ = state2string.at(op3->state) + op3->value;
    string rd_ = state2string.at(rd->state) + rd->value;
    string cond = cond2string.at(this->cond);
    string stype = stype2string.at(this->shift->type);
    string out = type + cond + " " + rd_ + ", " + op1_ + ", " + op2_ + ", " + op3_ + stype;
    return out;
}

string MemoryIns::toString() {
    string type = instype2string.at(this->type);
    string op1 = state2string.at(rd->state) + rd->value;
    string op2 = state2string.at(base->state) + base->value;
    string op3 = state2string.at(offset->state) + offset->value;
    string cond = cond2string.at(this->cond);
    string stype = stype2string.at(this->shift->type);
    string out = type + cond + " " + op1 + ", [" + op2 + ", " + op3 + "]" + stype;
    return out;
}

string PseudoLoad::toString() {
    string type = instype2string.at(this->type);
    string op1 = state2string.at(rd->state) + rd->value;
    string op2 = state2string.at(label->state) + label->value;
    string cond = cond2string.at(this->cond);
    string stype = stype2string.at(this->shift->type);
    string out = type + cond + " ," + op1 + ", " + op2 + stype;
    return out;
}

string StackIns::toString() {
    string type = instype2string.at(this->type);
    string cond = cond2string.at(this->cond);
    string stype = stype2string.at(this->shift->type);
    string out = type + cond + "{";
    for (auto &operand:regs) {
        out.append(state2string.at(operand->state) + operand->value + ", ");
    }
    out.append("}");
    return out;
}

string CmpIns::toString() {
    string type = instype2string.at(this->type);
    string cond = cond2string.at(this->cond);
    string stype = stype2string.at(this->shift->type);
    string op1_ = state2string.at(op1->state) + op1->value;
    string op2_ = state2string.at(op2->state) + op2->value;
    string out = type + cond + " " + op1_ + ", " + op2_ + " " + stype;
    return out;
}

string MovIns::toString() {
    string type = instype2string.at(this->type);
    string cond = cond2string.at(this->cond);
    string stype = stype2string.at(this->shift->type);
    string op1_ = state2string.at(op1->state) + op1->value;
    string op2_ = state2string.at(op2->state) + op2->value;
    string out = type + cond + " " + op1_ + ", " + op2_ + " " + stype;
    return out;
}

string BIns::toString() {
    string type = instype2string.at(this->type);
    string cond = cond2string.at(this->cond);
    string out = type + cond + " " + label;
    return out;
}

string BXIns::toString() {
    string type = instype2string.at(this->type);
    string cond = cond2string.at(this->cond);
    string out = type + cond + " LR";
    return out;
}

string BLIns::toString() {
    string type = instype2string.at(this->type);
    string cond = cond2string.at(this->cond);
    string out = type + cond + " " + label;
    return out;
}

string GlobalIns::toString() {
    string type = instype2string.at(this->type);
    string out = type + " " + this->name + " " + this->value;
    return out;
}

string Comment::toString() {
    return content;
}
