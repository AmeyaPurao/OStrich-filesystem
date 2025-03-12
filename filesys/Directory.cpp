//
// Created by ameya on 3/11/2025.
//

#include "Directory.h"

#include <cstring>

Directory::Directory(InodeTable* inodeTable, BitmapManager* inodeBitmap, BitmapManager* blockBitmap,
                     BlockManager* blockManager, const uint16_t permissions): File(
    inodeTable, inodeBitmap, blockBitmap, blockManager, permissions)
{
}

Directory::Directory(inode_index_t inodeNum, InodeTable* inodeTable, BitmapManager* inodeBitmap,
                     BitmapManager* blockBitmap, BlockManager* blockManager): File(
    inodeNum, inodeTable, inodeBitmap, blockBitmap, blockManager)
{
}

inode_index_t Directory::getDirectoryEntry(const char* fileName) const
{
    block_t block;
    for (inode_index_t i = 0; i < inode.blockCount; i++)
    {
        read_block_data(i, block.data);
        for (auto [inodeNumber, name] : block.directoryBlock.entries)
        {
            if (strcmp(name, fileName) == 0)
            {
                // std::cout << "Returning inode number: " << inodeNumber << " for file: " << fileName << std::endl;
                return inodeNumber;
            }
        }
    }
    // std::cout << "File: " << fileName << " not found" << std::endl;
    return INODE_NULL_VALUE;
}

bool Directory::addDirectoryEntry(const char* fileName, inode_index_t fileNum)
{
    const uint16_t offset = inode.numFiles % DIRECTORY_ENTRIES_PER_BLOCK;
    block_t block{};
    if (offset == 0)
    {
        strncpy(block.directoryBlock.entries[0].name, fileName, MAX_FILE_NAME_LENGTH);
        block.directoryBlock.entries[0].name[MAX_FILE_NAME_LENGTH] = '\0';
        // std::cout << "verify strcpy: " << block.directoryBlock.entries[0].name << std::endl;
        block.directoryBlock.entries[0].inodeNumber = fileNum;
        inode.numFiles++;
        write_new_block_data(block.data);
        // std::cout << "Added: " << fileName << std::endl;
        // std::cout << "New file count: " << inode.numFiles << std::endl;
        return inodeTable->writeInode(inodeLocation, inode);
    }
    read_block_data(inode.blockCount - 1, block.data);
    strncpy(block.directoryBlock.entries[offset].name, fileName, MAX_FILE_NAME_LENGTH);
    block.directoryBlock.entries[offset].name[MAX_FILE_NAME_LENGTH] = '\0';
    block.directoryBlock.entries[offset].inodeNumber = fileNum;
    inode.numFiles++;
    write_block_data(inode.blockCount - 1, block.data);
    // std::cout << "Added: " << fileName << std::endl;
    // std::cout << "New file count: " << inode.numFiles << std::endl;
    return inodeTable->writeInode(inodeLocation, inode);
}

bool Directory::removeDirectoryEntry(const char* fileName)
{
    return false;
}

bool Directory::modifyDirectoryEntry(const char* fileName, inode_index_t fileNum)
{
    return false;
}

std::vector<char*> Directory::listDirectoryEntries() const
{
    std::vector<char*> entries(inode.numFiles);
    block_t block;
    inode_index_t cur = 0;
    for (inode_index_t i = 0; i < inode.blockCount; i++)
    {
        read_block_data(i, block.data);
        for (auto [inodeNumber, name] : block.directoryBlock.entries)
        {
            if (cur == inode.numFiles)
            {
                return entries;
            }
            entries[cur] = new char[MAX_FILE_NAME_LENGTH + 1];
            strncpy(entries[cur], name, MAX_FILE_NAME_LENGTH);
            entries[cur][MAX_FILE_NAME_LENGTH] = '\0';
            cur++;
        }
    }
    return entries;
}

Directory* Directory::createDirectory(const char* name)
{
    auto* newDir = new Directory(inodeTable, inodeBitmap, blockBitmap, blockManager);
    if(!addDirectoryEntry(name, newDir->getInodeNumber()))
    {
        return nullptr;
    }
    return newDir;
}

File* Directory::createFile(const char* name)
{
    auto* newFile = new File(inodeTable, inodeBitmap, blockBitmap, blockManager);
    if(!addDirectoryEntry(name, newFile->getInodeNumber()))
    {
        return nullptr;
    }
    return newFile;
}

File* Directory::getFile(const char* name) const
{
    const inode_index_t inodeNumber = getDirectoryEntry(name);
    if (inodeNumber == INODE_NULL_VALUE)
    {
        return nullptr;
    }
    File* file = new File(inodeNumber, inodeTable, inodeBitmap, blockBitmap, blockManager);
    if(file->isDirectory())
    {
        return convertFile(file);
    }
    return file;
}

Directory::Directory(const File* file) : File(file)
{
}

Directory *Directory::convertFile(File *file)
{
    if(!file->isDirectory()) return nullptr;
    auto* dir = new Directory(file);
    delete file;
    return dir;
}
