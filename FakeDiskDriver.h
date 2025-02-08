#ifndef FAKE_DISK_DRIVER_H
#define FAKE_DISK_DRIVER_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

using namespace std;

class FakeDiskDriver {
public:
    // A block is fixed at 512 bytes.
    static constexpr size_t BLOCK_SIZE = 512;
    using Block = std::vector<uint8_t>;

    // Partition structure for simulation.
    struct Partition {
        size_t startBlock;  // Starting block index.
        size_t blockCount;  // Number of blocks in the partition.
        std::string type;   // Label (for example, "ext4", "FAT32", etc.)
    };

    /**
     * Constructor.
     *
     * @param diskFilename     The filename to use for the simulated disk.
     * @param numBlocks        Total number of blocks (disk size = numBlocks * BLOCK_SIZE).
     * @param simulatedLatency The artificial latency to simulate disk I/O delays (default: 10ms).
     */
    FakeDiskDriver(const std::string &diskFilename, size_t numBlocks,
                   std::chrono::milliseconds simulatedLatency = std::chrono::milliseconds(10));

    // Destructor.
    ~FakeDiskDriver();

    // Disable copy/assignment.
    FakeDiskDriver(const FakeDiskDriver&) = delete;
    FakeDiskDriver& operator=(const FakeDiskDriver&) = delete;

    /**
     * Reads the block at the given index.
     *
     * @param blockIndex  The block number to read.
     * @param block       The vector that will receive the data (will be resized to BLOCK_SIZE).
     * @return true if successful, false otherwise.
     */
    bool readBlock(size_t blockIndex, Block &block);

    /**
     * Writes the given block data at the given block index.
     *
     * @param blockIndex  The block number to write.
     * @param block       A vector containing exactly BLOCK_SIZE bytes.
     * @return true if successful, false otherwise.
     */
    bool writeBlock(size_t blockIndex, const Block &block);

    /**
     * Flushes any pending I/O operations.
     *
     * @return true on success.
     */
    bool flush();

    /**
     * Creates a partition if the specified block range is valid and does not overlap any existing partition.
     *
     * @param startBlock  The starting block index for the partition.
     * @param blockCount  The number of blocks for the partition.
     * @param type        A label for the partition.
     * @return true if the partition is created successfully.
     */
    bool createPartition(size_t startBlock, size_t blockCount, const std::string &type);

    /**
     * Deletes a partition given its index.
     *
     * @param partitionIndex  The index of the partition to delete.
     * @return true on success.
     */
    bool deletePartition(size_t partitionIndex);

    /**
     * Returns a copy of the current partition list.
     *
     * @return A vector containing all partitions.
     */
    vector<Partition> listPartitions() const;

private:
    string diskFilename;
    size_t totalBlocks;
    fstream diskFile;

    // In–memory partition table.
    vector<Partition> partitions;

    // Mutex to protect disk I/O (the file stream isn’t thread–safe).
    mutable mutex diskMutex;
    // Mutex to protect partition table modifications.
    mutable mutex partitionMutex;

    // Artificial I/O latency to simulate the SD card delay on Raspberry Pi 3.
    chrono::milliseconds ioLatency;

    // Private helper functions.
    bool openDisk();
    void closeDisk();
    bool ensureDiskSize();
};

#endif // FAKE_DISK_DRIVER_H
