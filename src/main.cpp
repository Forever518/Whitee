#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>

#include "front/lexer/lexer.h"
#include "front/syntax/syntax_analyze.h"
#include "ir/ir_build.h"
#include "ir/ir_check.h"
#include "optimize/ir/ir_optimize.h"
#include "machine_ir/machine_ir_build.h"

using namespace std;

OptimizeLevel optimizeLevel = OptimizeLevel::O0;
bool needIrCheck = false;
bool needIrPassCheck = false;
bool openFolder = false;

string sourceCodeFile;
string targetCodeFile;
string debugMessageDirectory;

void printHelp(const char *exec);

int setCompileOptions(int argc, char **argv);

int initConfig();

int main(int argc, char **argv) {
    int r;

    if ((r = setCompileOptions(argc, argv)) != 0) return r;

    if (sourceCodeFile.empty() || targetCodeFile.empty()) {
        printHelp(argv[0]);
        return _SCO_ST_ERR;
    }

    if ((r = initConfig()) != 0) return r;

    if (!lexicalAnalyze(sourceCodeFile)) {
        cout << "Error: Cannot parse source code file." << endl;
        return _LEX_ERR;
    }

    cout << "[AST]" << endl << "Start building AST..." << endl;
    auto root = syntaxAnalyze();
    cout << "AST built successfully." << endl;
    if (_debugAst) {
        ofstream astStream;
        astStream.open(debugMessageDirectory + "ast.txt", ios::out | ios::trunc);
        astStream << root->toString(0) << endl;
        astStream.close();
        cout << "AST written to ast.txt." << endl;
    }
    cout << endl;

    cout << "[IR]" << endl << "Start building IR in SSA form..." << endl;
    auto module = buildIrModule(root);
    cout << "IR built successfully." << endl;
    removePhiUserBlocksAndMultiCmp(module);
    if (_debugIr) {
        ofstream irStream;
        irStream.open(debugMessageDirectory + "ir.txt", ios::out | ios::trunc);
        irStream << module->toString() << endl;
        irStream.close();
        cout << "IR written to ir.txt." << endl;
    }
    cout << endl;

    if (needIrCheck && !irCheck(module)) {
        cout << "Error: IR is not correct." << endl;
        return _IR_CHK_ERR;
    }

    _isBuildingIr = false;

    if (optimizeLevel != OptimizeLevel::O0) {
        cout << "[Optimize]" << endl << "Start IR optimizing..." << endl;
        optimizeIr(module, optimizeLevel);
        cout << "IR Optimized successfully." << endl;
        if (_debugIr) {
            cout << "Optimized IR written to ir_optimize.txt." << endl;
        }
        cout << endl;
    } else {
        fixRightValue(module);
        for (auto &func : module->functions) {
            phiElimination(func);
            getFunctionRequiredStackSize(func);
        }
        if (_debugIr) {
            ofstream irStream;
            irStream.open(debugMessageDirectory + "ir_final.txt", ios::out | ios::trunc);
            irStream << module->toString() << endl;
            irStream.close();
        }
    }

    cout << "[Machine IR]" << endl << "Start building Machine IR..." << endl;
    auto machineModule = buildMachineModule(module);
    cout << "Machine IR built successfully." << endl;
    cout << endl;

    cout << "[ARM]" << endl << "Start building ARM..." << endl;
    machineIrStream.open(targetCodeFile, ios::out | ios::trunc);
    machineModule->toARM();
    machineIrStream.close();
    cout << "ARM built successfully!" << endl;
    cout << endl;

    return 0;
}

