//
// Created by ameya on 2/21/2025.
//

#ifndef INODETABLE_H
#define INODETABLE_H
#include "Block.h"
#include "../interface/BlockManager.h"


class InodeTable
{
public:
    InodeTable(block_index_t startBlock, inode_index_t numBlocks, inode_index_t size, BlockManager* blockManager);
    static bool initialize(block_index_t startBlock, inode_index_t numBlocks, BlockManager* blockManager);
    inode_index_t getFreeInodeNumber();
    bool setInodeLocation(block_index_t inodeNumber, inode_index_t location);
    inode_index_t getInodeLocation(block_index_t inodeNumber);
    static constexpr block_index_t NULL_VALUE = UINT32_MAX;

private:
    block_index_t startBlock;
    inode_index_t numBlocks;
    inode_index_t size;
    BlockManager* blockManager;
};


#endif //INODETABLE_H
