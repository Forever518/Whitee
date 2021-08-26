#include "machine_optimize.h"

#include <iostream>

#define WORD_BIT 32

extern string convertImm(int, const string &, bool);

extern string convertImm(int, const string &);

bool isTemp(const string &reg);

extern int ins_count;
extern int const_pool_id;

vector<shared_ptr<MachineIns>>
divOptimization(shared_ptr<BinaryInstruction> &binaryIns, shared_ptr<MachineFunc> &machineFunc) {
    vector<shared_ptr<MachineIns>> res;

    shared_ptr<Value> b_ins = binaryIns;

    shared_ptr<Operand> op1 = make_shared<Operand>(REG, "2");
    shared_ptr<Value> lhs = binaryIns->lhs;
    bool release1 = readRegister(lhs, op1, machineFunc, res, true, true);

    shared_ptr<Operand> op2 = make_shared<Operand>(REG, "3");
    shared_ptr<Value> rhs = binaryIns->rhs;
    bool release2 = false;

    shared_ptr<Operand> rd = make_shared<Operand>(REG, "1");
    bool release_rd = writeRegister(b_ins, rd, machineFunc, res);
    int constValue = s_p_c<NumberValue>(binaryIns->rhs)->number;
    bool isMinus = constValue < 0;


    if (countPowerOfTwo(isMinus ? -constValue : constValue) != 0xffffffff) {
        constValue = isMinus ? -constValue : constValue;
        unsigned int shift1 = countPowerOfTwo(constValue);
        unsigned int shift2 = WORD_BIT - shift1;

        // get op1 signbit
        // op2 > 0
        shared_ptr<Operand> temp_rd = make_shared<Operand>(REG, "1");
        temp_rd->value = allocTempRegister();

        shared_ptr<BinaryIns> asr1 = make_shared<BinaryIns>(mit::ASR, NON, NONE, 31, op1, op2, temp_rd);
        res.push_back(asr1);

        shared_ptr<BinaryIns> add1 = make_shared<BinaryIns>(mit::ADD, NON, LSR, shift2, op1, temp_rd, rd);
        res.push_back(add1);

        if (isMinus) {
            shared_ptr<Operand> zeroImm = make_shared<Operand>(IMM, "0");
            shared_ptr<MovIns> mov1 = make_shared<MovIns>(temp_rd, zeroImm);
            res.push_back(mov1);

            shared_ptr<BinaryIns> sub1 = make_shared<BinaryIns>(mit::SUB, NON, ASR, shift1, temp_rd, rd, rd);
            res.push_back(sub1);
        } else {
            shared_ptr<BinaryIns> asr2 = make_shared<BinaryIns>(mit::ASR, NON, NONE, shift1, rd, op2, rd);
            res.push_back(asr2);
        }
        releaseTempRegister(temp_rd->value);
    } else {
        unsigned int magicNumber = calDivMagicNumber(abs(constValue));
        unsigned int logNumber = calDivShiftNumber(abs(constValue));

        release2 = writeRegister(rhs, op2, machineFunc, res);
        int low16 = magicNumber & 0x0000FFFFU;
        int high16 = (magicNumber & 0xFFFF0000U) >> 16U;
        shared_ptr<Operand> low = make_shared<Operand>(IMM, to_string(low16));
        shared_ptr<Operand> high = make_shared<Operand>(IMM, to_string(high16));
        shared_ptr<MovIns> movw = make_shared<MovIns>(mit::MOVW, NON, NONE, 0, op2, low);
        shared_ptr<MovIns> movt = make_shared<MovIns>(mit::MOVT, NON, NONE, 0, op2, high);
        res.push_back(movw);
        res.push_back(movt);

        shared_ptr<Operand> temp_rd = make_shared<Operand>(REG, "1");
        temp_rd->value = allocTempRegister();

        if (magicNumber < (1 << (WORD_BIT - 1))) {
            shared_ptr<TriIns> sumll1 = make_shared<TriIns>(mit::SMULL, NON, NONE, 0, op2, op1, op2, temp_rd);
            res.push_back(sumll1);

            shared_ptr<BinaryIns> asr2 = make_shared<BinaryIns>(mit::ASR, NON, NONE, logNumber, op2, op2, rd);
            res.push_back(asr2);

            shared_ptr<BinaryIns> add2 = make_shared<BinaryIns>(mit::ADD, NON, LSR, WORD_BIT - 1, rd, op2, rd);
            res.push_back(add2);
        } else {
            shared_ptr<TriIns> sumll1 = make_shared<TriIns>(mit::SMULL, NON, NONE, 0, op2, op1, op2, temp_rd);
            res.push_back(sumll1);

            shared_ptr<BinaryIns> add2 = make_shared<BinaryIns>(mit::ADD, NON, NONE, 0, op2, op1, rd);
            res.push_back(add2);

            shared_ptr<Operand> temp_rd = make_shared<Operand>(REG, "1");
            temp_rd->value = allocTempRegister();
            shared_ptr<BinaryIns> asr2 = make_shared<BinaryIns>(mit::ASR, NON, NONE, logNumber, rd, op2, temp_rd);
            releaseTempRegister(temp_rd->value);
            res.push_back(asr2);

            shared_ptr<BinaryIns> add3 = make_shared<BinaryIns>(mit::ADD, NON, LSR, WORD_BIT - 1, temp_rd, rd, rd);
            res.push_back(add3);
        }

        releaseTempRegister(temp_rd->value);
    }

    if (release2) releaseTempRegister(op2->value);
    if (release1) releaseTempRegister(op1->value);

    if (release_rd) {
        store2Memory(rd, binaryIns->id, machineFunc, res);
    }

    return res;
}

