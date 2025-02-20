//
// Created by ameya on 2/20/2025.
//

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include "Block.h"
#include "../interface/BlockManager.h"


class FileSystem {
public:
    explicit FileSystem(BlockManager *blockManager);

private:
    BlockManager *blockManager;
    block_t superBlockWrapper{};
    superBlock_t* superBlock;

    void createFilesystem();
    void loadFilesystem();
    uint32_t findNextFreeInode();
};



#endif //FILESYSTEM_H
