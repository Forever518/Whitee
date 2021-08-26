#ifndef COMPILER_SYNTAX_ANALYZE_H
#define COMPILER_SYNTAX_ANALYZE_H

#include "syntax_tree.h"
#include <memory>
#include <unordered_map>

extern shared_ptr<CompUnitNode> syntaxAnalyze();

extern bool openFolder;
extern std::unordered_map<std::string, std::string> usageNameListOfVarSingleUseInUnRecursionFunction;

extern void changeCondDivideIntoMul(shared_ptr<CondNode> &condNode);

#endif