vector<shared_ptr<MachineIns>>
mulOptimization(shared_ptr<BinaryInstruction> &binaryIns, shared_ptr<MachineFunc> &machineFunc) {
    vector<shared_ptr<MachineIns>> res;

    shared_ptr<Operand> op1 = make_shared<Operand>(REG, "2");
    shared_ptr<Value> lhs = binaryIns->lhs;
    bool release1 = readRegister(lhs, op1, machineFunc, res, true, true);

    shared_ptr<Operand> op2 = make_shared<Operand>(REG, "3");
    bool release2 = false;

    shared_ptr<Operand> rd = make_shared<Operand>(REG, "1");
    shared_ptr<Value> b_ins = binaryIns;
    bool release_rd = writeRegister(b_ins, rd, machineFunc, res);
    int constValue = s_p_c<NumberValue>(binaryIns->rhs)->number;
    unsigned int shift;
    vector<int> ret;

    if (constValue == 0) {
        shared_ptr<Operand> zeroImm = make_shared<Operand>(IMM, "0");
        shared_ptr<MovIns> mov1 = make_shared<MovIns>(rd, zeroImm);
        res.push_back(mov1);
    } else if (constValue > 0) {
        if ((shift = countPowerOfTwo(constValue)) != 0xffffffff) {
            shared_ptr<BinaryIns> lsl1 = make_shared<BinaryIns>(mit::LSL, NON, NONE, shift, op1, op2, rd);
            res.push_back(lsl1);
        } else if ((shift = countPowerOfTwo(constValue + 1)) != 0xffffffff) {
            shared_ptr<BinaryIns> rsb1 = make_shared<BinaryIns>(mit::RSB, NON, LSL, shift, op1, op1, rd);
            res.push_back(rsb1);
        } else if ((shift = countPowerOfTwo(constValue - 1)) != 0xffffffff) {
            shared_ptr<BinaryIns> add1 = make_shared<BinaryIns>(mit::ADD, NON, LSL, shift, op1, op1, rd);
            res.push_back(add1);
        } else if ((ret = canConvertTwoPowMul(constValue)).size() > 1) {
            if (ret[0] == 1) {
                shared_ptr<BinaryIns> add1 = make_shared<BinaryIns>(mit::ADD, NON, LSL, ret[1], op1, op1, rd);
                res.push_back(add1);

                shared_ptr<BinaryIns> lsl1 = make_shared<BinaryIns>(mit::LSL, NON, NONE, ret[2], rd, op2, rd);
                res.push_back(lsl1);
            } else if (ret[0] == -1) {
                shared_ptr<BinaryIns> rsb1 = make_shared<BinaryIns>(mit::RSB, NON, LSL, ret[1], op1, op1, rd);
                res.push_back(rsb1);

                shared_ptr<BinaryIns> lsl1 = make_shared<BinaryIns>(mit::LSL, NON, NONE, ret[2], rd, op2, rd);
                res.push_back(lsl1);
            } else {
                cerr << "mulOptimization::ret[0] != 1 && ret[0] != -1" << endl;
            }
        } else {
            shared_ptr<Value> rhs = binaryIns->rhs;
            release2 = readRegister(rhs, op2, machineFunc, res, true, true);
            shared_ptr<BinaryIns> mul = make_shared<BinaryIns>(mit::MUL, NON, NONE, 0, op1, op2, rd);
            res.push_back(mul);
        }
    } else { // < 0
        if ((shift = countPowerOfTwo(-constValue)) != 0xffffffffU) {
            ///TO-DO if rd==op1??
            shared_ptr<Operand> temp_rd = make_shared<Operand>(REG, "1");
            temp_rd->value = allocTempRegister();
            shared_ptr<Operand> zeroImm = make_shared<Operand>(IMM, "0");
            shared_ptr<MovIns> mov1 = make_shared<MovIns>(NON, NONE, 0, temp_rd, zeroImm);
            res.push_back(mov1);

            shared_ptr<BinaryIns> sub1 = make_shared<BinaryIns>(mit::SUB, NON, LSL, shift, temp_rd, op1, rd);
            res.push_back(sub1);
            releaseTempRegister(temp_rd->value);
        } else if ((shift = countPowerOfTwo(-constValue + 1)) != 0xffffffff) {
            shared_ptr<BinaryIns> sub1 = make_shared<BinaryIns>(mit::SUB, NON, LSL, shift, op1, op1, rd);
            res.push_back(sub1);
        } else if ((shift = countPowerOfTwo(-constValue - 1)) != 0xffffffff) {
            shared_ptr<BinaryIns> add1 = make_shared<BinaryIns>(mit::ADD, NON, LSL, shift, op1, op1, rd);
            res.push_back(add1);
            shared_ptr<Operand> zeroImm = make_shared<Operand>(IMM, "0");
            shared_ptr<BinaryIns> rsb1 = make_shared<BinaryIns>(mit::RSB, NON, NONE, 0, rd, zeroImm, rd);
            res.push_back(rsb1);
        } else if ((ret = canConvertTwoPowMul(-constValue)).size() > 1) {
            if (ret[0] == 1) {
                shared_ptr<BinaryIns> add1 = make_shared<BinaryIns>(mit::ADD, NON, LSL, ret[1], op1, op1, rd);
                res.push_back(add1);

                shared_ptr<Operand> zeroImm = make_shared<Operand>(IMM, "0");
                shared_ptr<BinaryIns> rsb1 = make_shared<BinaryIns>(mit::RSB, NON, NONE, 0, rd, zeroImm, rd);
                res.push_back(rsb1);

                shared_ptr<BinaryIns> lsl1 = make_shared<BinaryIns>(mit::LSL, NON, NONE, ret[2], rd, op2, rd);
                res.push_back(lsl1);
            } else if (ret[0] == -1) {
                shared_ptr<BinaryIns> sub1 = make_shared<BinaryIns>(mit::SUB, NON, LSL, ret[1], op1, op1, rd);
                res.push_back(sub1);

                shared_ptr<BinaryIns> lsl1 = make_shared<BinaryIns>(mit::LSL, NON, NONE, ret[2], rd, op2, rd);
                res.push_back(lsl1);
            } else {
                cerr << "mulOptimization::ret[0] != 1 && ret[0] != -1" << endl;
            }
        } else {
            shared_ptr<Value> rhs = binaryIns->rhs;
            release2 = readRegister(rhs, op2, machineFunc, res, true, true);
            shared_ptr<BinaryIns> mul = make_shared<BinaryIns>(mit::MUL, NON, NONE, 0, op1, op2, rd);
            res.push_back(mul);
        }
    }

    if (release2) releaseTempRegister(op2->value);
    if (release1) releaseTempRegister(op1->value);

    if (release_rd) {
        store2Memory(rd, binaryIns->id, machineFunc, res);
    }

    return res;
}

