//
// Created by ameya on 2/21/2025.
//

#ifndef INODETABLE_H
#define INODETABLE_H
#include <cstring>
#include "Block.h"
#include "../interface/BlockManager.h"


class InodeTable
{
public:
    // InodeTable(block_index_t startBlock, inode_index_t numBlocks, inode_index_t size, BlockManager* blockManager);
    InodeTable(block_index_t startBlock, inode_index_t numBlocks, inode_index_t size, inode_index_t inodeRegionStart,
               BlockManager* blockManager);
    static bool initialize(block_index_t startBlock, inode_index_t numBlocks, BlockManager* blockManager);
    inode_index_t getFreeInodeNumber();
    bool setInodeLocation(block_index_t inodeNumber, inode_index_t location);
    inode_index_t getInodeLocation(block_index_t inodeNumber);
    bool writeInode(inode_index_t inodeLocation, inode_t& inode);
    bool readInode(inode_index_t inodeLocation, inode_t& inode);

    // outBuffer must be able to hold TABLE_ENTRIES_PER_BLOCK entries.
    bool readInodeBlock(inode_index_t blockIndex, inode_index_t* outBuffer);

private:
    block_index_t startBlock;
    inode_index_t numBlocks;
    inode_index_t size;
    BlockManager* blockManager;
    uint32_t inodeRegionStart;
};


#endif //INODETABLE_H