int setCompileOptions(int argc, char **argv) {
    bool argOptimizeFlag = false;
    bool argSourceFlag = false;
    bool argOutputFlag = false;
    bool argDebugFlag = false;
    bool argCheckFlag = false;
    bool argDebugDirectoryFlag = false;

    if (argc < 1) {
        printHelp(argv[0]);
        return _SCO_ARG_ERR;
    }

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == "-h"s || argv[i] == "--help"s) {
            printHelp(argv[0]);
            return _SCO_HELP;
        } else if (argv[i] == "-S"s && !argSourceFlag) {
            argSourceFlag = true;
        } else if (argv[i] == "-o"s && !argOutputFlag) {
            argOutputFlag = true;
        } else if (!argDebugFlag && (string(argv[i]).find("-d") == 0 || string(argv[i]).find("--debug") == 0)) {
            argDebugFlag = true;
            if (argv[i][1] == '-') argv[i] += 7;
            else argv[i] += 2;
            if (*argv[i] == '\0') {
                if (i + 1 == argc) {
                    printHelp(argv[0]);
                    return _SCO_DBG_ERR;
                }
                ++i;
            }
            int debugLevel = strtol(argv[i], argv + i, 10);
            if (debugLevel == 0 || debugLevel > 3) {
                cout << "Error: The debug mode only has 3 levels: 1 2 or 3." << endl;
                printHelp(argv[0]);
                return _SCO_DBG_ERR;
            } else {
                if (debugLevel > 0) {
                    _debugIr = true;
                    _debugMachineIr = true;
                }
                if (debugLevel > 1) {
                    _debugAst = true;
                }
                if (debugLevel > 2) {
                    _debugIrOptimize = true;
                    _debugLexer = true;
                }
            }
        } else if (!argOptimizeFlag && (string(argv[i]).find("-O") == 0)) {
            argOptimizeFlag = true;
            argv[i] += 2;
            if (*argv[i] == '\0') {
                if (i + 1 == argc) {
                    printHelp(argv[0]);
                    return _SCO_OP_ERR;
                }
                ++i;
            }
            if (argv[i] == "1"s) optimizeLevel = OptimizeLevel::O1;
            else if (argv[i] == "2"s) optimizeLevel = OptimizeLevel::O2;
            else if (argv[i] == "3"s) optimizeLevel = OptimizeLevel::O3;
            if (optimizeLevel >= OptimizeLevel::O1) {
                _optimizeMachineIr = true;
            }
            if (optimizeLevel >= OptimizeLevel::O2) {
                openFolder = true;
                _optimizeDivAndMul = true;
            }
        } else if (!argCheckFlag && (string(argv[i]).find("-c") == 0 || string(argv[i]).find("--check") == 0)) {
            argCheckFlag = true;
            if (argv[i][1] == '-') argv[i] += 7;
            else argv[i] += 2;
            if (*argv[i] == '\0') {
                if (i + 1 == argc) {
                    printHelp(argv[0]);
                    return _SCO_CHK_ERR;
                }
                ++i;
            }
            int debugLevel = strtol(argv[i], argv + i, 10);
            if (debugLevel == 0 || debugLevel > 2) {
                cout << "Error: The IR check mode only has 2 levels: 1 or 2." << endl;
                printHelp(argv[0]);
                return _SCO_CHK_ERR;
            }
            if (debugLevel > 0) {
                needIrCheck = true;
            }
            if (debugLevel > 1) {
                needIrPassCheck = true;
            }
        } else if (!argDebugDirectoryFlag && string(argv[i]).find("--set-debug-path=") == 0) {
            argDebugDirectoryFlag = true;
            argv[i] += 17;
            if (*argv[i] == '\0') {
                printHelp(argv[0]);
                return _SCO_DBG_PATH_ERR;
            }
            debugMessageDirectory = argv[i];
        } else {
            if (targetCodeFile.empty()) targetCodeFile = argv[i];
            else if (sourceCodeFile.empty()) sourceCodeFile = argv[i];
            else {
                printHelp(argv[0]);
                return _SCO_ARG_ERR;
            }
        }
    }
    return _SCO_SUCCESS;
}