void delete_imm_jump(shared_ptr<MachineModule> &machineModule) {
    for (auto &machineFunc:machineModule->machineFunctions) {
        for (int i = 0; i < machineFunc->machineBlocks.size(); i++) {
            auto machineBB = machineFunc->machineBlocks[i];
            for (auto it = machineBB->MachineInstructions.begin(); it != machineBB->MachineInstructions.end();) {
                if ((*it)->type == mit::BRANCH && (*it)->cond == NON) {
                    shared_ptr<BIns> jump = s_p_c<BIns>((*it));
                    if (i < machineFunc->machineBlocks.size() - 1 &&
                        machineFunc->machineBlocks[i + 1]->MachineInstructions[0]->type == mit::GLOBAL) {
                        shared_ptr<GlobalIns> next_block = s_p_c<GlobalIns>(
                                machineFunc->machineBlocks[i + 1]->MachineInstructions[0]);
                        if (jump->label == next_block->name) {
                            it = machineBB->MachineInstructions.erase(it);
                        } else {
                            it++;
                        }
                    } else {
                        it++;
                    }
                } else {
                    it++;
                }
            }
        }
    }
}

void delete_useless_compute(shared_ptr<MachineModule> &machineModule) {
    for (auto &machineFunc:machineModule->machineFunctions) {
        for (auto &machineBB: machineFunc->machineBlocks) {
            for (auto it = machineBB->MachineInstructions.begin(); it != machineBB->MachineInstructions.end();) {
                if ((*it)->type == mit::ADD || (*it)->type == mit::SUB) //jump
                {
                    shared_ptr<BinaryIns> compute = s_p_c<BinaryIns>((*it));
                    if (compute->op2->state == IMM && compute->op2->value == "0" &&
                        (compute->op1->value == compute->rd->value)) {
                        it = machineBB->MachineInstructions.erase(it);
                    } else {
                        it++;
                    }
                } else if ((*it)->type == mit::MOV) {
                    shared_ptr<MovIns> move = s_p_c<MovIns>((*it));
                    if (move->op1->state == REG && move->op2->state == REG && (move->op1->value == move->op2->value)) {
                        it = machineBB->MachineInstructions.erase(it);
                    } else {
                        it++;
                    }
                } else {
                    it++;
                }
            }
        }
    }
}

