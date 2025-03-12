//
// Created by ameya on 2/19/2025.
//

#ifndef BLOCK_H
#define BLOCK_H
#include <cstdint>
#include "../interface/BlockManager.h"


using block_index_t = uint32_t;
using inode_index_t = uint32_t;

constexpr uint64_t MAGIC_NUMBER = 0xCA5CADEDBA5EBA11;
constexpr block_index_t BLOCK_NULL_VALUE = UINT32_MAX;
constexpr inode_index_t INODE_NULL_VALUE = UINT32_MAX;

constexpr uint16_t NUM_DIRECT_BLOCKS = 15;
constexpr uint16_t NUM_INDIRECT_BLOCKS = 10;
constexpr uint16_t NUM_DOUBLE_INDIRECT_BLOCKS = 2;

constexpr uint16_t DIRECTORY_MASK = 1 << 9;

typedef struct inode
{
    uint64_t size;
    block_index_t blockCount;
    uint16_t uid;
    uint16_t gid;
    uint16_t permissions;
    uint16_t numFiles;
    block_index_t directBlocks[NUM_DIRECT_BLOCKS];
    block_index_t indirectBlocks[NUM_INDIRECT_BLOCKS];
    block_index_t doubleIndirectBlocks[NUM_DOUBLE_INDIRECT_BLOCKS];
} inode_t;

constexpr uint16_t INODES_PER_BLOCK = BlockManager::BLOCK_SIZE / sizeof(inode_t);

typedef struct inodeBlock
{
    inode_t inodes[INODES_PER_BLOCK];
} inodeBlock_t;

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

typedef struct bitmapBlock
{
    uint64_t parts[512];
} bitmapBlock_t;

constexpr uint16_t TABLE_ENTRIES_PER_BLOCK = BlockManager::BLOCK_SIZE / sizeof(inode_index_t);

typedef struct inodeTableBlock
{
    inode_index_t inodeNumbers[TABLE_ENTRIES_PER_BLOCK];
} inodeTableBlock_t;

constexpr uint16_t MAX_FILE_NAME_LENGTH = 251;

typedef struct dirEntry
{
    inode_index_t inodeNumber;
    char name[MAX_FILE_NAME_LENGTH+1];
} dirEntry_t;

constexpr uint16_t DIRECTORY_ENTRIES_PER_BLOCK = BlockManager::BLOCK_SIZE / sizeof(dirEntry_t);

typedef struct directoryBlock
{
    dirEntry_t entries[DIRECTORY_ENTRIES_PER_BLOCK];
} directoryBlock_t;

typedef union block
{
    uint8_t data[4096];
    inodeBlock_t inodeBlock;
    directBlock_t directBlock;
    logEntry_t logEntry;
    superBlock_t superBlock;
    bitmapBlock_t bitmapBlock;
    inodeTableBlock_t inodeTable;
    directoryBlock_t directoryBlock;
} block_t;

#endif //BLOCK_H