void printHelp(const char *exec) {
    cout << "Usage: " + string(exec)
            + " [-S] [-o] [-h | --help] [-d | --debug <level>]";
    if (strlen(exec) <= 22) {
        cout << endl << string(8 + strlen(exec), ' ')
             << "[-c | --check <level>] [--set-debug-path=<path>]"
             << endl << string(8 + strlen(exec), ' ')
             << "<target-file> <source-file> [-O <level>]" << endl;
    } else {
        cout << " [-c | --check <level>] [--set-debug-path=<path>]"
                " <target-file> <source-file> [-O <level>]" << endl;
    }
    cout << endl;
    cout << "    -S                  " << "generate assembly, can be omitted" << endl;
    cout << "    -o                  " << "set output file, can be omitted" << endl;
    cout << "    -h, --help          " << "show usage" << endl;
    cout << "    -d, --debug <level> " << "dump debug messages to certain files" << endl;
    cout << "                        " << "level 1: dump IR and optimized IR" << endl;
    cout << "                        " << "level 2: append AST" << endl;
    cout << "                        " << "level 3: append Lexer, each Optimization Pass "
                                          "and Register Allocation" << endl;
    cout << "    -c, --check <level> " << "check IR's relation" << endl;
    cout << "                        " << "level 1: check IR and final optimized IR only" << endl;
    cout << "                        " << "level 2: check IR after each optimization pass" << endl;
    cout << "    --set-debug-path=<path>" << endl;
    cout << "                        " << "set debug messages output path,"
                                          " default the same path with target assembly file" << endl;
    cout << "    <target-file>       " << "target assembly file in ARM-v7a" << endl;
    cout << "    <source-file>       " << "source code file matching SysY grammar" << endl;
    cout << "    -O <level>          " << "set optimization level, default non-optimization -O0" << endl;
    cout << endl;
}

bool createFolder(const char *path) {
    if (access(path, F_OK) != -1) return true;
    if (strlen(path) > FILENAME_MAX) {
        cout << "Error: debug path is too long." << endl;
        return false;
    }
    char tempPath[FILENAME_MAX] = {'\0'};
    for (int i = 0; i < strlen(path); ++i) {
        tempPath[i] = path[i];
        if ((tempPath[i] == _SLASH_CHAR) && access(tempPath, F_OK) == -1) {
            string command = "mkdir " + string(tempPath);
            system(command.c_str());
        }
    }
    return true;
}

int initConfig() {
    if (_debugIr) {
        while (debugMessageDirectory.find(_O_SLASH_CHAR) != string::npos) {
            debugMessageDirectory.replace(debugMessageDirectory.find(_O_SLASH_CHAR), 1, _SLASH_STRING);
        }
        while (targetCodeFile.find(_O_SLASH_CHAR) != string::npos) {
            targetCodeFile.replace(targetCodeFile.find(_O_SLASH_CHAR), 1, _SLASH_STRING);
        }
    }
    if (_debugIr && debugMessageDirectory.empty()) {
        string targetPath = string(targetCodeFile);
        string dirPath;
        if (targetPath.find_last_of(_SLASH_CHAR) != string::npos) {
            dirPath = targetPath.substr(0, targetPath.find_last_of(_SLASH_CHAR) + 1);
            targetPath = targetPath.substr(targetPath.find_last_of(_SLASH_CHAR) + 1, targetPath.size());
        }
        debugMessageDirectory = dirPath + "whitee-debug-" + targetPath + _SLASH_STRING;
    }
    if (_debugIr && debugMessageDirectory.at(debugMessageDirectory.size() - 1) != _SLASH_CHAR) {
        debugMessageDirectory += _SLASH_STRING;
    }
    if (_debugIr && !createFolder(debugMessageDirectory.c_str())) {
        return _INIT_CRT_ERR;
    }
    if (optimizeLevel > O0 && _debugIrOptimize
        && !createFolder((debugMessageDirectory + "optimize" + _SLASH_STRING).c_str())) {
        return _INIT_CRT_ERR;
    }
    return _INIT_SUCCESS;
}
