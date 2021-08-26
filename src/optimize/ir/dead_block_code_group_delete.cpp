#include "ir_optimize.h"

#include <queue>
#include <ctime>

time_t startDeadBlockCodeGroupDeleteTime;
unsigned long DEAD_BLOCK_CODE_GROUP_DELETE_TIMEOUT = 10;

inline bool checkDeadBlockCodeGroupDeleteTimeout(time_t startTime, unsigned long timeout) {
    return time(nullptr) - startTime > timeout / OPTIMIZE_TIMES;
}

bool judgeOnlyHasPhiUsers(const shared_ptr<Instruction> &ins) {
    if (noResultTypes.count(ins->type) != 0 || ins->type == PHI) return false;
    unordered_set<shared_ptr<Value>> visit;
    queue<shared_ptr<Value>> visitQueue;
    visit.insert(ins);
    visitQueue.push(ins);
    while (!visitQueue.empty()) {
        if (checkDeadBlockCodeGroupDeleteTimeout(startDeadBlockCodeGroupDeleteTime,
                                                 DEAD_BLOCK_CODE_GROUP_DELETE_TIMEOUT))
            return false;
        shared_ptr<Value> top = visitQueue.front();
        visitQueue.pop();
        for (auto &user : top->users) {
            if (user->valueType != INSTRUCTION) {
                cerr << "Error occurs in process remove unused instructions: user is not an instruction." << endl;
                return false;
            }
            shared_ptr<Instruction> userIns = s_p_c<Instruction>(user);
            if ((userIns->type != PHI && userIns->block != ins->block) || noResultTypes.count(userIns->type) != 0) {
                return false;
            } else if (visit.count(user) == 0) {
                visit.insert(user);
                visitQueue.push(user);
            }
        }
    }
    for (auto &val : visit) {
        if (dynamic_cast<PhiInstruction *>(val.get())) {
            shared_ptr<PhiInstruction> phiVal = s_p_c<PhiInstruction>(val);
            phiVal->block->phis.erase(phiVal);
            phiVal->abandonUse();
        } else if (val->valueType == INSTRUCTION) {
            shared_ptr<Instruction> insVal = s_p_c<Instruction>(val);
            auto it = insVal->block->instructions.begin();
            while (it != insVal->block->instructions.end()) {
                if (*it == insVal) {
                    insVal->abandonUse();
                    insVal->block->instructions.erase(it);
                    break;
                }
                ++it;
            }
        } else {
            cerr << "Error occurs in judge only phi users: non-instruction value." << endl;
        }
    }
    return true;
}

void deadBlockCodeGroupDelete(shared_ptr<Module> &module) {
    startDeadBlockCodeGroupDeleteTime = time(nullptr);
    for (auto &func : module->functions) {
        vector<shared_ptr<BasicBlock>> blocks = func->blocks;
        JUDGE_PHI_ONLY_VALUE_START:
        for (auto &bb : blocks) {
            if (checkDeadBlockCodeGroupDeleteTimeout(startDeadBlockCodeGroupDeleteTime,
                                                     DEAD_BLOCK_CODE_GROUP_DELETE_TIMEOUT))
                return;
            for (auto &ins : bb->instructions) {
                if (judgeOnlyHasPhiUsers(ins)) goto JUDGE_PHI_ONLY_VALUE_START;
            }
        }
    }
}
