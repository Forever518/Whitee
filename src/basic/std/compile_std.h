#ifndef COMPILER_COMPILE_STD_H
#define COMPILER_COMPILE_STD_H

extern bool _debugSyntax;
extern bool _debugLexer;
extern bool _debugIr;
extern bool _debugAst;
extern bool _debugIrOptimize;
extern bool _debugMachineIr;

extern bool _isBuildingIr;

extern bool _optimizeMachineIr;
extern bool _optimizeDivAndMul;

enum OptimizeLevel {
    O0,
    O1,
    O2,
    O3
};

#define s_p_c static_pointer_cast

#define _W_LEN 4
#define _GLB_REG_CNT 9
#define _TMP_REG_CNT 5
#define _GLB_REG_START 4
#define _TMP_REG_START 0
#define _LOOP_WEIGHT_BASE 10
#define _MAX_DEPTH 6
#define _MAX_LOOP_WEIGHT 1000000000

#define _SCO_SUCCESS 0
#define _SCO_ARG_ERR -1
#define _SCO_HELP -2
#define _SCO_DBG_ERR -3
#define _SCO_CHK_ERR -4
#define _SCO_DBG_PATH_ERR -5
#define _SCO_OP_ERR -6
#define _SCO_ST_ERR -7

#define _INIT_SUCCESS 0
#define _INIT_CRT_ERR -26

#define _LEX_ERR -51
#define _IR_CHK_ERR -61
#define _IR_OP_CHK_ERR -62
#define _MACH_IR_ERR -71

#if defined(WIN32)
#define _SLASH_CHAR '\\'
#define _SLASH_STRING "\\"
#define _O_SLASH_CHAR '/'
#else
#define _SLASH_CHAR '/'
#define _SLASH_STRING "/"
#define _O_SLASH_CHAR '\\'
#endif

#endif
