//
// Created by ameya on 2/26/2025.
//

#include "File.h"

#include <string.h>

#include "BitmapManager.h"
#include "InodeTable.h"

File::File(InodeTable* inodeTable, BitmapManager* inodeBitmap, BitmapManager* blockBitmap, BlockManager* blockManager,
           const uint16_t permissions)
    : inodeTable(inodeTable), inodeBitmap(inodeBitmap), blockBitmap(blockBitmap), blockManager(blockManager)
{
    inodeLocation = inodeBitmap->findNextFree();
    if (!inodeBitmap->setAllocated(inodeLocation))
    {
        std::cerr << "Could not set inode bit" << std::endl;
        throw std::runtime_error("Could not set inode bit");
    }
    inode.size = 0;
    inode.blockCount = 0;
    inode.uid = 0;
    inode.gid = 0;
    inode.numFiles = 0;
    inode.permissions = permissions;

    for (block_index_t i = 0; i < NUM_DIRECT_BLOCKS; i++)
    {
        inode.directBlocks[i] = INODE_NULL_VALUE;
    }

    for (block_index_t i = 0; i < NUM_INDIRECT_BLOCKS; i++)
    {
        inode.indirectBlocks[i] = INODE_NULL_VALUE;
    }

    for (block_index_t i = 0; i < NUM_DOUBLE_INDIRECT_BLOCKS; i++)
    {
        inode.doubleIndirectBlocks[i] = INODE_NULL_VALUE;
    }

    if (!inodeTable->writeInode(inodeLocation, inode))
    {
        std::cerr << "Could not write inode" << std::endl;
        throw std::runtime_error("Could not write inode");
    }

    inodeNumber = inodeTable->getFreeInodeNumber();
    if (inodeNumber == INODE_NULL_VALUE)
    {
        std::cerr << "Could not get free inode number" << std::endl;
        throw std::runtime_error("Could not get free inode number");
    }
    if (!inodeTable->setInodeLocation(inodeNumber, inodeLocation))
    {
        std::cerr << "Could not set inode location" << std::endl;
        throw std::runtime_error("Could not set inode location");
    }

    // std::cout << "Creating file with permissions: " << permissions << " with number " << inodeNumber << " at " <<
    // inodeLocation << std::endl;
}

File::File(inode_index_t inodeNumber, InodeTable* inodeTable, BitmapManager* inodeBitmap, BitmapManager* blockBitmap,
           BlockManager* blockManager): inodeTable(inodeTable), inodeBitmap(inodeBitmap), blockBitmap(blockBitmap),
                                        blockManager(blockManager), inodeNumber(inodeNumber)
{
    inodeLocation = inodeTable->getInodeLocation(inodeNumber);
    if (inodeLocation == INODE_NULL_VALUE)
    {
        throw std::runtime_error("Inode not found");
    }
    if (!inodeTable->readInode(inodeLocation, inode))
    {
        throw std::runtime_error("Could not read inode");
    }
}

File::File(const File* file)
{
    inodeTable = file->inodeTable;
    inodeBitmap = file->inodeBitmap;
    blockBitmap = file->blockBitmap;
    blockManager = file->blockManager;
    inodeLocation = file->inodeLocation;
    inodeNumber = file->inodeNumber;
    inode = file->inode;
}

inode_index_t File::getInodeNumber() const
{
    return inodeNumber;
}

block_index_t File::getBlockLocation(const block_index_t blockNum) const
{
    if (blockNum >= inode.blockCount)
    {
        return BLOCK_NULL_VALUE;
    }
    if (blockNum < NUM_DIRECT_BLOCKS)
    {
        return inode.directBlocks[blockNum];
    }
    throw std::runtime_error("Indirect blocks not implemented");
}

bool File::write_block_data(const block_index_t blockNum, const uint8_t* data)
{
    return blockManager->writeBlock(getBlockLocation(blockNum), data);
}

bool File::write_new_block_data(const uint8_t* data)
{
    block_index_t newBlock = blockBitmap->findNextFree();
    if (newBlock == BLOCK_NULL_VALUE)
    {
        return false;
    }
    if (!blockBitmap->setAllocated(newBlock))
    {
        return false;
    }
    if (inode.blockCount < NUM_DIRECT_BLOCKS)
    {
        inode.directBlocks[inode.blockCount] = newBlock;
    }
    else
    {
        throw std::runtime_error("Indirect blocks not implemented");
    }
    inode.blockCount++;
    if (!blockManager->writeBlock(newBlock, data))
    {
        return false;
    }
    return inodeTable->writeInode(inodeLocation, inode);
}

bool File::isDirectory() const
{
    return (inode.permissions & DIRECTORY_MASK) != 0;
}

bool File::write_at(const uint64_t offset, const uint8_t* data, const uint64_t size)
{
    if (offset > inode.size)
    {
        std::cerr << "Offset out of bounds" << std::endl;
        return false;
    }

    uint64_t cur = offset;
    block_t block{};
    while (cur < offset + size)
    {
        block_index_t blockNum = cur / BlockManager::BLOCK_SIZE;
        uint64_t blockOffset = cur % BlockManager::BLOCK_SIZE;
        uint64_t toWrite = std::min(BlockManager::BLOCK_SIZE - blockOffset, size - (cur - offset));
        if (blockNum >= inode.blockCount)
        {
            memcpy(block.data + blockOffset, data + cur - offset, toWrite);
            if (toWrite < BlockManager::BLOCK_SIZE)
            {
                memset(block.data + blockOffset + toWrite, 0, BlockManager::BLOCK_SIZE - toWrite);
            }
            if (!write_new_block_data(block.data))
            {
                return false;
            }
            continue;
        }
        if (!read_block_data(blockNum, block.data))
        {
            return false;
        }
        memcpy(block.data + blockOffset, data + cur - offset, toWrite);
        if (!write_block_data(blockNum, block.data))
        {
            return false;
        }
        cur += toWrite;
    }
    inode.size = std::max(offset + size, inode.size);
    return inodeTable->writeInode(inodeLocation, inode);
}

bool File::read_at(const uint64_t offset, uint8_t* data, const uint64_t size) const
{
    if (offset + size > inode.size)
    {
        std::cerr << "Offset out of bounds" << std::endl;
        return false;
    }
    uint64_t cur = offset;
    block_t block{};
    while (cur < offset + size)
    {
        block_index_t blockNum = cur / BlockManager::BLOCK_SIZE;
        uint64_t blockOffset = cur % BlockManager::BLOCK_SIZE;
        uint64_t toRead = std::min(BlockManager::BLOCK_SIZE - blockOffset, size - (cur - offset));
        if (!read_block_data(blockNum, block.data))
        {
            return false;
        }
        memcpy(data + cur - offset, block.data + blockOffset, toRead);
        cur += toRead;
    }
    return true;
}

uint64_t File::getSize() const
{
    return inode.size;
}

bool File::read_block_data(const block_index_t blockNum, uint8_t* data) const
{
    return blockManager->readBlock(getBlockLocation(blockNum), data);
}
