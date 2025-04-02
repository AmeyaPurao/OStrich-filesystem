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
#include "LogManager.h"


class FileSystem {
public:
    explicit FileSystem(BlockManager *blockManager);
    Directory* getRootDirectory() const;
    bool createCheckpoint();

    // Mount a read-only snapshot based on a checkpoint ID.
    // Returns a new FileSystem instance representing the snapshot, or nullptr on failure.
    FileSystem* mountReadOnlySnapshot(uint32_t checkpointID);


private:
    bool readOnly = false; // default false

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
    Directory* createRootInode();
};



#endif //FILESYSTEM_H
