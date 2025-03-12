//
// Created by ameya on 2/20/2025.
//

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "BitmapManager.h"
#include "Block.h"
#include "Directory.h"
#include "InodeTable.h"
#include "../interface/BlockManager.h"


class FileSystem {
public:
    explicit FileSystem(BlockManager *blockManager);
    Directory* getRootDirectory() const;
    // Directory* createDirectory();
    // File* createFile();
    // File* loadFile(inode_index_t inodeNumber);

private:
    BlockManager *blockManager;
    BitmapManager *inodeBitmap;
    BitmapManager *blockBitmap;
    InodeTable *inodeTable;

    block_t superBlockWrapper{};
    superBlock_t* superBlock;

    void createFilesystem();
    void loadFilesystem();
    bool readInode(inode_index_t inodeLocation, inode_t& inode);
    bool writeInode(inode_index_t inodeLocation, inode_t& inode);
    Directory* createRootInode();
};



#endif //FILESYSTEM_H
