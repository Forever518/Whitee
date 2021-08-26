#include "ir_check.h"

#include <iostream>

bool globalIrCorrect = true;
bool irUserCheck = false;

bool globalWarningPermit = false;

void irError(string &&);

void irWarning(string &&);

void functionCheck(const shared_ptr<Function> &);

void basicBlockCheck(const shared_ptr<BasicBlock> &);

void instructionCheck(const shared_ptr<Instruction> &);

void phiCheck(const shared_ptr<PhiInstruction> &);

bool irCheck(const shared_ptr<Module> &module) {
    for (auto &cst : module->globalConstants) {
        for (auto &user : cst->users) if (!user->valid) irError("global constant has invalid users.");
        if (irUserCheck && cst->users.empty()) irError("constant has no users.");
    }
    for (auto &glb : module->globalVariables) {
        for (auto &user : glb->users) if (!user->valid) irError("global variable has invalid users.");
        if (irUserCheck && glb->users.empty()) irError("global variable has no users.");
    }
    for (auto &str : module->globalStrings) {
        for (auto &user : str->users) if (!user->valid) irError("global string has invalid users.");
        if (irUserCheck && str->users.empty()) irError("string has no users.");
    }
    for (auto &func : module->functions) {
        functionCheck(func);
    }
    return globalIrCorrect;
}

void irError(string &&message) {
    cerr << "IR Error: " << message << endl;
    globalIrCorrect = false;
}

void irWarning(string &&message) {
    if (globalWarningPermit)
        cerr << "IR Warning: " << message << endl;
}

void functionCheck(const shared_ptr<Function> &func) {
    if (!func->valid && !func->callers.empty())
        irError("function " + func->name + " is not valid but other functions call it.");
    for (auto &callee : func->callees) {
        if (!callee->valid)
            irError("function " + func->name + "'s callee is not valid.");
        if (callee->callers.count(func) == 0)
            irError("function " + func->name + " callee-caller is not two-way.");
    }
    for (auto &caller : func->callers) {
        if (!caller->valid)
            irError("function " + func->name + "'s caller is not valid.");
        if (caller->callees.count(func) == 0)
            irError("function " + func->name + " caller-callee is not two way.");
    }
    for (auto &arg : func->params) {
        if (!arg->valid)
            irError("function " + func->name + "'s parameter is not valid.");
        if (arg->valueType != ValueType::PARAMETER)
            irError("function " + func->name + "'s parameter is not a ParameterValue.");
    }
    if (func->blocks.empty())
        irWarning("function " + func->name + " has not basic block.");
    else if (func->entryBlock != func->blocks.at(0))
        irError("function " + func->name + "'s entryBlock does not equal to blocks[0].");
    if (!func->entryBlock->predecessors.empty())
        irError("function " + func->name + "'s entryBlock has predecessor.");
    for (auto &bb : func->blocks) {
        if (bb->function != func)
            irError("there are basic blocks whose function isn't equal to its father function.");
        basicBlockCheck(bb);
    }
}

void basicBlockCheck(const shared_ptr<BasicBlock> &bb) {
    for (auto &it : bb->predecessors) {
        if (it->successors.count(bb) == 0)
            irError("block successor-predecessor is not two way.");
        if (!it->valid)
            irError("predecessor is not valid.");
    }
    for (auto &it : bb->successors) {
        if (it->predecessors.count(bb) == 0)
            irError("block predecessor-successor is not two way.");
        if (!it->valid)
            irError("successor is not valid.");
    }
    if (!bb->instructions.empty()
        && (*(bb->instructions.end() - 1))->type != InstructionType::JMP
        && (*(bb->instructions.end() - 1))->type != InstructionType::BR
        && (*(bb->instructions.end() - 1))->type != InstructionType::RET) {
        irError("block ends with non-jump instruction.");
    }
    bool cmpMark = false;
    for (auto &ins : bb->instructions) {
        if (cmpMark && ins->type != BR)
            irError("cmp Instruction must be followed by a branch.");
        if (!ins->valid)
            irError("block's instruction is not valid.");
        if (ins->block != bb)
            irError("block's instruction's block is not itself.");
        if (ins->type == CMP) cmpMark = true;
        instructionCheck(ins);
    }
    for (auto &phi : bb->phis) {
        if (!phi->valid)
            irError("block's phi is not valid.");
        if (phi->block != bb)
            irError("phi's instruction's block is not itself.");
        phiCheck(phi);
    }
}

