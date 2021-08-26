#ifndef COMPILER_SYMBOL_TABLE_H
#define COMPILER_SYMBOL_TABLE_H

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include "../../basic/hash/pair_hash.h"
#include "syntax_tree.h"


/**
 * @brief: Symbol Table in a particular Block.
 * Include a fatherBlockId to find father block,
 * when find a symbol, and the symbol is not find in this block,
 * if isTop == false. try to find this symbol in its father block.
 * if isTop == true, fatherBlockId is {0, 0}.
 */
class SymbolTablePerBlock {
public:
    std::pair<int, int> blockId;
    bool isTop;
    std::pair<int, int> fatherBlockId;
    std::unordered_map<std::string, std::shared_ptr<SymbolTableItem>> symbolTableThisBlock;

    SymbolTablePerBlock(std::pair<int, int> &blockId, bool &isTop, std::pair<int, int> &fatherBlockId)
            : blockId(blockId), isTop(isTop), fatherBlockId(fatherBlockId) {};
};

extern std::unordered_map<std::pair<int, int>, std::shared_ptr<SymbolTablePerBlock>, pair_hash> symbolTable;

/**
 * @brief about Symbol Table
 * @details find symbol by start block id, Recursion find till the top block.
 * Should distinguish Function & Variable, for they may have the same name in one block.
 * @param startBlockId: finding start block id, change in this function during recursion.
 * @param symbolName: name of the finding symbol.
 * @return SymbolTableItem of the finding item.
 */
extern std::shared_ptr<SymbolTableItem> findSymbol(std::pair<int, int> startBlockId, std::string &symbolName, bool isF);

extern void insertSymbol(std::pair<int, int> blockId, std::shared_ptr<SymbolTableItem> symbol);

/// record per layer's num.
/// <2, ...> if ...max is 2, record <2, 2>, next layer2 block id is <2, 3>
extern std::unordered_map<int, int> blockLayerId2LayerNum;

/**
 * @brief distribute blockId and insert symbolTablePerBlock into symbolTable.
 */
extern std::pair<int, int> distributeBlockId(int layerId, std::pair<int, int> fatherBlockId);

#endif
