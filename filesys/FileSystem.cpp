//
// Created by ameya on 2/20/2025.
//

#include "FileSystem.h"

FileSystem::FileSystem(BlockManager* blockManager)
{
    this->blockManager = blockManager;
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

void FileSystem::createFilesystem()
{
    superBlock->magic = MAGIC_NUMBER;
    superBlock->totalBlockCount = blockManager->getNumBlocks() - 1;
    superBlock->inodeCount = 4 * (superBlock->totalBlockCount / 16);
    superBlock->inodeBitmapSize = ((superBlock->inodeCount + 7) / 8 + BlockManager::BLOCK_SIZE - 1)
        /
        BlockManager::BLOCK_SIZE;
    superBlock->inodeRegionSize = (superBlock->inodeCount * sizeof(inode_t) +
            BlockManager::BLOCK_SIZE - 1) /
        BlockManager::BLOCK_SIZE;
    superBlock->inodeTableSize = (superBlock->inodeCount * sizeof(uint32_t) +
            BlockManager::BLOCK_SIZE - 1) /
        BlockManager::BLOCK_SIZE;

    const uint64_t remainingBlocks = superBlock->totalBlockCount - superBlock->inodeBitmapSize -
        superBlock->inodeRegionSize - superBlock->inodeTableSize;
    superBlock->dataBlockBitmapSize = ((remainingBlocks + 7) / 8 + BlockManager::BLOCK_SIZE - 1) /
        BlockManager::BLOCK_SIZE;
    superBlock->dataBlockCount = remainingBlocks - superBlock->dataBlockBitmapSize;
    superBlock->freeDataBlockCount = superBlock->dataBlockCount;
    superBlock->freeInodeCount = superBlock->inodeCount;
    superBlock->size = superBlock->dataBlockCount * BlockManager::BLOCK_SIZE;

    superBlock->inodeBitmap = 1;
    superBlock->inodeTable = superBlock->inodeBitmap + superBlock->inodeBitmapSize;
    superBlock->dataBlockBitmap = superBlock->inodeTable + superBlock->inodeTableSize;
    superBlock->inodeRegionStart = superBlock->dataBlockBitmap + superBlock->dataBlockBitmapSize;
    superBlock->dataBlockRegionStart = superBlock->inodeRegionStart + superBlock->inodeRegionSize;

    // Zero out the bitmaps
    constexpr block_t zeroBlock{};
    for (uint32_t i = 0; i < superBlock->dataBlockBitmapSize; i++)
    {
        if (!blockManager->writeBlock(superBlock->dataBlockBitmap + i, zeroBlock.data))
        {
            std::cerr << "Could not write block bitmap" << std::endl;
            throw std::runtime_error("Could not write block bitmap");
        }
    }

    for (uint32_t i = 0; i < superBlock->inodeBitmapSize; i++)
    {
        if (!blockManager->writeBlock(superBlock->inodeBitmap + i, zeroBlock.data))
        {
            std::cerr << "Could not write inode bitmap" << std::endl;
            throw std::runtime_error("Could not write inode bitmap");
        }
    }

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
}

uint32_t FileSystem::findNextFreeInode()
{

}