void instructionCheck(const shared_ptr<Instruction> &ins) {
    if (irUserCheck && ins->users.empty() && noResultTypes.count(ins->type) == 0)
        irError("instruction has no user.");
    if (ins->users.size() > 1 && ins->resultType == R_VAL_RESULT)
        irError("instruction has more than 1 users but is r-val.");
    if (ins->users.size() == 1 && ins->resultType == R_VAL_RESULT) {
        auto it = ins->users.begin();
        if ((*it)->valueType == ValueType::INSTRUCTION && s_p_c<Instruction>(*it)->block != ins->block)
            irError("r-val and its instruction's user is not in the same block.");
    }
    if (ins->type != InstructionType::PHI && ins->users.count(ins) != 0)
        irError("instruction uses itself.");
    for (auto &user : ins->users) {
        if (!user->valid)
            irError("instruction users contain invalid value in function " + ins->block->function->name
                    + " block " + to_string(ins->block->id) + ".");
    }
    switch (ins->type) {
        case InstructionType::BINARY:
        case InstructionType::CMP: {
            shared_ptr<BinaryInstruction> inst = s_p_c<BinaryInstruction>(ins);
            if (!inst->lhs->valid)
                irError("binary Instruction uses an invalid lhs.");
            else if (inst->lhs->users.count(inst) == 0)
                irError("binary Instruction's lhs users does not has itself.");
            if (!inst->rhs->valid)
                irError("binary Instruction uses an invalid lhs.");
            else if (inst->rhs->users.count(inst) == 0)
                irError("binary Instruction's rhs users does not has itself.");
            if (inst->type == CMP && inst->resultType != R_VAL_RESULT) {
                irError("cmp Instruction's result must be r-value.");
            }
            break;
        }
        case InstructionType::UNARY: {
            shared_ptr<UnaryInstruction> inst = s_p_c<UnaryInstruction>(ins);
            if (!inst->value->valid)
                irError("unary Instruction uses an invalid value.");
            else if (inst->value->users.count(inst) == 0)
                irError("unary Instruction's value users does not has itself.");
            break;
        }
        case InstructionType::LOAD: {
            shared_ptr<LoadInstruction> inst = s_p_c<LoadInstruction>(ins);
            if (!inst->address->valid)
                irError("load Instruction uses an invalid value.");
            else if (inst->address->users.count(inst) == 0)
                irError("load Instruction's address users does not has itself.");
            if (!inst->offset->valid)
                irError("load Instruction uses an invalid value.");
            else if (inst->offset->users.count(inst) == 0)
                irError("load Instruction's address users does not has itself.");
            break;
        }
        case InstructionType::RET: {
            shared_ptr<ReturnInstruction> inst = s_p_c<ReturnInstruction>(ins);
            if (inst->funcType == FuncType::FUNC_INT) {
                if (!inst->value->valid)
                    irError("return Instruction uses an invalid value.");
                else if (inst->value->users.count(inst) == 0)
                    irError("return Instruction's value users does not has itself.");
            }
            if (!inst->block->successors.empty())
                irError("return Instruction's bb must not have successors.");
            break;
        }
        case InstructionType::STORE: {
            shared_ptr<StoreInstruction> inst = s_p_c<StoreInstruction>(ins);
            if (!inst->value->valid)
                irError("store Instruction uses an invalid value.");
            else if (inst->value->users.count(inst) == 0)
                irError("store Instruction's value users does not has itself.");
            if (!inst->address->valid)
                irError("store Instruction uses an invalid address.");
            else if (inst->address->users.count(inst) == 0)
                irError("store Instruction's address users does not has itself.");
            if (!inst->offset->valid)
                irError("store Instruction uses an invalid offset.");
            else if (inst->offset->users.count(inst) == 0)
                irError("store Instruction's offset users does not has itself.");
            break;
        }
        case InstructionType::BR: {
            shared_ptr<BranchInstruction> inst = s_p_c<BranchInstruction>(ins);
            if (!inst->condition->valid)
                irError("branch Instruction uses an invalid condition.");
            else if (inst->condition->users.count(inst) == 0)
                irError("branch Instruction's condition users does not has itself.");
            if (inst->block->successors.size() != 2)
                irError("branch Instruction's successors size is not 2.");
            if (!inst->trueBlock->valid)
                irError("branch Instruction goto an invalid true block.");
            else if (inst->trueBlock->predecessors.count(inst->block) == 0
                     || inst->block->successors.count(inst->trueBlock) == 0)
                irError("branch Instruction's true block is not two-way.");
            if (!inst->falseBlock->valid)
                irError("branch Instruction goto an invalid false block.");
            else if (inst->falseBlock->predecessors.count(inst->block) == 0
                     || inst->block->successors.count(inst->falseBlock) == 0)
                irError("branch Instruction's false block is not two-way.");
            break;
        }
        case InstructionType::JMP: {
            shared_ptr<JumpInstruction> inst = s_p_c<JumpInstruction>(ins);
            if (inst->block->successors.size() != 1)
                irError("jump Instruction's successors size is not 1.");
            if (!inst->targetBlock->valid)
                irError("jump Instruction goto an invalid block.");
            else if (inst->targetBlock->predecessors.count(inst->block) == 0
                     || inst->block->successors.count(inst->targetBlock) == 0)
                irError("jump Instruction's target block is not two-way.");
            break;
        }
        case InstructionType::INVOKE: {
            shared_ptr<InvokeInstruction> inst = s_p_c<InvokeInstruction>(ins);
            if (inst->invokeType == InvokeType::COMMON) {
                if (!inst->targetFunction->valid)
                    irError("invoke an invalid function.");
            }
            for (auto &arg : inst->params) {
                if (!arg->valid)
                    irError("invoke Instruction's parameter is invalid.");
                else if (arg->users.count(inst) == 0)
                    irError("invoke Instruction's parameter users does not has itself.");
            }
            break;
        }
        case InstructionType::ALLOC: {
            break;
        }
        default: {
            irError("phi instruction should not be put into instructions.");
        }
    }
}

