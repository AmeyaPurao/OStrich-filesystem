//
// Created by ameya on 3/11/2025.
//

#include "Directory.h"

#include <cstring>

Directory::Directory(InodeTable* inodeTable, BitmapManager* inodeBitmap, BitmapManager* blockBitmap,
                     BlockManager* blockManager, LogManager* logManager, const uint16_t permissions): File(
    inodeTable, inodeBitmap, blockBitmap, blockManager, logManager, permissions)
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
        uint8_t offset = 0;
        for (auto [inodeNumber, name] : block.directoryBlock.entries)
        {
            if (strcmp(name, fileName) == 0)
            {
                // std::cout << "Returning inode number: " << inodeNumber << " for file: " << fileName << std::endl;
                return inodeNumber;
            }
            offset++;
            if (offset + i * DIRECTORY_ENTRIES_PER_BLOCK == inode.numFiles)
            {
                break;
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

bool Directory::removeDirectoryEntry(const char* fileName) {
    // Iterate over each block in the directory's inode block list.
    block_t block;
    for (uint32_t i = 0; i < inode.blockCount; i++) {
        // Read the current directory block.
        if (!read_block_data(i, block.data)) {
            std::cerr << "Failed to read directory block " << i << std::endl;
            continue;
        }
        // Search through all entries in this block.
        for (uint16_t j = 0; j < DIRECTORY_ENTRIES_PER_BLOCK; j++) {
            // Check if the entry is valid and matches the file name.
            if (block.directoryBlock.entries[j].inodeNumber != INODE_NULL_VALUE &&
                strcmp(block.directoryBlock.entries[j].name, fileName) == 0) {

                // Calculate the global index of the found entry.
                uint32_t entryGlobalIndex = i * DIRECTORY_ENTRIES_PER_BLOCK + j;
                if (entryGlobalIndex >= inode.numFiles) {
                    std::cerr << "Entry global index out of bounds" << std::endl;
                    return false;
                }

                // Log the deletion of the inode by creating a deletion log record.
                LogRecordPayload payload{};
                payload.inodeDelete.inodeIndex = block.directoryBlock.entries[j].inodeNumber;
                if (!logManager->logOperation(LogOpType::LOG_OP_INODE_DELETE, &payload)) {
                    std::cerr << "Failed to log inode deletion" << std::endl;
                    return false;
                }

                // Prepare a new copy of the directory block (copy-on-write).
                block_t newBlock;
                memcpy(newBlock.data, block.data, sizeof(block.data));

                // Determine the last valid directory entry (global index = inode.numFiles - 1).
                uint32_t lastEntryGlobalIndex = inode.numFiles - 1;
                uint32_t lastBlockIndex = lastEntryGlobalIndex / DIRECTORY_ENTRIES_PER_BLOCK;
                uint16_t lastOffset = lastEntryGlobalIndex % DIRECTORY_ENTRIES_PER_BLOCK;

                if (entryGlobalIndex != lastEntryGlobalIndex) {
                    // If the entry being deleted is not the last one, swap it with the last entry.
                    if (lastBlockIndex == i) {
                        // Both entries are in the same block.
                        newBlock.directoryBlock.entries[j] = newBlock.directoryBlock.entries[lastOffset];
                    } else {
                        // The last entry is in a different block. Read that block.
                        block_t lastBlock;
                        if (!read_block_data(lastBlockIndex, lastBlock.data)) {
                            std::cerr << "Failed to read last directory block" << std::endl;
                            return false;
                        }
                        newBlock.directoryBlock.entries[j] = lastBlock.directoryBlock.entries[lastOffset];
                        // Remove the last entry from its block copy by clearing it.
                        memset(lastBlock.directoryBlock.entries[lastOffset].name, 0, MAX_FILE_NAME_LENGTH + 1);
                        lastBlock.directoryBlock.entries[lastOffset].inodeNumber = INODE_NULL_VALUE;
                        // Perform copy-on-write update for the last block.
                        block_index_t newLastBlock = blockBitmap->findNextFree();
                        if (newLastBlock == BLOCK_NULL_VALUE) {
                            std::cerr << "No free block for copy-on-write update of last block" << std::endl;
                            return false;
                        }
                        if (!blockBitmap->setAllocated(newLastBlock)) {
                            std::cerr << "Failed to set allocated for new last block" << std::endl;
                            return false;
                        }
                        if (!blockManager->writeBlock(newLastBlock, lastBlock.data)) {
                            std::cerr << "Failed to write updated last block" << std::endl;
                            return false;
                        }
                        // Update the directory inode pointer for the last block.
                        inode.directBlocks[lastBlockIndex] = newLastBlock;
                    }
                } else {
                    // If the entry to remove is the last one, simply clear it in the new copy.
                    memset(newBlock.directoryBlock.entries[j].name, 0, MAX_FILE_NAME_LENGTH + 1);
                    newBlock.directoryBlock.entries[j].inodeNumber = INODE_NULL_VALUE;
                }

                // Decrement the total number of directory entries.
                inode.numFiles--;

                // Now, perform a copy-on-write update for the directory block where deletion occurred.
                block_index_t newBlockLocation = blockBitmap->findNextFree();
                if (newBlockLocation == BLOCK_NULL_VALUE) {
                    std::cerr << "No free block available for copy-on-write directory update" << std::endl;
                    return false;
                }
                if (!blockBitmap->setAllocated(newBlockLocation)) {
                    std::cerr << "Failed to set allocated for new directory block" << std::endl;
                    return false;
                }
                if (!blockManager->writeBlock(newBlockLocation, newBlock.data)) {
                    std::cerr << "Failed to write new directory block" << std::endl;
                    return false;
                }
                // Update the directory inode pointer to reference the new copy.
                inode.directBlocks[i] = newBlockLocation;
                // Write the updated inode back to the inode table.
                if (!inodeTable->writeInode(inodeLocation, inode)) {
                    std::cerr << "Failed to write updated inode after directory deletion" << std::endl;
                    return false;
                }
                return true;
            }
        }
    }
    // If no matching entry is found, return false.
    return false;
}

bool Directory::modifyDirectoryEntry(const char* fileName, inode_index_t fileNum)
{
    block_t block;
    for (inode_index_t i = 0; i < inode.blockCount; i++)
    {
        read_block_data(i, block.data);
        uint8_t offset = 0;
        for (auto [inodeNumber, name] : block.directoryBlock.entries)
        {
            if (strcmp(name, fileName) == 0)
            {
                block.directoryBlock.entries[offset].inodeNumber = fileNum;
                return write_block_data(i, block.data);
            }
            offset++;
        }
    }
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
    auto* newDir = new Directory(inodeTable, inodeBitmap, blockBitmap, blockManager, logManager);
    if (!addDirectoryEntry(name, newDir->getInodeNumber()))
    {
        return nullptr;
    }
    return newDir;
}

File* Directory::createFile(const char* name)
{
    auto* newFile = new File(inodeTable, inodeBitmap, blockBitmap, blockManager, logManager);
    if (!addDirectoryEntry(name, newFile->getInodeNumber()))
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
    if (file->isDirectory())
    {
        return convertFile(file);
    }
    return file;
}

Directory::Directory(const File* file) : File(file)
{
}

Directory* Directory::convertFile(File* file)
{
    if (!file->isDirectory()) return nullptr;
    auto* dir = new Directory(file);
    delete file;
    return dir;
}