void reduce_redundant_move(shared_ptr<MachineModule> &machineModule) {
    for (auto &machineFunc:machineModule->machineFunctions) {
        for (auto &machineBB:machineFunc->machineBlocks) {
            for (auto it = machineBB->MachineInstructions.begin(); it != machineBB->MachineInstructions.end();) {
                if ((*it)->type == mit::MOV && it != machineBB->MachineInstructions.end() - 1 &&
                    isTemp(s_p_c<MovIns>(*it)->op1->value)) {
                    shared_ptr<MovIns> move = s_p_c<MovIns>((*it));
                    if ((*(it + 1))->type == mit::MOV) { //MOV R0, R1 MOV R2 R0 ---> MOV R2 R1
                        shared_ptr<MovIns> move1 = s_p_c<MovIns>((*(it + 1)));
                        if (move->op1->state == REG && move1->op2->state == REG &&
                            (move->op1->value == move1->op2->value)) {
                            move1->op2->state = move->op2->state;
                            move1->op2->value = move->op2->value;
                            it = machineBB->MachineInstructions.erase(it);
                        } else {
                            it++;
                        }
                    } else if ((*(it + 1))->type == mit::LOAD || (*(it + 1))->type == mit::STORE) {
                        shared_ptr<MemoryIns> memo = s_p_c<MemoryIns>((*(it + 1)));
                        if ((memo->base->value == move->op1->value) && move->op2->state == REG) {
                            memo->base->state = move->op2->state;
                            memo->base->value = move->op2->value;
                            it = machineBB->MachineInstructions.erase(it);
                        } else if (memo->type == mit::STORE && memo->rd->value == move->op1->value &&
                                   move->op2->state == REG) {
                            memo->rd->state = move->op2->state;
                            memo->rd->value = move->op2->value;
                            it = machineBB->MachineInstructions.erase(it);
                        } else if (memo->offset->state == REG && memo->offset->value == move->op1->value &&
                                   move->op2->state == REG) {
                            memo->offset->state = move->op2->state;
                            memo->offset->value = move->op2->value;
                            it = machineBB->MachineInstructions.erase(it);
                        } else {
                            it++;
                        }
                    } else {
                        it++;
                    }
                } else if (((*it)->type == mit::LOAD || (*it)->type == mit::PSEUDO_LOAD) &&
                           it != machineBB->MachineInstructions.end() - 1 && ((*(it + 1))->type == mit::MOV) &&
                           isTemp(s_p_c<MemoryIns>(*it)->rd->value)) {
                    shared_ptr<MovIns> move = s_p_c<MovIns>((*(it + 1)));
                    if ((*it)->type == mit::LOAD) {
                        shared_ptr<MemoryIns> load = s_p_c<MemoryIns>((*it));
                        if (move->op2->state == REG && move->op2->value == load->rd->value) {
                            load->rd->state = move->op1->state;
                            load->rd->value = move->op1->value;
                            it = machineBB->MachineInstructions.erase(it + 1);
                        } else {
                            it++;
                        }
                    } else {
                        shared_ptr<PseudoLoad> pload = s_p_c<PseudoLoad>((*it));
                        if (move->op2->state == REG && move->op2->value == pload->rd->value) {
                            pload->rd->state = move->op1->state;
                            pload->rd->value = move->op1->value;
                            it = machineBB->MachineInstructions.erase(it + 1);
                        } else {
                            it++;
                        }
                    }
                } else {
                    it++;
                }
            }
        }
    }
}

