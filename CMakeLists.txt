cmake_minimum_required(VERSION 3.13)
project(whitee)

math(EXPR stack_size "16*1024*1024")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")

if (UNIX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,stack-size=${stack_size}")
else ()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--stack,${stack_size}")
endif ()

add_executable(whitee
        src/main.cpp
        src/basic/hash/pair_hash.h
        src/basic/std/compile_std.h
        src/basic/std/compile_std.cpp
        src/front/lexer/lexer.h
        src/front/lexer/lexer.cpp
        src/front/lexer/token_info.cpp
        src/front/syntax/syntax_tree.h
        src/front/syntax/syntax_tree.cpp
        src/front/syntax/syntax_analyze.cpp
        src/front/syntax/syntax_analyze.h
        src/front/syntax/symbol_table.cpp
        src/front/syntax/symbol_table.h
        src/ir/ir_build.cpp
        src/ir/ir.h
        src/ir/ir.cpp
        src/ir/ir_build.h
        src/ir/ir_output.cpp
        src/ir/ir_ssa.cpp
        src/ir/ir_ssa.h
        src/ir/ir_utils.h
        src/ir/ir_utils.cpp
        src/ir/ir_check.h
        src/ir/ir_check.cpp
        src/machine_ir/machine_ir.h
        src/machine_ir/machine_ir.cpp
        src/machine_ir/machine_ir_build.h
        src/machine_ir/machine_ir_build.cpp
        src/optimize/syntax/change_cond_divid_into_mult.cpp
        src/optimize/machine/machine_optimize.h
        src/optimize/machine/machine_optimize.cpp
        src/optimize/machine/machine_opt_util.cpp
        src/optimize/ir/ir_optimize.h
        src/optimize/ir/ir_optimize.cpp
        src/optimize/ir/constant_folding.cpp
        src/optimize/ir/dead_code_elimination.cpp
        src/optimize/ir/function_inline.cpp
        src/optimize/ir/constant_branch_conversion.cpp
        src/optimize/ir/end_optimize.cpp
        src/optimize/ir/block_combination.cpp
        src/optimize/ir/read_only_variable_to_constant.cpp
        src/optimize/ir/local_array_folding.cpp
        src/optimize/ir/dead_array_delete.cpp
        src/optimize/ir/array_external_lift.cpp
        src/optimize/ir/calculate_variable_weight.cpp
        src/optimize/ir/register_alloc.cpp
        src/optimize/ir/dead_block_code_group_delete.cpp
        src/optimize/ir/loop_invariant_code_motion.cpp
        src/optimize/ir/local_common_subexpression_elimination.cpp
        )
