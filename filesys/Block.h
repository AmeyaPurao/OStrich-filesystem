//
// Created by ameya on 2/19/2025.
//

#ifndef BLOCK_H
#define BLOCK_H
#include <cstdint>

#define MAGIC_NUMBER 0xCA5CADEDBA5EBA11

typedef struct inode
{
    uint64_t size;
    uint32_t blockCount;
    uint16_t uid;
    uint16_t gid;
    uint16_t permissions;
    uint32_t directBlocks[15];
    uint32_t indirectBlocks[10];
    uint32_t doubleIndirectBlocks[2];
} inode_t;

typedef struct directBlock
{
    uint32_t blockNumbers[1024];
} directBlock_t;

typedef struct logRecord
{
    uint32_t inodeNumber;
    uint32_t inodeBlock;
} logRecord_t;

typedef struct logEntry
{
    logRecord_t records[512];
} logEntry_t;

typedef struct superBlock
{
    uint64_t magic;
    uint64_t size;
    uint32_t totalBlockCount;
    uint32_t dataBlockCount;
    uint32_t inodeCount;
    uint32_t freeDataBlockCount;
    uint32_t freeInodeCount;
    uint32_t dataBlockBitmap;
    uint32_t dataBlockBitmapSize;
    uint32_t inodeBitmap;
    uint32_t inodeBitmapSize;
    uint32_t inodeTable;
    uint32_t inodeTableSize;
    uint32_t dataBlockRegionStart;
    uint32_t inodeRegionStart;
    uint32_t inodeRegionSize;
} superBlock_t;

typedef union block
{
    uint8_t data[4096];
    inode_t inode;
    directBlock_t directBlock;
    logEntry_t logEntry;
    superBlock_t superBlock;
} block_t;

#endif //BLOCK_H
