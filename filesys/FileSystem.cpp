//
// Created by ameya on 2/20/2025.
//

#include "FileSystem.h"
#include <string.h>

static const block_index_t LOG_AREA_SIZE = 64; // Reserve 64 blocks for the log area, need to adjust this later

FileSystem::FileSystem(BlockManager* blockManager): blockManager(blockManager), inodeBitmap(nullptr),
                                                    blockBitmap(nullptr)
{
    this->superBlock = &superBlockWrapper.superBlock;
    if (!blockManager->readBlock(0, superBlockWrapper.data))
    {
        std::cerr << "Could not read superblock" << std::endl;
        throw std::runtime_error("Could not read superblock");
    }

    if (superBlock->magic != MAGIC_NUMBER)
    {
        std::cout << "Creating new filesystem" << std::endl;
        createFilesystem();
    }
    else
    {
        std::cout << "Existing filesystem detected" << std::endl;
    }
    loadFilesystem();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void FileSystem::createFilesystem()
{
    superBlock->magic = MAGIC_NUMBER;
    superBlock->totalBlockCount = blockManager->getNumBlocks() - 1;
    superBlock->inodeCount = 4 * (superBlock->totalBlockCount / 16);
    superBlock->inodeBitmapSize = ((superBlock->inodeCount + 7) / 8 + BlockManager::BLOCK_SIZE - 1)
        / BlockManager::BLOCK_SIZE;
    superBlock->inodeRegionSize = (superBlock->inodeCount * sizeof(inode_t) +
            BlockManager::BLOCK_SIZE - 1) /
        BlockManager::BLOCK_SIZE;
    superBlock->inodeTableSize = (superBlock->inodeCount * sizeof(inode_index_t) +
            BlockManager::BLOCK_SIZE - 1) /
        BlockManager::BLOCK_SIZE;

    // Calculate remaining blocks for data and log area.
    const block_index_t remainingBlocks = superBlock->totalBlockCount - superBlock->inodeBitmapSize -
        superBlock->inodeRegionSize - superBlock->inodeTableSize;
    superBlock->dataBlockBitmapSize = ((remainingBlocks + 7) / 8 + BlockManager::BLOCK_SIZE - 1) /
        BlockManager::BLOCK_SIZE;

    // Reserve LOG_AREA_SIZE blocks for logging.
    if (remainingBlocks < superBlock->dataBlockBitmapSize + LOG_AREA_SIZE)
    {
        std::cerr << "Not enough blocks remaining for data and log areas." << std::endl;
        throw std::runtime_error("Insufficient space for log area");
    }
    superBlock->dataBlockCount = remainingBlocks - superBlock->dataBlockBitmapSize - LOG_AREA_SIZE;
    superBlock->freeDataBlockCount = superBlock->dataBlockCount;
    superBlock->freeInodeCount = superBlock->inodeCount;
    superBlock->size = superBlock->dataBlockCount * BlockManager::BLOCK_SIZE;

    // Layout: bitmaps and tables are laid out consecutively.
    superBlock->inodeBitmap = 1;
    superBlock->inodeTable = superBlock->inodeBitmap + superBlock->inodeBitmapSize;
    superBlock->dataBlockBitmap = superBlock->inodeTable + superBlock->inodeTableSize;
    superBlock->inodeRegionStart = superBlock->dataBlockBitmap + superBlock->dataBlockBitmapSize;
    superBlock->dataBlockRegionStart = superBlock->inodeRegionStart + superBlock->inodeRegionSize;

    // Reserve the log area at the end of the disk.
    superBlock->logAreaStart = superBlock->dataBlockRegionStart + superBlock->dataBlockCount;
    superBlock->logAreaSize = LOG_AREA_SIZE;

    // Zero out the bitmaps.
    constexpr block_t zeroBlock{};
    for (block_index_t i = 0; i < superBlock->dataBlockBitmapSize; i++)
    {
        if (!blockManager->writeBlock(superBlock->dataBlockBitmap + i, zeroBlock.data))
        {
            std::cerr << "Could not write block bitmap" << std::endl;
            throw std::runtime_error("Could not write block bitmap");
        }
    }

    for (block_index_t i = 0; i < superBlock->inodeBitmapSize; i++)
    {
        if (!blockManager->writeBlock(superBlock->inodeBitmap + i, zeroBlock.data))
        {
            std::cerr << "Could not write inode bitmap" << std::endl;
            throw std::runtime_error("Could not write inode bitmap");
        }
    }

    InodeTable::initialize(superBlock->inodeTable, superBlock->inodeTableSize, blockManager);

    if (!blockManager->writeBlock(0, superBlockWrapper.data))
    {
        std::cerr << "Could not write superblock" << std::endl;
        throw std::runtime_error("Could not write superblock");
    }
}

void FileSystem::loadFilesystem()
{
    std::cout << "Loading filesystem" << std::endl;
    std::cout << "Size: " << superBlock->size << std::endl;
    inodeBitmap = new BitmapManager(superBlock->inodeBitmap, superBlock->inodeBitmapSize, superBlock->inodeCount,
                                    blockManager);
    blockBitmap = new BitmapManager(superBlock->dataBlockBitmap, superBlock->dataBlockBitmapSize,
                                    superBlock->dataBlockCount, blockManager);
    inodeTable = new InodeTable(superBlock->inodeTable, superBlock->inodeTableSize, superBlock->inodeCount,
                                blockManager);

    // Initialize LogManager using the log area from the superblock.
    logManager = new LogManager(blockManager, blockBitmap, inodeTable, superBlock->logAreaStart, superBlock->logAreaSize, superBlock->systemStateSeqNum);

}

bool FileSystem::readInode(inode_index_t inodeLocation, inode_t& inode)
{
    block_index_t inodeBlock = superBlock->inodeRegionStart + inodeLocation / INODES_PER_BLOCK;
    block_t tempBlock;
    if (!blockManager->readBlock(inodeBlock, tempBlock.data))
    {
        std::cerr << "Could not read inode block" << std::endl;
        return false;
    }
    inode = tempBlock.inodeBlock.inodes[inodeLocation % INODES_PER_BLOCK];
    return true;
}

bool FileSystem::writeInode(inode_index_t inodeLocation, inode_t& inode)
{
    block_index_t inodeBlock = superBlock->inodeRegionStart + inodeLocation / INODES_PER_BLOCK;
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
    return true;
}

bool FileSystem::addDirectoryEntry(inode_index_t baseDirectory, const char* fileName, inode_index_t fileNum)
{
    // TODO: Implement copy on write and logging (and support for indirect blocks)
    inode_index_t baseDirLocation = inodeTable->getInodeLocation(baseDirectory);
    if (baseDirLocation == InodeTable::NULL_VALUE)
    {
        std::cerr << "Could not get inode location" << std::endl;
        return false;
    }
    inode_t baseDirInode;
    if (!readInode(baseDirLocation, baseDirInode))
    {
        std::cerr << "Could not read inode" << std::endl;
        return false;
    }
    block_index_t blockNum = baseDirInode.numFiles / DIRECTORY_ENTRIES_PER_BLOCK;
    if (blockNum >= NUM_DIRECT_BLOCKS)
    {
        std::cerr << "Directory too large" << std::endl;
        return false;
    }
    if (blockNum >= baseDirInode.blockCount)
    {
        block_index_t newBlock = blockBitmap->findNextFree();
        if (!blockBitmap->setAllocated(newBlock))
        {
            std::cerr << "Could not set block bitmap" << std::endl;
            return false;
        }
        baseDirInode.directBlocks[blockNum] = newBlock;
        baseDirInode.blockCount++;
    }
    block_t tempBlock;
    if (!blockManager->readBlock(baseDirInode.directBlocks[blockNum], tempBlock.data))
    {
        std::cerr << "Could not read block" << std::endl;
        return false;
    }
    uint16_t offset = baseDirInode.numFiles % DIRECTORY_ENTRIES_PER_BLOCK;
    tempBlock.directoryBlock.entries[offset].inodeNumber = fileNum;
    strncpy(tempBlock.directoryBlock.entries[offset].name, fileName, MAX_FILE_NAME_LENGTH);
    tempBlock.directoryBlock.entries[offset].name[MAX_FILE_NAME_LENGTH] = '\0';
    if (!blockManager->writeBlock(baseDirInode.directBlocks[blockNum], tempBlock.data))
    {
        std::cerr << "Could not write block" << std::endl;
        return false;
    }
    baseDirInode.numFiles++;
    if (!writeInode(baseDirLocation, baseDirInode))
    {
        std::cerr << "Could not write inode" << std::endl;
        return false;
    }
    return true;
}

inode_index_t FileSystem::createDirectory(inode_index_t baseDirectory, const char* name)
{
    const inode_index_t inodeLocation = inodeBitmap->findNextFree();
    if (!inodeBitmap->setAllocated(inodeLocation))
    {
        std::cerr << "Could not set inode bit" << std::endl;
        return InodeTable::NULL_VALUE;
    }

    inode_t inode{};
    inode.size = 0;
    inode.blockCount = 0;
    inode.uid = 0;
    inode.gid = 0;
    inode.numFiles = 0;
    inode.permissions = 1 << 9;
    for (block_index_t i = 0; i < NUM_DIRECT_BLOCKS; i++)
    {
        inode.directBlocks[i] = InodeTable::NULL_VALUE;
    }

    for (block_index_t i = 0; i < NUM_INDIRECT_BLOCKS; i++)
    {
        inode.indirectBlocks[i] = InodeTable::NULL_VALUE;
    }

    for (block_index_t i = 0; i < NUM_DOUBLE_INDIRECT_BLOCKS; i++)
    {
        inode.doubleIndirectBlocks[i] = InodeTable::NULL_VALUE;
    }

    writeInode(inodeLocation, inode);

    const inode_index_t inodeNum = inodeTable->getFreeInodeNumber();
    if (inodeNum == InodeTable::NULL_VALUE)
    {
        std::cerr << "Could not get free inode number" << std::endl;
        return InodeTable::NULL_VALUE;
    }
    if (!inodeTable->setInodeLocation(inodeNum, inodeLocation))
    {
        std::cerr << "Could not set inode location" << std::endl;
        return InodeTable::NULL_VALUE;
    }

    if (baseDirectory == InodeTable::NULL_VALUE)
    {
        // creating root directory
        return inodeNum;
    }

    if (!addDirectoryEntry(baseDirectory, name, inodeNum))
    {
        std::cerr << "Could not add directory entry" << std::endl;
        return InodeTable::NULL_VALUE;
    }
    return inodeNum;
}
