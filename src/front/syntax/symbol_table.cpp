#include "symbol_table.h"

#include <memory>
#include <utility>
#include <iostream>

std::shared_ptr<SymbolTableItem> findSymbol(std::pair<int, int> startBlockId, std::string &symbolName, bool isF) {
    std::string findName = (isF ? "F*" : "V*") + symbolName;
    if (symbolTable[startBlockId]->symbolTableThisBlock.find(findName) !=
        symbolTable[startBlockId]->symbolTableThisBlock.end()) {
        return symbolTable[startBlockId]->symbolTableThisBlock[findName];
    } else {
        while (!symbolTable[startBlockId]->isTop) {
            startBlockId = symbolTable[startBlockId]->fatherBlockId;
            if (symbolTable[startBlockId]->symbolTableThisBlock.find(findName) !=
                symbolTable[startBlockId]->symbolTableThisBlock.end()) {
                return symbolTable[startBlockId]->symbolTableThisBlock[findName];
            }
        }
    }
    cerr << "findSymbol: Source code may have errors." << endl;
    return nullptr;
} // NOLINT

void insertSymbol(std::pair<int, int> blockId, std::shared_ptr<SymbolTableItem> symbol) {
    symbolTable[blockId]->symbolTableThisBlock[symbol->uniqueName] = std::move(symbol);
}

void insertBlock(std::pair<int, int> blockId, std::pair<int, int> fatherBlockId) {
    bool isTop = (blockId.first == 0);
    symbolTable[blockId] = std::make_shared<SymbolTablePerBlock>(blockId, isTop, fatherBlockId);
}

std::pair<int, int> distributeBlockId(int layerId, std::pair<int, int> fatherBlockId) {
    if (blockLayerId2LayerNum.find(layerId) == blockLayerId2LayerNum.end()) {
        blockLayerId2LayerNum[layerId] = 0;
    }
    std::pair<int, int> blockId = {layerId, ++blockLayerId2LayerNum[layerId]};
    insertBlock(blockId, fatherBlockId);
    return blockId;
}
