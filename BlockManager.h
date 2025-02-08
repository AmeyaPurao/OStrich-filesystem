#ifndef BLOCK_MANAGER_H
#define BLOCK_MANAGER_H

#include "FakeDiskDriver.h"
#include <vector>
#include <cstdint>
#include <mutex>
#include <iostream>
#include <algorithm>

class BlockManager {
public:
    // The file system block size is 4096 bytes.
    static constexpr size_t BLOCK_SIZE = 4096;
    using Block = std::vector<uint8_t>;

    /**
     * Constructor.
     * @param disk       Reference to the underlying FakeDiskDriver.
     * @param partition  The partition (a slice of the disk) that this BlockManager will manage.
     */
    BlockManager(FakeDiskDriver &disk, const FakeDiskDriver::Partition &partition);

    /**
     * Reads a file system block (4096 bytes) from the partition.
     * @param blockIndex Logical block index (0-based within the partition).
     * @param block      Output buffer (will be resized to BLOCK_SIZE).
     * @return true if the block was read successfully.
     */
    bool readBlock(size_t blockIndex, Block &block);

    /**
     * Writes a file system block (4096 bytes) to the partition.
     * @param blockIndex Logical block index (0-based within the partition).
     * @param block      Input buffer of size BLOCK_SIZE.
     * @return true if the block was written successfully.
     */
    bool writeBlock(size_t blockIndex, const Block &block);

private:
    FakeDiskDriver &disk;
    FakeDiskDriver::Partition partition;
    size_t sectorsPerBlock;  // Number of 512-byte sectors per 4096-byte block.
    mutable std::mutex blockMutex;  // Protects BlockManager state and operations.
};

#endif // BLOCK_MANAGER_H
