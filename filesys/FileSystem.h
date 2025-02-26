//
// Created by ameya on 2/20/2025.
//

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "BitmapManager.h"
#include "Block.h"
#include "InodeTable.h"
#include "../interface/BlockManager.h"
#include "LogManager.h"


class FileSystem {
public:
    explicit FileSystem(BlockManager *blockManager);
    inode_index_t createDirectory(inode_index_t baseDirectory, const char* name);

private:
    BlockManager *blockManager;
    LogManager* logManager;
    BitmapManager *inodeBitmap;
    BitmapManager *blockBitmap;
    InodeTable *inodeTable;

    block_t superBlockWrapper{};
    superBlock_t* superBlock;

    void createFilesystem();
    void loadFilesystem();
    bool readInode(inode_index_t inodeLocation, inode_t& inode);
    bool writeInode(inode_index_t inodeLocation, inode_t& inode);
    bool addDirectoryEntry(inode_index_t baseDirectory, const char* fileName, inode_index_t fileNum);
};



#endif //FILESYSTEM_H
