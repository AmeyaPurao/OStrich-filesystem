#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include "Superblock.h"
#include "LogRecord.h"
#include "Segment.h"
#include "BlockManager.h"  // Your BlockManager handles 4096-byte blocks.
#include <mutex>
#include <vector>

class LogManager {
public:
    // segmentSize (in bytes) must be a multiple of Superblock::BLOCK_SIZE (4096).
    // When constructed, if a valid superblock exists in block 0, recovery is performed;
    // otherwise, a new superblock is created.
    LogManager(BlockManager *blockManager, size_t segmentSize);
    ~LogManager();

    // Append a log record to the log.
    bool appendRecord(const LogRecord &record);

    // Append a checkpoint record (this is the public version that locks).
    bool appendCheckpoint(const LogRecord &checkpointRecord);

    // Flush the current segment to disk.
    bool flush();

    // Recovery: read the superblock and scan log segments to reconstruct state.
    bool recover();

private:
    BlockManager *blockManager; // Underlying block I/O interface.
    size_t segmentSize;         // In bytes.
    uint32_t currentSegmentSeq; // Highest segment number that has been written.
    size_t currentSegmentBlockStart; // Block index (relative to partition) where current segment begins.
    size_t currentOffset;       // Offset (in bytes) into the current segment's data buffer.
    Segment *currentSegment;    // The segment currently being written.
    std::mutex logMutex;        // Protects log operations.
    uint64_t globalSequence;    // Global log record sequence number.
    size_t blocksPerSegment;    // = segmentSize / Superblock::BLOCK_SIZE

    // Write the current segment's data (log records) to disk.
    bool writeCurrentSegment();

    // Start a new segment for subsequent log records.
    bool startNewSegment();

    // Internal helper to write a checkpoint (assumes caller holds the lock).
    bool doAppendCheckpoint(const LogRecord &checkpointRecord);

    // Pad a buffer to BLOCK_SIZE bytes.
    static std::vector<uint8_t> padToBlockSize(const std::vector<uint8_t>& data);
};

#endif // LOG_MANAGER_H
