//
// Created by ameya on 2/21/2025.
//

#include "InodeTable.h"

InodeTable::InodeTable(const block_index_t startBlock, const inode_index_t numBlocks, const inode_index_t size, const inode_index_t inodeRegionStart,
                       BlockManager* blockManager): startBlock(startBlock), numBlocks(numBlocks), size(size), inodeRegionStart(inodeRegionStart),
                                                    blockManager(blockManager)
{
}

bool InodeTable::initialize(const block_index_t startBlock, const inode_index_t numBlocks,
                            BlockManager* blockManager)
{
    block_t tempBlock;
    for (inode_index_t i = 0; i < numBlocks; i++)
    {
        for (inode_index_t j = 0; j < TABLE_ENTRIES_PER_BLOCK; j++)
        {
            tempBlock.inodeTable.inodeNumbers[j] = INODE_NULL_VALUE;
        }
        if (!blockManager->writeBlock(startBlock + i, tempBlock.data))
        {
            std::cerr << "Could not write inode table block" << std::endl;
            return false;
        }
    }
    return true;
}

inode_index_t InodeTable::getFreeInodeNumber()
{
    block_t tempBlock;
    for (inode_index_t i = 0; i < size; i++)
    {
        if (i % TABLE_ENTRIES_PER_BLOCK == 0)
        {
            if (!blockManager->readBlock(startBlock + i / TABLE_ENTRIES_PER_BLOCK, tempBlock.data))
            {
                std::cerr << "Could not read inode table block" << std::endl;
                return INODE_NULL_VALUE;
            }
        }
        if (tempBlock.inodeTable.inodeNumbers[i % TABLE_ENTRIES_PER_BLOCK] == INODE_NULL_VALUE)
        {
            return i;
        }
    }
    return INODE_NULL_VALUE;
}

bool InodeTable::setInodeLocation(inode_index_t inodeNumber, inode_index_t location)
{
    if (inodeNumber >= size)
    {
        std::cerr << "Inode number out of bounds" << std::endl;
        return false;
    }
    inode_index_t blockNum = inodeNumber / TABLE_ENTRIES_PER_BLOCK;
    inode_index_t entryNum = inodeNumber % TABLE_ENTRIES_PER_BLOCK;
    block_t tempBlock;
    if (!blockManager->readBlock(startBlock + blockNum, tempBlock.data))
    {
        std::cerr << "Could not read inode table block" << std::endl;
        return false;
    }
    tempBlock.inodeTable.inodeNumbers[entryNum] = location;
    if (!blockManager->writeBlock(startBlock + blockNum, tempBlock.data))
    {
        std::cerr << "Could not write inode table block" << std::endl;
        return false;
    }
    return true;
}

inode_index_t InodeTable::getInodeLocation(inode_index_t inodeNumber)
{
    if (inodeNumber >= size)
    {
        std::cerr << "Inode number out of bounds" << std::endl;
        return INODE_NULL_VALUE;
    }
    inode_index_t blockNum = inodeNumber / TABLE_ENTRIES_PER_BLOCK;
    inode_index_t entryNum = inodeNumber % TABLE_ENTRIES_PER_BLOCK;
    block_t tempBlock;
    if (!blockManager->readBlock(startBlock + blockNum, tempBlock.data))
    {
        std::cerr << "Could not read inode table block" << std::endl;
        return INODE_NULL_VALUE;
    }
    return tempBlock.inodeTable.inodeNumbers[entryNum];
}

bool InodeTable::writeInode(inode_index_t inodeLocation, inode_t& inode)
{
    block_index_t inodeBlock = inodeRegionStart + inodeLocation / INODES_PER_BLOCK;
    block_t tempBlock;
    if (!blockManager->readBlock(inodeBlock, tempBlock.data))
    {
        std::cerr << "Could not read inode block" << std::endl;
        return false;
    }
    tempBlock.inodeBlock.inodes[inodeLocation % INODES_PER_BLOCK] = inode;
    if (!blockManager->writeBlock(inodeBlock, tempBlock.data))
    {
        std::cerr << "Could not write inode block" << std::endl;
        return false;
    }
    // std::cout << "[write inode] inodeLocation: " << inodeLocation << std::endl;
    // std::cout << "\tpermissions: " << inode.permissions << std::endl;
    return true;
}

bool InodeTable::readInode(inode_index_t inodeLocation, inode_t& inode)
{
    block_index_t inodeBlock = inodeRegionStart + inodeLocation / INODES_PER_BLOCK;
    block_t tempBlock;
    if (!blockManager->readBlock(inodeBlock, tempBlock.data))
    {
        std::cerr << "Could not read inode block" << std::endl;
        return false;
    }
    inode = tempBlock.inodeBlock.inodes[inodeLocation % INODES_PER_BLOCK];
    // std::cout << "[read inode] inodeLocation: " << inodeLocation << std::endl;
    return true;
}