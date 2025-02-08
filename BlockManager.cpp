#include "BlockManager.h"

BlockManager::BlockManager(FakeDiskDriver &disk, const FakeDiskDriver::Partition &partition)
        : disk(disk), partition(partition)
{
    // Calculate the number of sectors per block.
    sectorsPerBlock = BLOCK_SIZE / FakeDiskDriver::SECTOR_SIZE;
    if (BLOCK_SIZE % FakeDiskDriver::SECTOR_SIZE != 0) {
        std::cerr << "Error: BLOCK_SIZE (" << BLOCK_SIZE
                  << ") is not a multiple of the sector size ("
                  << FakeDiskDriver::SECTOR_SIZE << ").\n";
    }
}

bool BlockManager::readBlock(size_t blockIndex, Block &block) {
    // Lock the BlockManager while reading.
    std::lock_guard<std::mutex> lock(blockMutex);

    // Calculate the physical starting sector within the disk:
    size_t startSector = partition.startSector + blockIndex * sectorsPerBlock;

    // Verify that we do not exceed the partition's boundaries.
    if (blockIndex * sectorsPerBlock >= partition.sectorCount) {
        std::cerr << "readBlock: block index " << blockIndex
                  << " is out of partition range.\n";
        return false;
    }

    block.resize(BLOCK_SIZE);
    for (size_t i = 0; i < sectorsPerBlock; i++) {
        FakeDiskDriver::Sector sectorData;
        if (!disk.readSector(startSector + i, sectorData)) {
            std::cerr << "readBlock: failed to read sector "
                      << (startSector + i) << "\n";
            return false;
        }
        // Copy sector data into the correct position in the block.
        std::copy(sectorData.begin(), sectorData.end(),
                  block.begin() + i * FakeDiskDriver::SECTOR_SIZE);
    }
    return true;
}

bool BlockManager::writeBlock(size_t blockIndex, const Block &block) {
    // Lock the BlockManager while writing.
    std::lock_guard<std::mutex> lock(blockMutex);

    if (block.size() != BLOCK_SIZE) {
        std::cerr << "writeBlock: block size mismatch (expected "
                  << BLOCK_SIZE << " bytes).\n";
        return false;
    }

    size_t startSector = partition.startSector + blockIndex * sectorsPerBlock;
    if (blockIndex * sectorsPerBlock >= partition.sectorCount) {
        std::cerr << "writeBlock: block index " << blockIndex
                  << " is out of partition range.\n";
        return false;
    }

    for (size_t i = 0; i < sectorsPerBlock; i++) {
        // Extract the corresponding 512-byte sector from the block.
        FakeDiskDriver::Sector sectorData(
                block.begin() + i * FakeDiskDriver::SECTOR_SIZE,
                block.begin() + (i + 1) * FakeDiskDriver::SECTOR_SIZE
        );
        if (!disk.writeSector(startSector + i, sectorData)) {
            std::cerr << "writeBlock: failed to write sector "
                      << (startSector + i) << "\n";
            return false;
        }
    }
    return true;
}
