//
// Created by ameya on 2/20/2025.
//

#ifndef BITMAPMANAGER_H
#define BITMAPMANAGER_H
#include "Block.h"
#include "../interface/BlockManager.h"


class BitmapManager
{
public:
    BitmapManager(block_index_t startBlock, block_index_t numBlocks, block_index_t size, BlockManager* blockManager);

private:
    block_index_t startBlock;
    block_index_t numBlocks;
    block_index_t size;
    BlockManager* blockManager;
};


#endif //BITMAPMANAGER_H
