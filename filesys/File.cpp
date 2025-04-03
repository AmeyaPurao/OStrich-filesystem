//
// Created by ameya on 2/26/2025.
//

#include "File.h"

#include <string.h>

#include "BitmapManager.h"
#include "InodeTable.h"
#include "LogRecord.h"

File::File(InodeTable* inodeTable, BitmapManager* inodeBitmap, BitmapManager* blockBitmap, BlockManager* blockManager,
           LogManager* logManager, const uint16_t permissions)
    : inodeTable(inodeTable), inodeBitmap(inodeBitmap), blockBitmap(blockBitmap), blockManager(blockManager), logManager(logManager)
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
    LogRecordPayload payload{};
    payload.inodeAdd.inodeIndex = inodeNumber;
    payload.inodeAdd.inodeLocation = inodeLocation;
    logManager->logOperation(LogOpType::LOG_OP_INODE_ADD, &payload);
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
    cout << "Writing new block data to block " << newBlock << endl;

    // Perform copy-on-write update on the inode:
    inode_index_t newInodeLocation = inodeBitmap->findNextFree();
    if (newInodeLocation == NULL_INDEX)
    {
        std::cerr << "Failed to allocate new inode for copy-on-write update in write_new_block_data" << std::endl;
        return false;
    }
    if (!inodeBitmap->setAllocated(newInodeLocation))
    {
        std::cerr << "Failed to mark new inode as allocated in write_new_block_data" << std::endl;
        return false;
    }
    inode_t newFileInode = inode;
    if (!inodeTable->writeInode(newInodeLocation, newFileInode))
    {
        std::cerr << "Failed to write updated inode to disk in write_new_block_data" << std::endl;
        return false;
    }
    LogRecordPayload payload{};
    payload.inodeUpdate.inodeIndex = getInodeNumber();
    payload.inodeUpdate.inodeLocation = newInodeLocation;
    if (!logManager->logOperation(LogOpType::LOG_OP_INODE_UPDATE, &payload))
    {
        std::cerr << "Failed to log inode update in write_new_block_data" << std::endl;
        return false;
    }
    cout << "Updating inode table for inode " << getInodeNumber() << ": replacing location " << inodeLocation
         << " with new location " << newInodeLocation << std::endl;
    if (!inodeTable->setInodeLocation(getInodeNumber(), newInodeLocation))
    {
        std::cerr << "Failed to update inode table for inode in write_new_block_data" << std::endl;
        return false;
    }
    return true;
}

bool File::isDirectory() const
{
    return (inode.permissions & DIRECTORY_MASK) != 0;
}

bool File::write_at(const uint64_t offset, const uint8_t* data, const uint64_t size) {
    if (offset > inode.size) {
        std::cerr << "Offset out of bounds" << std::endl;
        return false;
    }

    uint64_t cur = offset;
    block_t block;
    while (cur < offset + size) {
        block_index_t blockNum = cur / BlockManager::BLOCK_SIZE;
        uint64_t blockOffset = cur % BlockManager::BLOCK_SIZE;
        uint64_t toWrite = std::min(BlockManager::BLOCK_SIZE - blockOffset, size - (cur - offset));

        // Case 1: Updating an already allocated block (copy-on-write update)
        if (blockNum < inode.blockCount) {
            // Read the existing block into our temporary block.
            if (!read_block_data(blockNum, block.data)) {
                std::cerr << "Failed to read block " << blockNum << std::endl;
                return false;
            }
            // Allocate a new block for the updated copy.
            block_index_t newBlock = blockBitmap->findNextFree();
            if (newBlock == BLOCK_NULL_VALUE) {
                std::cerr << "No free block available for copy-on-write update" << std::endl;
                return false;
            }
            if (!blockBitmap->setAllocated(newBlock)) {
                std::cerr << "Failed to mark new block as allocated" << std::endl;
                return false;
            }
            // Copy the old block contents (already in block.data) and apply modifications.
            memcpy(block.data + blockOffset, data + (cur - offset), toWrite);
            // Write the modified block to the newly allocated block.
            if (!blockManager->writeBlock(newBlock, block.data)) {
                std::cerr << "Failed to write new block for updated data" << std::endl;
                return false;
            }
            // Update the inode pointer for this block.
            inode.directBlocks[blockNum] = newBlock;
        }
            // Case 2: Writing to a new block that has not been allocated yet.
        else {
            // Zero out our temporary block.
            memset(block.data, 0, BlockManager::BLOCK_SIZE);
            // Copy the new data into the block at the correct offset.
            memcpy(block.data + blockOffset, data + (cur - offset), toWrite);
            // Allocate and write this block using our helper.
            if (!write_new_block_data(block.data)) {
                std::cerr << "Failed to allocate and write new block for file extension" << std::endl;
                return false;
            }
        }
        cur += toWrite;
    }

    // Update the file size if necessary.
    inode.size = std::max(offset + size, inode.size);

    // --- Perform copy-on-write update on the file's own inode ---
    inode_index_t newInodeLocation = inodeBitmap->findNextFree();
    if (newInodeLocation == NULL_INDEX) {
        std::cerr << "Failed to allocate new inode for copy-on-write update in write_at" << std::endl;
        return false;
    }
    if (!inodeBitmap->setAllocated(newInodeLocation)) {
        std::cerr << "Failed to mark new inode as allocated in write_at" << std::endl;
        return false;
    }
    // Create a new file inode by copying the updated inode.
    inode_t newFileInode = inode;
    // Write the new inode to disk.
    if (!inodeTable->writeInode(newInodeLocation, newFileInode)) {
        std::cerr << "Failed to write updated file inode to disk in write_at" << std::endl;
        return false;
    }
    // Log the file inode update.
    {
        LogRecordPayload payload{};
        payload.inodeUpdate.inodeIndex = getInodeNumber();
        payload.inodeUpdate.inodeLocation = newInodeLocation;
        if (!logManager->logOperation(LogOpType::LOG_OP_INODE_UPDATE, &payload)) {
            std::cerr << "Failed to log inode update for file write in write_at" << std::endl;
            return false;
        }
    }
    // Update the inode table to point to the new file inode location.
    if (!inodeTable->setInodeLocation(getInodeNumber(), newInodeLocation)) {
        std::cerr << "Failed to update inode table for file inode in write_at" << std::endl;
        return false;
    }
    return true;
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
