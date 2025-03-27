#include "BlockManager.h"
#include "cstdio"

BlockManager::BlockManager(CurrentDiskDriver& disk, const CurrentDiskDriver::Partition& partition)
    : disk(disk), partition(partition)
{
    // Calculate the number of sectors per block.
    sectorsPerBlock = BLOCK_SIZE / CurrentDiskDriver::SECTOR_SIZE;
    if (BLOCK_SIZE % CurrentDiskDriver::SECTOR_SIZE != 0)
    {
        printf("Error: BLOCK_SIZE (%zu) is not a multiple of the sector size (%zu).\n",
            BLOCK_SIZE, CurrentDiskDriver::SECTOR_SIZE);
    }
}

bool BlockManager::readBlock(const size_t blockIndex, uint8_t* buffer)
{
    // std::cout << "\tReading block " << blockIndex << "\n";
    std::lock_guard<std::mutex> lock(blockMutex);
    size_t startSector = partition.startSector + blockIndex * sectorsPerBlock;
    if (blockIndex * sectorsPerBlock >= partition.sectorCount)
    {
        printf("readBlock: block index %zu is out of partition range.\n", blockIndex);
        return false;
    }

    // block.resize(BLOCK_SIZE);
    for (size_t i = 0; i < sectorsPerBlock; i++)
    {
        if (!disk.readSector(startSector + i, buffer + i * CurrentDiskDriver::SECTOR_SIZE))
        {
            printf("readBlock: failed to read sector %zu.\n", (startSector + i));
            return false;
        }
    }
    return true;
}

bool BlockManager::writeBlock(const size_t blockIndex, const uint8_t* buffer)
{
    // std::cout << "\tWriting block " << blockIndex << "\n";
    std::lock_guard<std::mutex> lock(blockMutex);
    // if (block.size() != BLOCK_SIZE) {
    //     std::cerr << "writeBlock: block size mismatch (expected " << BLOCK_SIZE << " bytes).\n";
    //     return false;
    // }

    size_t startSector = partition.startSector + blockIndex * sectorsPerBlock;
    if (blockIndex * sectorsPerBlock >= partition.sectorCount)
    {
        printf("writeBlock: block index %zu is out of partition range.\n", blockIndex);
        return false;
    }

    for (size_t i = 0; i < sectorsPerBlock; i++)
    {
        if (!disk.writeSector(startSector + i, buffer + i * CurrentDiskDriver::SECTOR_SIZE))
        {
            printf("writeBlock: failed to write sector %zu.\n", (startSector + i));
            return false;
        }
    }
    return true;
}