bool isTemp(const string &reg) {
    return reg == "0" || reg == "1" || reg == "2" || reg == "3" || reg == "14";
}

void merge_mla_and_mls(shared_ptr<MachineModule> &machineModule) {
    for (auto &machineFunc:machineModule->machineFunctions) {
        for (auto &machineBB:machineFunc->machineBlocks) {
            for (auto it = machineBB->MachineInstructions.begin(); (it + 1) != machineBB->MachineInstructions.end();) {
                if ((*it)->type == mit::MUL && isTemp(s_p_c<BinaryIns>(*it)->rd->value)) {
                    shared_ptr<BinaryIns> mul = s_p_c<BinaryIns>((*it));
                    if ((*(it + 1))->type == mit::ADD) {
                        shared_ptr<BinaryIns> add = s_p_c<BinaryIns>(*(it + 1));
                        if (add->op2->state == REG && add->op1->value == mul->rd->value) {
                            shared_ptr<TriIns> mla = make_shared<TriIns>(mit::MLA, NON, NONE, 0, mul->op1, mul->op2,
                                                                         add->op2, add->rd);
                            it = machineBB->MachineInstructions.erase(it);
                            it = machineBB->MachineInstructions.erase(it);
                            machineBB->MachineInstructions.insert(it, mla);
                        } else if (add->op2->state == REG && add->op2->value == mul->rd->value) {
                            shared_ptr<TriIns> mla = make_shared<TriIns>(mit::MLA, NON, NONE, 0, mul->op1, mul->op2,
                                                                         add->op1, add->rd);
                            it = machineBB->MachineInstructions.erase(it);
                            it = machineBB->MachineInstructions.erase(it);
                            machineBB->MachineInstructions.insert(it, mla);
                        } else {
                            it++;
                        }
                    } else if ((*(it + 1))->type == mit::COMMENT && (*(it + 2))->type == mit::ADD) {
                        shared_ptr<BinaryIns> add = s_p_c<BinaryIns>(*(it + 2));
                        if (add->op2->state == REG && add->op1->value == mul->rd->value) {
                            shared_ptr<TriIns> mla = make_shared<TriIns>(mit::MLA, NON, NONE, 0, mul->op1, mul->op2,
                                                                         add->op2, add->rd);
                            it = machineBB->MachineInstructions.erase(it);
                            it++;//comment
                            it = machineBB->MachineInstructions.erase(it);
                            machineBB->MachineInstructions.insert(it, mla);
                        } else if (add->op2->state == REG && add->op2->value == mul->rd->value) {
                            shared_ptr<TriIns> mla = make_shared<TriIns>(mit::MLA, NON, NONE, 0, mul->op1, mul->op2,
                                                                         add->op1, add->rd);
                            it = machineBB->MachineInstructions.erase(it);
                            it++;//comment
                            it = machineBB->MachineInstructions.erase(it);
                            machineBB->MachineInstructions.insert(it, mla);
                        } else {
                            it++;
                        }
                    } else if ((*(it + 1))->type == mit::SUB) {
                        shared_ptr<BinaryIns> sub = s_p_c<BinaryIns>(*(it + 1));
                        if (sub->op2->state == REG && sub->op2->value == mul->rd->value) {
                            shared_ptr<TriIns> mla = make_shared<TriIns>(mit::MLS, NON, NONE, 0, mul->op1, mul->op2,
                                                                         sub->op1, sub->rd);
                            it = machineBB->MachineInstructions.erase(it);
                            it = machineBB->MachineInstructions.erase(it);
                            machineBB->MachineInstructions.insert(it, mla);
                        } else {
                            it++;
                        }
                    } else if ((*(it + 1))->type == mit::COMMENT && (*(it + 2))->type == mit::SUB) {
                        shared_ptr<BinaryIns> sub = s_p_c<BinaryIns>(*(it + 2));
                        if (sub->op2->state == REG && sub->op2->value == mul->rd->value) {
                            shared_ptr<TriIns> mla = make_shared<TriIns>(mit::MLS, NON, NONE, 0, mul->op1, mul->op2,
                                                                         sub->op1, sub->rd);
                            it = machineBB->MachineInstructions.erase(it);
                            it++;//comment
                            it = machineBB->MachineInstructions.erase(it);
                            machineBB->MachineInstructions.insert(it, mla);
                        } else {
                            it++;
                        }
                    } else {
                        it++;
                    }
                } else {
                    it++;
                }
            }
        }
    }
}

