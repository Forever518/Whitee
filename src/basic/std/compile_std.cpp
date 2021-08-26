#include "compile_std.h"

bool _debugSyntax = false;
bool _debugLexer = false;
bool _debugIr = false;
bool _debugAst = false;
bool _debugIrOptimize = false;
bool _debugMachineIr = false;

bool _isBuildingIr = true; // Used for IR Phi.

bool _optimizeMachineIr = false;
bool _optimizeDivAndMul = false;
