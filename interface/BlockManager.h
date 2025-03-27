#ifndef BLOCK_MANAGER_H
#define BLOCK_MANAGER_H

#ifdef ON_LINUX
#include "FakeDiskDriver.h"
#else
#include "RealDiskDriver.h"
#endif
#include "vector"
#include "cstdint"
#include "mutex"
// #include "algorithm"

// Doing this instead of an AbstractClass so we don't have to compile the FakeDiskDriver.
// I didn't wanna bother with makefile stuff bc its annoying so im doing it this way -Arvind.
#ifdef ON_LINUX
using CurrentDiskDriver = FakeDiskDriver;
#else
using CurrentDiskDriver = RealDiskDriver;
#endif


class BlockManager
{
public:
    // The file system block size is 4096 bytes.
    static constexpr size_t BLOCK_SIZE = 4096;
    // using BlockBuffer = uint8_t[BLOCK_SIZE];

    /**
     * Constructor.
     * @param disk       Reference to the underlying FakeDiskDriver.
     * @param partition  The partition (a slice of the disk) that this BlockManager will manage.
     */
    BlockManager(CurrentDiskDriver& disk, const CurrentDiskDriver::Partition& partition);

    /**
     * Reads a file system block (4096 bytes) from the partition.
     * @param blockIndex Logical block index (0-based within the partition).
     * @param buffer      Output buffer (will be resized to BLOCK_SIZE).
     * @return true if the block was read successfully.
     */
    bool readBlock(size_t blockIndex, uint8_t* buffer);

    /**
     * Writes a file system block (4096 bytes) to the partition.
     * @param blockIndex Logical block index (0-based within the partition).
     * @param buffer      Input buffer of size BLOCK_SIZE.
     * @return true if the block was written successfully.
     */
    bool writeBlock(size_t blockIndex, const uint8_t* buffer);

    uint32_t getNumBlocks() const
    {
        return partition.sectorCount / sectorsPerBlock;
    }

private:
    CurrentDiskDriver& disk;
    CurrentDiskDriver::Partition partition;
    size_t sectorsPerBlock; // Number of 512-byte sectors per 4096-byte block.
    mutable std::mutex blockMutex; // Protects BlockManager state and operations.
};

#endif // BLOCK_MANAGER_H