void exchange_branch_ins(shared_ptr<MachineModule> &machineModule) {
    for (auto &machineFunc:machineModule->machineFunctions) {
        for (int i = 0; i < machineFunc->machineBlocks.size(); i++) {
            auto machineBB = machineFunc->machineBlocks[i];
            for (auto it = machineBB->MachineInstructions.begin(); it != machineBB->MachineInstructions.end();) {
                if ((*it)->type == mit::BRANCH && it < machineBB->MachineInstructions.end() - 1 &&
                    (*(it + 1))->type == mit::BRANCH) {
                    shared_ptr<BIns> branch1 = s_p_c<BIns>((*it));
                    shared_ptr<BIns> branch2 = s_p_c<BIns>((*(it + 1)));
                    if (i < machineFunc->machineBlocks.size() - 1 &&
                        machineFunc->machineBlocks[i + 1]->MachineInstructions[0]->type == mit::GLOBAL) {
                        shared_ptr<GlobalIns> next_block = s_p_c<GlobalIns>(
                                machineFunc->machineBlocks[i + 1]->MachineInstructions[0]);
                        if (branch1->cond != NON && branch2->cond == NON && branch1->label == next_block->name) {
                            if (branch1->cond == EQ) {
                                branch2->cond = NE;
                            } else if (branch1->cond == NE) {
                                branch2->cond = EQ;
                            } else if (branch1->cond == GT) {
                                branch2->cond = LE;
                            } else if (branch1->cond == GE) {
                                branch2->cond = LS;
                            } else if (branch1->cond == LS) {
                                branch2->cond = GE;
                            } else if (branch1->cond == LE) {
                                branch2->cond = GT;
                            }
                            it = machineBB->MachineInstructions.erase(it);
                        } else {
                            it++;
                        }
                    } else {
                        it++;
                    }
                } else {
                    it++;
                }
            }
        }
    }
}
