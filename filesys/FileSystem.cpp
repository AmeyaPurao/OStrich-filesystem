//
// Created by ameya on 2/20/2025.
//

#include "FileSystem.h"

#include <string.h>

#include "Directory.h"

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
        // std::cout << "Creating new filesystem; found magic: " << superBlock->magic << " | expected: " << MAGIC_NUMBER <<
            // std::endl;
        createFilesystem();
    }
    else
    {
        // std::cout << "Existing filesystem detected" << std::endl;
    }
    loadFilesystem();
}

Directory* FileSystem::getRootDirectory() const
{
    return new Directory(0, inodeTable, inodeBitmap, blockBitmap, blockManager);
}

// ReSharper disable once CppMemberFunctionMayBeConst
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
    superBlock->inodeTableSize = (superBlock->inodeCount * sizeof(inode_index_t) +
            BlockManager::BLOCK_SIZE - 1) /
        BlockManager::BLOCK_SIZE;

    const block_index_t remainingBlocks = superBlock->totalBlockCount - superBlock->inodeBitmapSize -
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
    // std::cout << "Loading filesystem" << std::endl;
    // std::cout << "Size: " << superBlock->size << std::endl;
    // std::cout << "Inode region start: " << superBlock->inodeRegionStart << std::endl;
    // std::cout << "Data block region start: " << superBlock->dataBlockRegionStart << std::endl;
    inodeBitmap = new BitmapManager(superBlock->inodeBitmap, superBlock->inodeBitmapSize, superBlock->inodeCount,
                                    blockManager);
    blockBitmap = new BitmapManager(superBlock->dataBlockBitmap, superBlock->dataBlockBitmapSize,
                                    superBlock->dataBlockCount, blockManager, superBlock->dataBlockRegionStart);
    inodeTable = new InodeTable(superBlock->inodeTable, superBlock->inodeTableSize, superBlock->inodeCount,
                                superBlock->inodeRegionStart,
                                blockManager);
    if (inodeTable->getInodeLocation(0) == INODE_NULL_VALUE)
    {
        delete createRootInode();
    }
}
Directory* FileSystem::createRootInode()
{
    // std::cout << "Creating root inode" << std::endl;
    return new Directory(inodeTable, inodeBitmap, blockBitmap, blockManager, DIRECTORY_MASK);
}

// Directory* FileSystem::createDirectory()
// {
//     return new Directory(inodeTable, inodeBitmap, blockBitmap, blockManager);
// }
//
// File* FileSystem::createFile()
// {
//     return new File(inodeTable, inodeBitmap, blockBitmap, blockManager);
// }
//
// File* FileSystem::loadFile(const inode_index_t inodeNumber)
// {
//     inode_t tmp;
//     // std::cout << "Loading file from inode: " << inodeNumber << std::endl;
//     const inode_index_t inodeLocation = inodeTable->getInodeLocation(inodeNumber);
//     if(inodeLocation == INODE_NULL_VALUE)
//     {
//         // std::cout << "inode not found" << std::endl;
//         return nullptr;
//     }
//     if (!inodeTable->readInode(inodeLocation, tmp))
//     {
//         // std::cout << "Could not read inode" << std::endl;
//         return nullptr;
//     }
//     if ((tmp.permissions & DIRECTORY_MASK) != 0)
//     {
//         // std::cout << "inode is directory" << std::endl;
//         return new Directory(inodeNumber, inodeTable, inodeBitmap, blockBitmap, blockManager);
//     }
//     // std::cout << "inode is file" << std::endl;
//     // std::cout << "permissions: " << tmp.permissions << std::endl;
//     return new File(inodeNumber, inodeTable, inodeBitmap, blockBitmap, blockManager);
// }
