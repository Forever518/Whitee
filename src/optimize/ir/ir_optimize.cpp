#include "ir_optimize.h"
#include "../../basic/std/compile_std.h"

// lazy to check if the IR changed after a pass.
const unsigned int OPTIMIZE_TIMES = 10;

extern bool needIrCheck;
extern bool needIrPassCheck;

void optimizeIr(shared_ptr<Module> &module, OptimizeLevel level) {
    for (int i = 0; i < OPTIMIZE_TIMES; ++i)
        deadCodeElimination(module);

    for (int i = 0; i < OPTIMIZE_TIMES; ++i) {
        globalIrCorrect = true;
        if (level >= O1) {
            readOnlyVariableToConstant(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Non-write variable to constant." << endl;
        }

        if (level >= O1) {
            constantFolding(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Constant Folding." << endl;
        }

        if (level >= O3) {
            localArrayFolding(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Local Array Folding." << endl;
        }

        if (level >= O3) {
            deadArrayDelete(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Dead Array Delete." << endl;
        }

        if (level >= O3) {
            arrayExternalLift(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Array External Lift." << endl;
        }

        if (level >= O2) {
            deadBlockCodeGroupDelete(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Dead Block Code Group Delete." << endl;
        }

        if (level >= O1) {
            loopInvariantCodeMotion(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Loop Invariant Code Motion." << endl;
        }

        if (level >= O1) {
            localCommonSubexpressionElimination(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module))
                cerr << "Error: Local Common "
                        "Subexpression Elimination." << endl;
        }

        if (level >= O1) {
            constantBranchConversion(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Constant Branch Conversion." << endl;
        }

        if (level >= O1) {
            blockCombination(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Block Combination." << endl;
        }

        if (level >= O2) {
            functionInline(module);
            deadCodeElimination(module);
            if (needIrPassCheck && !irCheck(module)) cerr << "Error: Function Inline." << endl;
        }

        if (_debugIrOptimize) {
            const string fileName = debugMessageDirectory + "optimize"
                                    + _SLASH_STRING + "ir_op_pass_" + to_string(i + 1) + ".txt";
            ofstream irOptimizeStream(fileName, ios::out | ios::trunc);
            irOptimizeStream << module->toString() << endl;
            irOptimizeStream.close();
        }

        if (needIrCheck && !irCheck(module)) {
            cout << "Error: IR is not correct after optimize pass " << to_string(i + 1) << "." << endl;
            cerr << "Error: IR is not correct after optimize pass " << to_string(i + 1) << "." << endl;
            exit(_IR_OP_CHK_ERR);
        }
    }

    for (int i = 0; i < OPTIMIZE_TIMES; ++i)
        deadCodeElimination(module);

    if (_debugIr) {
        const string fileName = debugMessageDirectory + "ir_optimize.txt";
        ofstream irOptimizeStream(fileName, ios::out | ios::trunc);
        irOptimizeStream << module->toString() << endl;
        irOptimizeStream.close();
    }
    irUserCheck = true;
    if (needIrCheck && !irCheck(module)) {
        cout << "Error: IR is not correct after optimize." << endl;
        cerr << "Error: IR is not correct after optimize." << endl;
        exit(_IR_OP_CHK_ERR);
    }
    endOptimize(module, level);
}
