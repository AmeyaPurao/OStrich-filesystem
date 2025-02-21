//
// Created by ameya on 2/19/2025.
//

#ifndef BLOCK_H
#define BLOCK_H
#include <cstdint>

#define MAGIC_NUMBER 0xCA5CADEDBA5EBA11
using block_index_t = uint32_t;
using inode_index_t = uint32_t;

typedef struct inode
{
    uint64_t size;
    block_index_t blockCount;
    uint16_t uid;
    uint16_t gid;
    uint16_t permissions;
    block_index_t directBlocks[15];
    block_index_t indirectBlocks[10];
    block_index_t doubleIndirectBlocks[2];
} inode_t;

typedef struct directBlock
{
    block_index_t blockNumbers[1024];
} directBlock_t;

typedef struct logRecord
{
    inode_index_t inodeNumber;
    block_index_t inodeBlock;
} logRecord_t;

typedef struct logEntry
{
    logRecord_t records[512];
} logEntry_t;

typedef struct superBlock
{
    uint64_t magic;
    uint64_t size;
    block_index_t totalBlockCount;
    block_index_t dataBlockCount;
    inode_index_t inodeCount;
    block_index_t freeDataBlockCount;
    inode_index_t freeInodeCount;
    block_index_t dataBlockBitmap;
    block_index_t dataBlockBitmapSize;
    block_index_t inodeBitmap;
    inode_index_t inodeBitmapSize;
    block_index_t inodeTable;
    inode_index_t inodeTableSize;
    block_index_t dataBlockRegionStart;
    block_index_t inodeRegionStart;
    inode_index_t inodeRegionSize;
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
