#include "FakeDiskDriver.h"
#include <iostream>
#include <thread>

using namespace std;

// Constructor: Opens (or creates) the file and ensures its size.
FakeDiskDriver::FakeDiskDriver(const string &filename, size_t numBlocks,
                               chrono::milliseconds simulatedLatency)
        : diskFilename(filename), totalBlocks(numBlocks), ioLatency(simulatedLatency)
{
    if (!openDisk()) {
        cerr << "Error: Could not open disk file " << diskFilename << "\n";
    } else if (!ensureDiskSize()) {
        cerr << "Error: Could not ensure disk file size for " << diskFilename << "\n";
    }
}

// Destructor: Flushes and closes the file.
FakeDiskDriver::~FakeDiskDriver() {
    flush();
    closeDisk();
}

// openDisk: Opens the file in binary read/write mode, creating it if necessary.
bool FakeDiskDriver::openDisk() {
    lock_guard<mutex> lock(diskMutex);
    diskFile.open(diskFilename, ios::in | ios::out | ios::binary);
    if (!diskFile.is_open()) {
        // File does not exist; create it.
        diskFile.clear();
        diskFile.open(diskFilename, ios::out | ios::binary);
        if (!diskFile.is_open()) {
            cerr << "Error: Failed to create disk file " << diskFilename << "\n";
            return false;
        }
        diskFile.close();
        // Reopen in read/write mode.
        diskFile.open(diskFilename, ios::in | ios::out | ios::binary);
        if (!diskFile.is_open()) {
            cerr << "Error: Failed to reopen disk file " << diskFilename << "\n";
            return false;
        }
    }
    return true;
}

// closeDisk: Closes the file.
void FakeDiskDriver::closeDisk() {
    lock_guard<mutex> lock(diskMutex);
    if (diskFile.is_open()) {
        diskFile.close();
    }
}

// ensureDiskSize: Makes sure the file is large enough to hold all blocks.
bool FakeDiskDriver::ensureDiskSize() {
    lock_guard<mutex> lock(diskMutex);
    diskFile.clear();
    diskFile.seekg(0, ios::end);
    streampos currentSize = diskFile.tellg();
    size_t requiredSize = totalBlocks * BLOCK_SIZE;
    if (static_cast<size_t>(currentSize) < requiredSize) {
        // Expand the file to the required size.
        diskFile.clear();
        diskFile.seekp(requiredSize - 1, ios::beg);
        char zero = 0;
        if (!diskFile.write(&zero, 1)) {
            cerr << "Error: ensureDiskSize: failed to expand disk file\n";
            return false;
        }
        if (!diskFile.flush()) {
            cerr << "Error: ensureDiskSize: failed to flush disk file\n";
            return false;
        }
    }
    return true;
}

// readBlock: Reads a block from the simulated disk.
bool FakeDiskDriver::readBlock(size_t blockIndex, Block &block) {
    if (blockIndex >= totalBlocks) {
        cerr << "Error: readBlock: block index " << blockIndex << " out of range\n";
        return false;
    }
    lock_guard<mutex> lock(diskMutex);

    // Simulate I/O latency (e.g., SD card delay on the Pi 3).
    this_thread::sleep_for(ioLatency);

    block.resize(BLOCK_SIZE);
    diskFile.clear();
    diskFile.seekg(blockIndex * BLOCK_SIZE, ios::beg);
    if (!diskFile.read(reinterpret_cast<char*>(block.data()), BLOCK_SIZE)) {
        cerr << "Error: readBlock: failed to read block " << blockIndex << "\n";
        return false;
    }
    return true;
}

// writeBlock: Writes a block to the simulated disk.
bool FakeDiskDriver::writeBlock(size_t blockIndex, const Block &block) {
    if (blockIndex >= totalBlocks) {
        cerr << "Error: writeBlock: block index " << blockIndex << " out of range\n";
        return false;
    }
    if (block.size() != BLOCK_SIZE) {
        cerr << "Error: writeBlock: block size mismatch (expected " << BLOCK_SIZE << " bytes)\n";
        return false;
    }

    lock_guard<mutex> lock(diskMutex);

    // Simulate I/O latency (e.g., SD card delay on the Pi 3).
    this_thread::sleep_for(ioLatency);

    diskFile.clear();
    diskFile.seekp(blockIndex * BLOCK_SIZE, ios::beg);
    if (!diskFile.write(reinterpret_cast<const char*>(block.data()), BLOCK_SIZE)) {
        cerr << "Error: writeBlock: failed to write block " << blockIndex << "\n";
        return false;
    }
    // For performance, we don’t flush after every block (unless required).
    return true;
}

// flush: Flushes the file buffers.
bool FakeDiskDriver::flush() {
    lock_guard<mutex> lock(diskMutex);
    if (diskFile.is_open()) {
        // Simulate a flush latency.
        this_thread::sleep_for(ioLatency);
        diskFile.flush();
        return true;
    }
    return false;
}

// createPartition: Creates a partition if the block range is valid and non–overlapping.
bool FakeDiskDriver::createPartition(size_t startBlock, size_t blockCount, const string &type) {
    if (startBlock + blockCount > totalBlocks) {
        cerr << "Error: createPartition: partition exceeds disk size\n";
        return false;
    }
    lock_guard<mutex> lock(partitionMutex);
    // Check for overlaps.
    for (const auto &p : partitions) {
        size_t pEnd = p.startBlock + p.blockCount;
        size_t newEnd = startBlock + blockCount;
        if (!(newEnd <= p.startBlock || startBlock >= pEnd)) {
            cerr << "Error: createPartition: partition overlaps an existing partition\n";
            return false;
        }
    }
    partitions.push_back({startBlock, blockCount, type});
    return true;
}

// deletePartition: Deletes the partition specified by its index.
bool FakeDiskDriver::deletePartition(size_t partitionIndex) {
    lock_guard<mutex> lock(partitionMutex);
    if (partitionIndex >= partitions.size()) {
        cerr << "Error: deletePartition: partition index out of range\n";
        return false;
    }
    partitions.erase(partitions.begin() + partitionIndex);
    return true;
}

// listPartitions: Returns the current list of partitions.
vector<FakeDiskDriver::Partition> FakeDiskDriver::listPartitions() const {
    lock_guard<mutex> lock(partitionMutex);
    return partitions;
}