void phiCheck(const shared_ptr<PhiInstruction> &phi) {
    if (irUserCheck && phi->users.empty())
        irError("phi has no user.");
    if (phi->users.size() > 1 && phi->resultType == R_VAL_RESULT)
        irError("phi has more than 1 users but is r-val.");
    for (auto &user : phi->users) {
        if (user->valueType == ValueType::BASIC_BLOCK) {
            irError("phi have block user.");
        }
    }
    if (phi->operands.size() < 2) {
        irError("phi must have more than 1 operands.");
        return;
    }
    if (phi->operands.size() != phi->block->predecessors.size()) {
        irError("phi's operands is not equal to block's predecessors.");
    }
    for (auto &user : phi->users) {
        if (!user->valid && user->valueType != ValueType::BASIC_BLOCK)
            irError("phi has invalid user in function " + phi->block->function->name
                    + " block " + to_string(phi->block->id) + ".");
    }
    for (auto &it : phi->operands) {
        if (!it.first->valid)
            irError("phi's operand block is invalid.");
        else if (phi->block->predecessors.count(it.first) == 0)
            irError("phi's operand block is not in its predecessors.");
        if (it.second->valueType == ValueType::UNDEFINED)
            irWarning("phi has an undefined value.");
        if (!it.second->valid)
            irError("phi's operand is invalid.");
        else if (it.second->users.count(phi) == 0)
            irError("phi's operand users does not have itself.");
        else if (it.second->valueType == ValueType::INSTRUCTION
                 && s_p_c<Instruction>(it.second)->resultType != L_VAL_RESULT) {
            irError("phi's instruction operand is not a l-value.");
        }
    }
}
